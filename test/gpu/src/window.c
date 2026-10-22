#include "util.h"
#include <siwindow.h>
#include <siwindow/native.h>

/* siwindow on the offscreen video driver, then sigpu, a device and a
 * surface for slot "main". */
static ecs_entity_t start(bool window) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    ecs_init();
    ECS_MODULE_IMPORT(siwindow, { .keep_open_on_close = true });
    ECS_MODULE_IMPORT(sigpu, { 0 });
    size_t size;
    const unsigned char *pack = gpu_test_pack(&size);
    sigpu_asset_register(TEST_PACK, pack, size);
    if (window) {
        ecs_set(ecs_new(), Window, { .slot = "main", .title = "test", .width = 64, .height = 48 });
    }
    gpu_test_device(0);
    ecs_entity_t surface = ecs_new();
    ecs_set(surface, GpuSurface, { .window_slot = "main", .clear = { 0, 0, 1, 1 } });
    return surface;
}

static void stop(void) {
    ecs_fini();
    SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
}

void window_surface_waits_for_window(void) {
    ecs_entity_t surface = start(false);
    gpu_test_frames(2);
    test_uint(GpuWaiting, gpu_test_state(surface)->status);
    test_assert(strstr(gpu_test_state(surface)->message, "main") != NULL);
    ecs_entity_t window = ecs_new();
    ecs_set(window, Window, { .slot = "main", .title = "late", .width = 32, .height = 32 });
    gpu_test_frames(2);
    test_uint(GpuReady, gpu_test_state(surface)->status);
    const GpuSurfaceTargets *targets = ecs_get(surface, GpuSurfaceTargets);
    test_assert(targets->graph != 0);
    test_uint(GpuReady, ecs_get(targets->graph, GpuGraphState)->status);
    stop();
}

void window_surface_presents(void) {
    ecs_entity_t surface = start(true);
    gpu_test_frames(3);
    test_uint(GpuReady, gpu_test_state(surface)->status);
    const GpuSurfaceTargets *targets = ecs_get(surface, GpuSurfaceTargets);
    test_uint(GpuReady, ecs_get(targets->graph, GpuGraphState)->status);
    test_uint(2, ecs_get(targets->graph, GpuGraphState)->passes);
    test_uint(2, gpu_test_stats()->passes);
    test_uint(1, gpu_test_stats()->submissions);
    uint32_t width = 0, height = 0;
    test_true(sigpu_extent(targets->color, &width, &height));
    test_assert(width >= 64 && height >= 48);
    test_uint(0xffff0000u, gpu_test_pixel(targets->color, 1, 1));
    stop();
}

void window_resize_reallocates(void) {
    ecs_entity_t surface = start(true);
    gpu_test_frames(3);
    const GpuSurfaceTargets *targets = ecs_get(surface, GpuSurfaceTargets);
    const ecs_entity_t color = targets->color;
    const uint32_t revision = ecs_get(targets->graph, GpuGraphState)->revision;
    const uint32_t allocation = gpu_test_state(color)->allocation;
    ecs_query_each(it, i, ecs_in(Window)) {
        Window desc = ((const Window *)ecs_field(&it, 0))[i];
        desc.width = 80;
        desc.height = 60;
        ecs_set_cid(it.entities[i], ecs_id(Window), &desc);
    }
    gpu_test_frames(3);
    uint32_t width = 0, height = 0;
    test_true(sigpu_extent(color, &width, &height));
    test_assert(width >= 80 && height >= 60);
    test_uint(allocation + 1, gpu_test_state(color)->allocation);
    test_uint(revision, ecs_get(targets->graph, GpuGraphState)->revision);
    stop();
}

void window_minimized_suspends(void) {
    ecs_entity_t surface = start(true);
    gpu_test_frames(3);
    SDL_Window *native = siwindow_native(siwindow_find("main"));
    SDL_HideWindow(native);
    gpu_test_frames(1);
    /* Clear and present need the swapchain: both are suspended. */
    test_uint(0, gpu_test_stats()->passes);
    test_uint(2, gpu_test_stats()->passes_suspended);
    SDL_ShowWindow(native);
    gpu_test_frames(1);
    test_uint(2, gpu_test_stats()->passes);
    test_uint(GpuReady, gpu_test_state(surface)->status);
    stop();
}

void window_closed_waits_again(void) {
    ecs_entity_t surface = start(true);
    gpu_test_frames(3);
    test_uint(GpuReady, gpu_test_state(surface)->status);
    ecs_kill(siwindow_find("main"));
    gpu_test_frames(2);
    test_uint(GpuWaiting, gpu_test_state(surface)->status);
    test_uint(0, gpu_test_stats()->passes);
    stop();
}

void window_two_windows(void) {
    ecs_entity_t first = start(true);
    ecs_set(ecs_new(), Window, { .slot = "second", .title = "second", .width = 32, .height = 32 });
    ecs_entity_t second = ecs_new();
    ecs_set(second, GpuSurface, { .window_slot = "second", .clear = { 1, 0, 0, 1 } });
    gpu_test_frames(3);
    test_uint(GpuReady, gpu_test_state(first)->status);
    test_uint(GpuReady, gpu_test_state(second)->status);
    test_uint(4, gpu_test_stats()->passes);
    test_uint(1, gpu_test_stats()->submissions);
    test_uint(0xff0000ffu, gpu_test_pixel(ecs_get(second, GpuSurfaceTargets)->color, 1, 1));
    stop();
}

typedef struct {
    ecs_entity_t program;
    float color[4];
} fill_t;

static void fill(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    fill_t *data = user_data;
    gpu_cmd_uniform(ctx, "fill_color", data->color, sizeof(data->color));
    gpu_cmd_program(ctx, data->program, NULL);
    gpu_cmd_draw(ctx, 3, 1, 0, 0);
}

void window_split_screen(void) {
    ecs_entity_t surface = start(true);
    gpu_test_frames(2);
    const GpuSurfaceTargets *targets = ecs_get(surface, GpuSurfaceTargets);
    const ecs_entity_t program =
        gpu_test_graphics(gpu_test_shader(TEST_PACK), "fullscreen", "solid");
    fill_t left = { program, { 1, 0, 0, 1 } }, right = { program, { 0, 1, 0, 1 } };
    fill_t *halves[2] = { &left, &right };
    for (uint32_t i = 0; i < 2; i++) {
        ecs_entity_t pass = sigpu_pass(targets->graph, &(GpuPassDesc){
            .kind = GpuPassRender, .order = GPU_ORDER_SCENE + (int32_t)i, .execute = fill,
            .user_data = halves[i] });
        sigpu_pass_color(pass, targets->color, GpuLoadLoad, GpuStoreStore, NULL, 0);
        GpuPass desc = *ecs_get(pass, GpuPass);
        desc.viewport[0] = 0.5f * (float)i;
        desc.viewport[2] = 0.5f;
        desc.viewport[3] = 1.0f;
        ecs_set_cid(pass, ecs_id(GpuPass), &desc);
    }
    gpu_test_frames(3);
    test_uint(GpuReady, ecs_get(targets->graph, GpuGraphState)->status);
    uint32_t width, height;
    sigpu_extent(targets->color, &width, &height);
    /* The second viewport does not clear the first one's half. */
    test_uint(0xff0000ffu, gpu_test_pixel(targets->color, 2, height / 2));
    test_uint(0xff00ff00u, gpu_test_pixel(targets->color, width - 2, height / 2));
    stop();
}
