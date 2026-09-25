#include "render/render_internal.h"

static ecs_system_id_t shadow_pass_system;
static ecs_system_id_t bloom_pass_system;

static void begin_gpu_frame(ecs_iter_t *it) {
    SIGPU_RENDERSTATS->frame_start_ns = SIGPU_RENDERSTATS->profile_enabled ? SDL_GetTicksNS() : 0;
    SIGPU_RENDERSTATS->acquire_ns = SIGPU_RENDERSTATS->collect_ns = SIGPU_RENDERSTATS->cull_ns =
        SIGPU_RENDERSTATS->encode_ns = 0;
    SIGPU_RENDERSTATS->draw_calls = SIGPU_RENDERSTATS->drawn_instances = 0;
    SIGPU_RENDERQUEUE->shared_axis_count = 0;
    SIGPU_RENDERQUEUE->shared_rotated_count = 0;
    SIGPU_RENDERQUEUE->owned_axis_count = 0;
    SIGPU_RENDERQUEUE->owned_rotated_count = 0;
    SIGPU_RENDERQUEUE->shared_axis_batch_count = 0;
    SIGPU_RENDERQUEUE->shared_rotated_batch_count = 0;
    SIGPU_RENDERQUEUE->owned_axis_batch_count = 0;
    SIGPU_RENDERQUEUE->owned_rotated_batch_count = 0;
    SIGPU_RENDERQUEUE->any_bloom = false;

    if (!sigpu_begin_frame())
        ecs_quit();
}

static void upload_instances(ecs_iter_t *it) {
    if (!SIGPU_FRAMECONTEXT->swapchain) {
        sigpu_upload_instances();
        return;
    }
    SIGPU_RENDERSTATS->encode_ns = SIGPU_RENDERSTATS->profile_enabled ? SDL_GetTicksNS() : 0;
    sigpu_frame_targets_prepare();
    sigpu_upload_instances();
}

static void shadow_pass(ecs_iter_t *it) {
    if (SIGPU_FRAMECONTEXT->swapchain)
        sigpu_shadow_pass();
}

static void forward_pass(ecs_iter_t *it) {
    if (SIGPU_FRAMECONTEXT->swapchain)
        sigpu_forward_pass();
}

static void bloom_pass(ecs_iter_t *it) {
    if (SIGPU_FRAMECONTEXT->swapchain)
        sigpu_bloom_pass();
}

static void composite_pass(ecs_iter_t *it) {
    if (SIGPU_FRAMECONTEXT->swapchain)
        sigpu_composite_pass();
    if (SIGPU_RENDERSTATS->profile_enabled && SIGPU_FRAMECONTEXT->swapchain)
        SIGPU_RENDERSTATS->encode_ns = SDL_GetTicksNS() - SIGPU_RENDERSTATS->encode_ns;
}

static void end_gpu_frame(ecs_iter_t *it) { sigpu_end_frame(); }

void sigpu_render_schedule_set_shadows(bool enabled) {
    if (shadow_pass_system) {
        if (enabled)
            ecs_system_enable(shadow_pass_system);
        else
            ecs_system_disable(shadow_pass_system);
    }
    sigpu_bounds_set_enabled(enabled);
    sigpu_primitives_set_shadows(enabled);
    if (!enabled) {
        SIGPU_STATICRENDERCACHE->static_shadow_visible_count = 0;
        for (Uint32 i = 0; i < SIGPU_STATICRENDERCACHE->static_chunk_count; i++) {
            sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[i];
            chunk->shadow_visible = false;
            RenderChunkVisibility *visibility =
                ecs_try_get(SIGPU_STATICRENDERCACHE->chunk_info[i].entity, RenderChunkVisibility);
            if (visibility)
                visibility->shadow = false;
        }
    }
}

void sigpu_render_schedule_set_bloom(bool enabled) {
    if (!bloom_pass_system)
        return;
    if (enabled)
        ecs_system_enable(bloom_pass_system);
    else
        ecs_system_disable(bloom_pass_system);
}

void sigpu_render_schedule_register(ecs_system_id_t input_system) {
    ecs_system(
        {
            .name = "BeginGpuFrame",
            .phase = EcsPreUpdate,
            .after = { input_system },
            .callback = begin_gpu_frame,
            .main_thread_only = true,
            .no_defer = true,
        }
    );
    ecs_system_id_t upload = ecs_system(
        {
            .name = "UploadInstances",
            .phase = EcsPostRender,
            .callback = upload_instances,
            .main_thread_only = true,
        }
    );
    shadow_pass_system = ecs_system(
        {
            .name = "ShadowPass",
            .phase = EcsPostRender,
            .after = { upload },
            .callback = shadow_pass,
            .main_thread_only = true,
        }
    );
    ecs_system_id_t forward = ecs_system(
        {
            .name = "ForwardPass",
            .phase = EcsPostRender,
            .after = { shadow_pass_system },
            .callback = forward_pass,
            .main_thread_only = true,
        }
    );
    bloom_pass_system = ecs_system(
        {
            .name = "BloomPass",
            .phase = EcsPostRender,
            .after = { forward },
            .callback = bloom_pass,
            .main_thread_only = true,
        }
    );
    ecs_system_id_t composite = ecs_system(
        {
            .name = "CompositePass",
            .phase = EcsPostRender,
            .after = { bloom_pass_system },
            .callback = composite_pass,
            .main_thread_only = true,
        }
    );
    ecs_system(
        {
            .name = "EndGpuFrame",
            .phase = EcsPostRender,
            .after = { composite },
            .callback = end_gpu_frame,
            .main_thread_only = true,
            .no_defer = true,
        }
    );
    sigpu_render_schedule_set_shadows(SIGPU_RENDERSETTINGS->shadows_enabled);
    sigpu_render_schedule_set_bloom(SIGPU_RENDERSETTINGS->bloom_enabled);
}

void sigpu_render_schedule_reset(void) {
    shadow_pass_system = bloom_pass_system = 0;
    sigpu_bounds_reset();
    sigpu_primitives_reset();
}

void sigpu_render_schedule_fini(void) {
    SDL_free(SIGPU_RENDERQUEUE->shared_axis_batches);
    SDL_free(SIGPU_RENDERQUEUE->shared_rotated_batches);
    SDL_free(SIGPU_RENDERQUEUE->owned_axis_batches);
    SDL_free(SIGPU_RENDERQUEUE->owned_rotated_batches);
}
