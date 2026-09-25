#include "backend/passes/passes.h"
void sigpu_shadow_pass(void) {
    for (Uint32 cascade_index = 0; cascade_index < SIGPU_RENDERVIEW->cascade_count; cascade_index++) {
    sigpu_shadow_cascade_t *cascade = &SIGPU_RENDERVIEW->cascades[cascade_index];
    if (cascade_index && cascade->cache_valid &&
        cascade->cached_static_generation == SIGPU_STATICRENDERCACHE->static_generation &&
        SDL_memcmp(&cascade->cached_view_projection, &cascade->view_projection, sizeof(sigpu_mat4_t)) == 0) {
        SIGPU_RENDERSTATS->shadow_reuses[cascade_index]++;
        continue;
    }
    SIGPU_RENDERSTATS->shadow_redraws[cascade_index]++;
    SDL_GPUDepthStencilTargetInfo depth_target = {
        .texture = SIGPU_GPUTARGETS->shadow_texture,
        .clear_depth = 1.0f,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
        .cycle = false,
        .layer = (Uint8)cascade_index,
    };
    SDL_GPURenderPass *pass =
        SDL_BeginGPURenderPass(SIGPU_FRAMECONTEXT->command_buffer, NULL, 0, &depth_target);
    SDL_PushGPUVertexUniformData(
        SIGPU_FRAMECONTEXT->command_buffer,
        0,
        &cascade->view_projection,
        sizeof(cascade->view_projection)
    );
    sigpu_bind_mesh(pass);
    sigpu_draw_static_chunks(
        pass,
        SIGPU_GPUPIPELINES->axis_shadow_pipeline,
        SIGPU_GPUPIPELINES->rotated_shadow_pipeline,
        true,
        cascade_index
    );
    if (cascade_index == 0) {
    sigpu_draw_shared_batches(
        pass,
        SIGPU_GPUPIPELINES->shared_axis_shadow_pipeline,
        SIGPU_GPUCONTEXT->shadow_axis_buffer,
        SIGPU_RENDERQUEUE->shadow_shared_axis_batches,
        SIGPU_RENDERQUEUE->shadow_shared_axis_batch_count,
        sizeof(sigpu_shared_axis_instance_t),
        true
    );
    sigpu_draw_shared_batches(
        pass,
        SIGPU_GPUPIPELINES->shared_rotated_shadow_pipeline,
        SIGPU_GPUCONTEXT->shadow_rotated_buffer,
        SIGPU_RENDERQUEUE->shadow_shared_rotated_batches,
        SIGPU_RENDERQUEUE->shadow_shared_rotated_batch_count,
        sizeof(sigpu_shared_rotated_instance_t),
        true
    );
    sigpu_draw_owned_batches(
        pass,
        SIGPU_GPUPIPELINES->axis_shadow_pipeline,
        SIGPU_GPUCONTEXT->shadow_axis_buffer,
        SIGPU_RENDERQUEUE->shadow_owned_axis_batches,
        SIGPU_RENDERQUEUE->shadow_owned_axis_batch_count,
        SIGPU_GPUCONTEXT->shadow_axis_capacity,
        sizeof(sigpu_axis_instance_t),
        true
    );
    sigpu_draw_owned_batches(
        pass,
        SIGPU_GPUPIPELINES->rotated_shadow_pipeline,
        SIGPU_GPUCONTEXT->shadow_rotated_buffer,
        SIGPU_RENDERQUEUE->shadow_owned_rotated_batches,
        SIGPU_RENDERQUEUE->shadow_owned_rotated_batch_count,
        SIGPU_GPUCONTEXT->shadow_rotated_capacity,
        sizeof(sigpu_rotated_instance_t),
        true
    );
    }
    SDL_EndGPURenderPass(pass);
    cascade->cached_view_projection = cascade->view_projection;
    cascade->cached_static_generation = SIGPU_STATICRENDERCACHE->static_generation;
    cascade->cache_valid = true;
    }
}
