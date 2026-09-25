#include "backend/backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double profile_frames_ms[120];
static Uint32 profile_count;
static Uint32 profile_warmup;
static double profile_acquire_ms, profile_collect_ms, profile_cull_ms, profile_encode_ms;
static Uint32 profile_draw_calls, profile_drawn_instances;

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left, b = *(const double *)right;
    return (a > b) - (a < b);
}

static void profile_frame(void) {
    if (!SIGPU_RENDERSTATS->profile_enabled || profile_warmup++ < 60)
        return;
    profile_frames_ms[profile_count++] =
        (SDL_GetTicksNS() - SIGPU_RENDERSTATS->frame_start_ns) / 1000000.0;
    profile_acquire_ms += SIGPU_RENDERSTATS->acquire_ns / 1000000.0;
    profile_collect_ms += SIGPU_RENDERSTATS->collect_ns / 1000000.0;
    profile_cull_ms += SIGPU_RENDERSTATS->cull_ns / 1000000.0;
    profile_encode_ms += SIGPU_RENDERSTATS->encode_ns / 1000000.0;
    profile_draw_calls += SIGPU_RENDERSTATS->draw_calls;
    profile_drawn_instances += SIGPU_RENDERSTATS->drawn_instances;
    if (profile_count != SDL_arraysize(profile_frames_ms))
        return;
    qsort(profile_frames_ms, profile_count, sizeof(profile_frames_ms[0]), compare_double);
    fprintf(
        stderr,
        "sigpu %ux%u frame p50=%.2f p95=%.2f ms acquire=%.2f collect=%.2f cull=%.2f encode=%.2f "
        "draw=%u instances=%u chunks=%u/%u\n",
        SIGPU_FRAMECONTEXT->frame_width,
        SIGPU_FRAMECONTEXT->frame_height,
        profile_frames_ms[profile_count / 2],
        profile_frames_ms[(profile_count * 95) / 100],
        profile_acquire_ms / profile_count,
        profile_collect_ms / profile_count,
        profile_cull_ms / profile_count,
        profile_encode_ms / profile_count,
        profile_draw_calls / profile_count,
        profile_drawn_instances / profile_count,
        SIGPU_STATICRENDERCACHE->static_camera_visible_count,
        SIGPU_STATICRENDERCACHE->static_chunk_count
    );
    profile_count = 0;
    profile_acquire_ms = profile_collect_ms = profile_cull_ms = profile_encode_ms = 0.0;
    profile_draw_calls = profile_drawn_instances = 0;
}

void sigpu_init(const char *title, int width, int height, int samples) {
    const char *profile = getenv("SIGPU_PROFILE");
    SIGPU_RENDERSTATS->profile_enabled = profile && profile[0] && profile[0] != '0';
    profile_count = profile_warmup = 0;
    SDL_Init(SDL_INIT_VIDEO);
    SIGPU_GPUCONTEXT->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    SIGPU_GPUCONTEXT->window = SDL_CreateWindow(
        title,
        width,
        height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    SDL_ClaimWindowForGPUDevice(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->window);
    SIGPU_GPUCONTEXT->linear_swapchain = SDL_WindowSupportsGPUSwapchainComposition(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUCONTEXT->window,
        SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR
    );
    SDL_SetGPUSwapchainParameters(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUCONTEXT->window,
        SIGPU_GPUCONTEXT->linear_swapchain ? SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR
                                           : SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
        SDL_GPU_PRESENTMODE_VSYNC
    );
    SIGPU_GPUCONTEXT->swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->window);
    sigpu_resources_create(samples);
}

bool sigpu_begin_frame(void) {
    SIGPU_FRAMECONTEXT->command_buffer = SDL_AcquireGPUCommandBuffer(SIGPU_GPUCONTEXT->device);
    SIGPU_FRAMECONTEXT->swapchain = NULL;
    Uint64 acquire_start = SIGPU_RENDERSTATS->profile_enabled ? SDL_GetTicksNS() : 0;
    SDL_WaitAndAcquireGPUSwapchainTexture(
        SIGPU_FRAMECONTEXT->command_buffer,
        SIGPU_GPUCONTEXT->window,
        &SIGPU_FRAMECONTEXT->swapchain,
        &SIGPU_FRAMECONTEXT->frame_width,
        &SIGPU_FRAMECONTEXT->frame_height
    );
    if (SIGPU_RENDERSTATS->profile_enabled)
        SIGPU_RENDERSTATS->acquire_ns = SDL_GetTicksNS() - acquire_start;
    SIGPU_RENDERQUEUE->axis_mapped =
        SDL_MapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->axis_transfer, true);
    SIGPU_RENDERQUEUE->rotated_mapped = SDL_MapGPUTransferBuffer(
        SIGPU_GPUCONTEXT->device,
        SIGPU_GPUCONTEXT->rotated_transfer,
        true
    );
    return true;
}

void sigpu_end_frame(void) {
    if (!SIGPU_FRAMECONTEXT->swapchain) {
        SDL_SubmitGPUCommandBuffer(SIGPU_FRAMECONTEXT->command_buffer);
        SIGPU_FRAMECONTEXT->command_buffer = NULL;
        return;
    }

    SDL_SubmitGPUCommandBuffer(SIGPU_FRAMECONTEXT->command_buffer);
    profile_frame();
    SIGPU_FRAMECONTEXT->command_buffer = NULL;
    SIGPU_FRAMECONTEXT->swapchain = NULL;
}

void sigpu_fini(void) {
    SDL_WaitForGPUIdle(SIGPU_GPUCONTEXT->device);
    sigpu_resources_destroy();
    SDL_ReleaseWindowFromGPUDevice(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->window);
    SDL_DestroyWindow(SIGPU_GPUCONTEXT->window);
    SDL_DestroyGPUDevice(SIGPU_GPUCONTEXT->device);
    SDL_Quit();
}

bool sigpu_set_fullscreen(bool enabled) {
    return SDL_SetWindowFullscreen(SIGPU_GPUCONTEXT->window, enabled);
}
