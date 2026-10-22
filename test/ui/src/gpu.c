#include "sigpu_internal.h"
#include <siui.h>
#include <siwindow.h>
#include <stdlib.h>
#include <test.h>

static ecs_entity_t surface;

static void start(void) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    ecs_init();
    ECS_MODULE_IMPORT(siwindow, { .keep_open_on_close = true });
    ECS_MODULE_IMPORT(sigpu, { 0 });
    ECS_MODULE_IMPORT(siui, { .gpu = true });
    ecs_set(ecs_new(), Window, { .slot = "main", .title = "ui", .width = 64, .height = 64 });
    ecs_set(ecs_new(), GpuDevice, { .debug = true });
    surface = ecs_new();
    ecs_set(surface, GpuSurface, { .window_slot = "main", .clear = { 0, 0, 0, 1 } });
}

static void stop(void) {
    ecs_fini();
    SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
}

static void frames(uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        ecs_progress();
    }
}

static uint32_t *read_color(uint32_t *width, uint32_t *height) {
    const GpuSurfaceTargets *targets = ecs_get(surface, GpuSurfaceTargets);
    gpu_texture_info_t info;
    SDL_GPUTexture *texture = gpu_resource_texture(targets->color, &info);
    if (!texture) {
        return NULL;
    }
    SDL_GPUDevice *gpu = gpu_session_device();
    const uint32_t size = info.width * info.height * 4;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        gpu, &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                                 .size = size });
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(copy, &(SDL_GPUTextureRegion){ .texture = texture,
                                                              .w = info.width, .h = info.height,
                                                              .d = 1 },
                               &(SDL_GPUTextureTransferInfo){ .transfer_buffer = transfer });
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(gpu, true, &fence, 1);
    SDL_ReleaseGPUFence(gpu, fence);
    uint32_t *pixels = malloc(size);
    memcpy(pixels, SDL_MapGPUTransferBuffer(gpu, transfer, false), size);
    SDL_UnmapGPUTransferBuffer(gpu, transfer);
    SDL_ReleaseGPUTransferBuffer(gpu, transfer);
    *width = info.width;
    *height = info.height;
    return pixels;
}

static uint32_t pixel(uint32_t x, uint32_t y) {
    uint32_t width, height;
    uint32_t *pixels = read_color(&width, &height);
    const uint32_t value = pixels ? pixels[y * width + x] : 0xdeadbeef;
    free(pixels);
    return value;
}

static ecs_entity_t node(ecs_entity_t parent, float x, float y, float w, float h,
                         siui_color_t color) {
    ecs_entity_t entity = ecs_new();
    ecs_set(entity, UiNode, { .position = SIUI_ABSOLUTE, .left = siui_px(x), .top = siui_px(y),
                              .width = siui_px(w), .height = siui_px(h) });
    ecs_set(entity, UiPaint, { .background = color });
    if (parent) {
        ecs_relate(entity, ChildOf, parent);
    }
    return entity;
}

void gpu_background_drawn(void) {
    start();
    node(0, 0, 0, 64, 64, (siui_color_t){ 255, 0, 0, 255 });
    frames(4);
    test_uint(0xff0000ffu, pixel(10, 10));
    stop();
}

void gpu_children_over_parents(void) {
    start();
    ecs_entity_t root = node(0, 0, 0, 64, 64, (siui_color_t){ 255, 0, 0, 255 });
    node(root, 16, 16, 32, 32, (siui_color_t){ 0, 255, 0, 255 });
    frames(4);
    test_uint(0xff0000ffu, pixel(4, 4));
    test_uint(0xff00ff00u, pixel(32, 32));
    stop();
}

void gpu_text_drawn(void) {
    start();
    ecs_entity_t root = node(0, 0, 0, 64, 64, (siui_color_t){ 0, 0, 0, 255 });
    ecs_set(root, UiText, { .text = "HHHH", .font_size = 24, .color = { 255, 255, 255, 255 } });
    frames(4);
    uint32_t width, height, bright = 0;
    uint32_t *pixels = read_color(&width, &height);
    test_assert(pixels != NULL);
    for (uint32_t i = 0; i < width * height; i++) {
        bright += (pixels[i] & 0xff) > 128;
    }
    free(pixels);
    test_assert(bright > 20);
    stop();
}

void gpu_still_ui_uploads_nothing(void) {
    start();
    ecs_entity_t root = node(0, 0, 0, 64, 64, (siui_color_t){ 0, 0, 0, 255 });
    ecs_set(root, UiText, { .text = "still", .font_size = 16, .color = { 255, 255, 255, 255 } });
    frames(8);
    for (uint32_t i = 0; i < 4; i++) {
        frames(1);
        const GpuStats *stats = ecs_get_resource_read(GpuStats);
        test_uint(0, stats->uploads);
        test_uint(0, stats->tables_extracted);
        test_uint(0, stats->graph_compiles);
    }
    stop();
}
