#include "util.h"
#include <stdlib.h>

r3d_test_t r3d_test_start(int32_t width, int32_t height, int32_t samples) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    ecs_init();
    ECS_MODULE_IMPORT(siwindow, { .keep_open_on_close = true });
    ECS_MODULE_IMPORT(sigpu, { 0 });
    ECS_MODULE_IMPORT(sirender3d, { 0 });
    r3d_test_t t = { 0 };
    t.window = ecs_new();
    ecs_set(t.window, Window, { .slot = "main", .title = "render3d", .width = width,
                                .height = height });
    ecs_set(ecs_new(), GpuDevice, { .debug = true });
    t.surface = ecs_new();
    ecs_set(t.surface, GpuSurface, { .window_slot = "main" });
    ecs_set(ecs_new(), DirectionalLight, { .x = 0, .y = -1, .z = 0.001f,
                                           .color = { 255, 255, 255, 255 }, .intensity = 1 });
    t.camera = ecs_new();
    ecs_set(t.camera, Position3d, { 0, 0, 10 });
    ecs_set(t.camera, Camera3d, { .fov = 60, .near_plane = 0.1f, .far_plane = 100 });
    ecs_set(t.camera, Multisampling, { samples });
    ecs_set(t.camera, Sky, { { 0, 0, 255, 255 } });
    ecs_set(t.camera, BloomSettings, { .enabled = false });
    ecs_relate(t.camera, RendersTo, t.surface);
    return t;
}

void r3d_test_stop(void) {
    ecs_fini();
    SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
}

void r3d_test_frames(uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        ecs_progress();
    }
}

static bool read_texture(SDL_GPUTexture *texture, uint32_t width, uint32_t height, void *out) {
    SDL_GPUDevice *gpu = gpu_session_device();
    const uint32_t size = width * height * 4;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        gpu, &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                                 .size = size });
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(copy, &(SDL_GPUTextureRegion){ .texture = texture, .w = width,
                                                              .h = height, .d = 1 },
                               &(SDL_GPUTextureTransferInfo){ .transfer_buffer = transfer });
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(gpu, true, &fence, 1);
    SDL_ReleaseGPUFence(gpu, fence);
    const void *mapped = SDL_MapGPUTransferBuffer(gpu, transfer, false);
    memcpy(out, mapped, size);
    SDL_UnmapGPUTransferBuffer(gpu, transfer);
    SDL_ReleaseGPUTransferBuffer(gpu, transfer);
    return true;
}

uint32_t r3d_test_pixel(const r3d_test_t *t, float u, float v) {
    const GpuSurfaceTargets *targets = ecs_try_get(t->surface, GpuSurfaceTargets);
    gpu_texture_info_t info;
    SDL_GPUTexture *texture = targets ? gpu_resource_texture(targets->color, &info) : NULL;
    if (!texture) {
        return 0xdeadbeef;
    }
    uint32_t *pixels = malloc((size_t)info.width * info.height * 4);
    read_texture(texture, info.width, info.height, pixels);
    const uint32_t x = (uint32_t)(u * (float)(info.width - 1));
    const uint32_t y = (uint32_t)(v * (float)(info.height - 1));
    const uint32_t pixel = pixels[y * info.width + x];
    free(pixels);
    return pixel;
}

uint8_t r3d_channel(uint32_t pixel, uint32_t channel) {
    return (uint8_t)(pixel >> (channel * 8));
}

const GpuStats *r3d_test_stats(void) { return ecs_get_resource_read(GpuStats); }
