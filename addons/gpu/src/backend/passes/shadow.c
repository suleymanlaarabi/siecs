#include "backend/passes/passes.h"
void sigpu_shadow_pass(void) {
    if (sigpu_no_instances() && SIGPU_STATICRENDERCACHE->static_shadow_visible_count == 0) {
        return;
    }

    SDL_GPUDepthStencilTargetInfo depth_target = {
        .texture = SIGPU_GPUTARGETS->shadow_texture,
        .clear_depth = 1.0f,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
        .cycle = true,
    };
    SDL_GPURenderPass *pass =
        SDL_BeginGPURenderPass(SIGPU_FRAMECONTEXT->command_buffer, NULL, 0, &depth_target);
    SDL_PushGPUVertexUniformData(
        SIGPU_FRAMECONTEXT->command_buffer,
        0,
        &SIGPU_RENDERVIEW->light_view_projection,
        sizeof(SIGPU_RENDERVIEW->light_view_projection)
    );
    sigpu_bind_mesh(pass);
    sigpu_draw_static_chunks(
        pass,
        SIGPU_GPUPIPELINES->axis_shadow_pipeline,
        SIGPU_GPUPIPELINES->rotated_shadow_pipeline,
        true
    );
    sigpu_draw_shared_batches(
        pass,
        SIGPU_GPUPIPELINES->shared_axis_shadow_pipeline,
        SIGPU_GPUCONTEXT->axis_buffer,
        SIGPU_RENDERQUEUE->shared_axis_batches,
        SIGPU_RENDERQUEUE->shared_axis_batch_count,
        sizeof(sigpu_shared_axis_instance_t),
        true
    );
    sigpu_draw_shared_batches(
        pass,
        SIGPU_GPUPIPELINES->shared_rotated_shadow_pipeline,
        SIGPU_GPUCONTEXT->rotated_buffer,
        SIGPU_RENDERQUEUE->shared_rotated_batches,
        SIGPU_RENDERQUEUE->shared_rotated_batch_count,
        sizeof(sigpu_shared_rotated_instance_t),
        true
    );
    sigpu_draw_owned_batches(
        pass,
        SIGPU_GPUPIPELINES->axis_shadow_pipeline,
        SIGPU_GPUCONTEXT->axis_buffer,
        SIGPU_RENDERQUEUE->owned_axis_batches,
        SIGPU_RENDERQUEUE->owned_axis_batch_count,
        SIGPU_GPUCONTEXT->axis_capacity,
        sizeof(sigpu_axis_instance_t),
        true
    );
    sigpu_draw_owned_batches(
        pass,
        SIGPU_GPUPIPELINES->rotated_shadow_pipeline,
        SIGPU_GPUCONTEXT->rotated_buffer,
        SIGPU_RENDERQUEUE->owned_rotated_batches,
        SIGPU_RENDERQUEUE->owned_rotated_batch_count,
        SIGPU_GPUCONTEXT->rotated_capacity,
        sizeof(sigpu_rotated_instance_t),
        true
    );
    SDL_EndGPURenderPass(pass);
}
