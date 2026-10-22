#include "sigpu_internal.h"
#include "shaders/sigpu_pack.generated.h"
#include <siwindow/native.h>
#include <string.h>

#define GPU_INTERNAL_PACK "sigpu://internal.gpushader"

/* Swapchain textures acquired this frame, by surface. */
typedef struct {
    ecs_entity_t surface;
    SDL_GPUTexture *texture;
    uint32_t width;
    uint32_t height;
} gpu_acquired_t;

static GPU_VEC(gpu_acquired_t) acquired;

void gpu_internal_assets(void) {
    sigpu_asset_register(GPU_INTERNAL_PACK, sigpu_gpushader, sizeof(sigpu_gpushader));
}

/* The present program and its sampler, created with the first surface so a
 * session without surfaces owns no extra GPU object. */
static void internal_resources(void) {
    if (gpu_session.present_program && ecs_is_alive(gpu_session.present_program)) {
        return;
    }
    const ecs_entity_t shader = ecs_new_named(0, "sigpu::internal::shader");
    ecs_set(shader, GpuShader, { .path = GPU_INTERNAL_PACK });
    gpu_session.present_program = ecs_new_named(0, "sigpu::internal::present");
    ecs_set(gpu_session.present_program, GpuProgram,
            { .shader = shader, .vertex = "fullscreen", .fragment = "present" });
    gpu_session.present_sampler = ecs_new_named(0, "sigpu::internal::linear");
    ecs_set(gpu_session.present_sampler, GpuSampler,
            { .min_filter = GpuFilterLinear, .mag_filter = GpuFilterLinear,
              .address_u = GpuAddressClamp, .address_v = GpuAddressClamp,
              .address_w = GpuAddressClamp });
}

static ecs_entity_t child(ecs_entity_t parent) {
    const ecs_entity_t entity = ecs_new();
    ecs_relate(entity, ChildOf, parent);
    return entity;
}

/* Graph, color target, clear and present passes of a surface, created once;
 * the clear color follows the descriptor. */
static void surface_targets(ecs_entity_t surface, const GpuSurface *desc) {
    internal_resources();
    const GpuSurfaceTargets *existing = ecs_try_get(surface, GpuSurfaceTargets);
    if (existing && ecs_is_alive(existing->graph) && ecs_is_alive(existing->clear_pass)) {
        const GpuPassIO *io = ecs_try_get(existing->clear_pass, GpuPassIO);
        if (io && io->count == 1 && memcmp(io->items[0].clear, desc->clear, sizeof(desc->clear))) {
            GpuPassIO updated = *io;
            memcpy(updated.items[0].clear, desc->clear, sizeof(desc->clear));
            ecs_set_cid(existing->clear_pass, ecs_id(GpuPassIO), &updated);
        }
        return;
    }
    GpuSurfaceTargets targets = { 0 };
    targets.graph = child(surface);
    ecs_set(targets.graph, GpuRenderGraph, { .order = 0 });
    targets.color = child(surface);
    ecs_set(targets.color, GpuTexture,
            { .format = GpuFormatRgba8Srgb, .usage = GpuTextureColorTarget | GpuTextureSampled,
              .size_from = surface });
    targets.clear_pass = sigpu_pass(targets.graph, &(GpuPassDesc){
        .name = "clear", .kind = GpuPassRender, .order = GPU_ORDER_CLEAR });
    sigpu_pass_color(targets.clear_pass, targets.color, GpuLoadClear, GpuStoreStore, desc->clear, 0);
    targets.present_pass = sigpu_pass(targets.graph, &(GpuPassDesc){
        .name = "present", .kind = GpuPassPresent, .order = GPU_ORDER_PRESENT,
        .program = gpu_session.present_program });
    sigpu_pass_sample(targets.present_pass, "source", targets.color, gpu_session.present_sampler);
    sigpu_pass_access(targets.present_pass, &(GpuAccess){ .resource = surface,
                                                          .usage = GpuUseColor,
                                                          .load = GpuLoadDontCare });
    ecs_set_cid(surface, ecs_id(GpuSurfaceTargets), &targets);
}

/* The window is about to be destroyed: its swapchain goes first and the
 * surface waits for a window again. */
static void window_destroyed(ecs_entity_t window, void *native) {
    (void)window;
    if (!gpu_session.gpu) {
        return;
    }
    for (uint32_t id = 1; id <= gpu_session.records.count; id++) {
        gpu_record_t *record = gpu_record_at(id);
        if (record && record->kind == GPU_RESOURCE_SURFACE && record->as.surface.window == native) {
            gpu_surface_release_window(record);
            gpu_touch(record->entity, true);
        }
    }
}

void gpu_surface_release_window(gpu_record_t *record) {
    if (!record->as.surface.window || !gpu_session.gpu) {
        record->as.surface.window = NULL;
        return;
    }
    if (!gpu_session.lost) {
        SDL_WaitForGPUIdle(gpu_session.gpu);
    }
    SDL_ReleaseWindowFromGPUDevice(gpu_session.gpu, record->as.surface.window);
    record->as.surface.window = NULL;
}

static SDL_GPUPresentMode present_mode(SDL_Window *window, uint8_t mode) {
    const SDL_GPUPresentMode wanted = mode == GpuPresentImmediate ? SDL_GPU_PRESENTMODE_IMMEDIATE
                                      : mode == GpuPresentMailbox ? SDL_GPU_PRESENTMODE_MAILBOX
                                                                  : SDL_GPU_PRESENTMODE_VSYNC;
    return SDL_WindowSupportsGPUPresentMode(gpu_session.gpu, window, wanted)
               ? wanted
               : SDL_GPU_PRESENTMODE_VSYNC;
}

static void pixel_size(SDL_Window *window, uint32_t *width, uint32_t *height) {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    *width = w > 0 ? (uint32_t)w : 1;
    *height = h > 0 ? (uint32_t)h : 1;
}

void gpu_surface_build(ecs_entity_t entity, gpu_result_t *result, const gpu_record_t *current) {
    /* A copy: creating the targets migrates the entity to another table. */
    const GpuSurface value = *ecs_get(entity, GpuSurface);
    const GpuSurface *desc = &value;
    if (!memchr(desc->window_slot, 0, sizeof(desc->window_slot)) || !desc->window_slot[0]) {
        gpu_fail(result, GpuErrorInvalid, 0, "window_slot is empty or not terminated");
        return;
    }
    if (desc->present_mode > GpuPresentMailbox) {
        gpu_fail(result, GpuErrorInvalid, 0, "present_mode is not a GpuPresentMode");
        return;
    }
    surface_targets(entity, desc);
    const ecs_entity_t window = siwindow_find(desc->window_slot);
    SDL_Window *native = window ? siwindow_native(window) : NULL;
    if (!native) {
        gpu_result_set(result, GpuWaiting, GpuErrorNone, 0, "waiting for window '%s'",
                       desc->window_slot);
        return;
    }
    if (!gpu_session.window_hook) {
        siwindow_on_destroy(window_destroyed);
        gpu_session.window_hook = true;
    }
    gpu_record_t *kept = (gpu_record_t *)current;
    if (kept && kept->kind == GPU_RESOURCE_SURFACE && kept->as.surface.window == native) {
        SDL_SetGPUSwapchainParameters(
            gpu_session.gpu, native,
            kept->as.surface.srgb ? SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR
                                  : SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
            present_mode(native, desc->present_mode)
        );
        uint32_t width, height;
        pixel_size(native, &width, &height);
        result->resized = width != kept->as.surface.width || height != kept->as.surface.height;
        kept->as.surface.width = width;
        kept->as.surface.height = height;
        result->kept = true;
        result->status = GpuReady;
        result->built.kind = GPU_RESOURCE_SURFACE;
        return;
    }
    if (!SDL_ClaimWindowForGPUDevice(gpu_session.gpu, native)) {
        gpu_fail(result, GpuErrorSdl, window, "SDL refused the window: %s", SDL_GetError());
        return;
    }
    const bool srgb = SDL_WindowSupportsGPUSwapchainComposition(
        gpu_session.gpu, native, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR);
    SDL_SetGPUSwapchainParameters(gpu_session.gpu, native,
                                  srgb ? SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR
                                       : SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                  present_mode(native, desc->present_mode));
    gpu_record_t *built = &result->built;
    built->kind = GPU_RESOURCE_SURFACE;
    built->as.surface.window = native;
    built->as.surface.window_entity = window;
    built->as.surface.srgb = srgb;
    built->as.surface.format = SDL_GetGPUSwapchainTextureFormat(gpu_session.gpu, native);
    pixel_size(native, &built->as.surface.width, &built->as.surface.height);
    result->status = GpuReady;
}

/* Polled once per frame: few surfaces. A Waiting surface whose window
 * appeared is queued; a resized window resizes its surface and the textures
 * that follow it. */
void gpu_surfaces_sync(void) {
    for (uint32_t id = 1; id <= gpu_session.records.count; id++) {
        gpu_record_t *record = gpu_record_at(id);
        if (!record || record->kind != GPU_RESOURCE_SURFACE || !record->as.surface.window) {
            continue;
        }
        uint32_t width, height;
        pixel_size(record->as.surface.window, &width, &height);
        if (width != record->as.surface.width || height != record->as.surface.height) {
            record->as.surface.width = width;
            record->as.surface.height = height;
            gpu_wake(record->entity);
        }
    }
    ecs_query_each(it, i, ecs_filter(GpuSurface), ecs_in(GpuResourceState)) {
        const GpuResourceState *states = ecs_field(&it, 0);
        if (states[i].status != GpuWaiting || states[i].queued) {
            continue;
        }
        const GpuSurface *desc = ecs_get(it.entities[i], GpuSurface);
        if (siwindow_native(siwindow_find(desc->window_slot))) {
            gpu_touch(it.entities[i], false);
        }
    }
}

SDL_GPUTexture *gpu_surface_acquire(SDL_GPUCommandBuffer *cmd, ecs_entity_t surface,
                                    uint32_t *width, uint32_t *height) {
    for (uint32_t i = 0; i < acquired.count; i++) {
        if (acquired.items[i].surface == surface) {
            *width = acquired.items[i].width;
            *height = acquired.items[i].height;
            return acquired.items[i].texture;
        }
    }
    const gpu_record_t *record = gpu_resource_record(surface, GPU_RESOURCE_SURFACE);
    gpu_acquired_t entry = { .surface = surface };
    if (record && record->as.surface.window) {
        const SDL_WindowFlags flags = SDL_GetWindowFlags(record->as.surface.window);
        if (!(flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) &&
            !SDL_WaitAndAcquireGPUSwapchainTexture(cmd, record->as.surface.window, &entry.texture,
                                                   &entry.width, &entry.height)) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: swapchain acquisition: %s",
                         SDL_GetError());
            entry.texture = NULL;
        }
    }
    gpu_vec_push(acquired, entry);
    *width = entry.width;
    *height = entry.height;
    return entry.texture;
}

void gpu_surfaces_frame_end(void) { acquired.count = 0; }

void gpu_surfaces_fini(void) {
    gpu_vec_free(acquired);
    gpu_session.window_hook = false;
}
