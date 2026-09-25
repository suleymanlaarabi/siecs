#include "backend/passes/passes.h"
void sigpu_forward_pass(void) {
    bool msaa = SIGPU_GPUTARGETS->sample_count != SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUColorTargetInfo color_targets[2] = {
        {
            .texture = msaa ? SIGPU_GPUTARGETS->msaa_texture : SIGPU_GPUTARGETS->scene_texture,
            .clear_color = SIGPU_RENDERSETTINGS->sky_linear,
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = msaa ? SDL_GPU_STOREOP_RESOLVE : SDL_GPU_STOREOP_STORE,
            .resolve_texture = msaa ? SIGPU_GPUTARGETS->scene_texture : NULL,
            .cycle = msaa,
        },
        {
            .texture =
                msaa ? SIGPU_GPUTARGETS->bloom_msaa_texture : SIGPU_GPUTARGETS->bloom_texture,
            .clear_color = { 0.0f, 0.0f, 0.0f, 0.0f },
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = msaa ? SDL_GPU_STOREOP_RESOLVE : SDL_GPU_STOREOP_STORE,
            .resolve_texture = msaa ? SIGPU_GPUTARGETS->bloom_texture : NULL,
            .cycle = msaa,
        },
    };
    SDL_GPUDepthStencilTargetInfo depth_target = {
        .texture = SIGPU_GPUTARGETS->depth_texture,
        .clear_depth = 1.0f,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
        .cycle = true,
    };
    SDL_GPURenderPass *pass =
        SDL_BeginGPURenderPass(SIGPU_FRAMECONTEXT->command_buffer, color_targets, 2, &depth_target);
    sigpu_transform_uniform_t transforms = {
        .view_projection = SIGPU_RENDERVIEW->view_projection,
        .light_view_projection = SIGPU_RENDERVIEW->light_view_projection,
    };
    sigpu_lighting_uniform_t lighting = {
        .camera_position = { SIGPU_RENDERVIEW->camera.position.x, SIGPU_RENDERVIEW->camera.position.y, SIGPU_RENDERVIEW->camera.position.z, 1.0f },
        .sun_direction_intensity = {
            SIGPU_RENDERSETTINGS->sun_direction.x,
            SIGPU_RENDERSETTINGS->sun_direction.y,
            SIGPU_RENDERSETTINGS->sun_direction.z,
            SIGPU_RENDERSETTINGS->sun_intensity,
        },
        .sun_color = { SIGPU_RENDERSETTINGS->sun_color.r, SIGPU_RENDERSETTINGS->sun_color.g, SIGPU_RENDERSETTINGS->sun_color.b, 1.0f },
        .ambient_color_intensity = {
            SIGPU_RENDERSETTINGS->ambient_color.r,
            SIGPU_RENDERSETTINGS->ambient_color.g,
            SIGPU_RENDERSETTINGS->ambient_color.b,
            SIGPU_RENDERSETTINGS->ambient_intensity,
        },
        .fog_color = { SIGPU_RENDERSETTINGS->fog_color.r, SIGPU_RENDERSETTINGS->fog_color.g, SIGPU_RENDERSETTINGS->fog_color.b, 1.0f },
        .fog_parameters = {
            SIGPU_RENDERSETTINGS->fog_start,
            SIGPU_RENDERSETTINGS->fog_end,
            SIGPU_RENDERSETTINGS->fog_enabled ? 1.0f : 0.0f,
            SIGPU_RENDERSETTINGS->shadows_enabled ? 1.0f : 0.0f,
        },
        .shadow_parameters = {
            SIGPU_RENDERSETTINGS->shadow_distance,
            0.0f,
            0.0f,
            0.0f,
        },
    };
    SDL_GPUTextureSamplerBinding shadow_binding = {
        .texture = SIGPU_GPUTARGETS->shadow_texture,
        .sampler = SIGPU_GPUTARGETS->shadow_sampler,
    };
    SDL_PushGPUVertexUniformData(
        SIGPU_FRAMECONTEXT->command_buffer,
        0,
        &transforms,
        sizeof(transforms)
    );
    SDL_PushGPUFragmentUniformData(
        SIGPU_FRAMECONTEXT->command_buffer,
        0,
        &lighting,
        sizeof(lighting)
    );
    SDL_BindGPUFragmentSamplers(pass, 0, &shadow_binding, 1);
    sigpu_bind_mesh(pass);
    sigpu_draw_static_chunks(
        pass,
        SIGPU_GPUPIPELINES->axis_pipeline,
        SIGPU_GPUPIPELINES->rotated_pipeline,
        false
    );
    sigpu_draw_shared_batches(
        pass,
        SIGPU_GPUPIPELINES->shared_axis_pipeline,
        SIGPU_GPUCONTEXT->axis_buffer,
        SIGPU_RENDERQUEUE->shared_axis_batches,
        SIGPU_RENDERQUEUE->shared_axis_batch_count,
        sizeof(sigpu_shared_axis_instance_t),
        false
    );
    sigpu_draw_shared_batches(
        pass,
        SIGPU_GPUPIPELINES->shared_rotated_pipeline,
        SIGPU_GPUCONTEXT->rotated_buffer,
        SIGPU_RENDERQUEUE->shared_rotated_batches,
        SIGPU_RENDERQUEUE->shared_rotated_batch_count,
        sizeof(sigpu_shared_rotated_instance_t),
        false
    );
    sigpu_draw_owned_batches(
        pass,
        SIGPU_GPUPIPELINES->axis_pipeline,
        SIGPU_GPUCONTEXT->axis_buffer,
        SIGPU_RENDERQUEUE->owned_axis_batches,
        SIGPU_RENDERQUEUE->owned_axis_batch_count,
        SIGPU_GPUCONTEXT->axis_capacity,
        sizeof(sigpu_axis_instance_t),
        false
    );
    sigpu_draw_owned_batches(
        pass,
        SIGPU_GPUPIPELINES->rotated_pipeline,
        SIGPU_GPUCONTEXT->rotated_buffer,
        SIGPU_RENDERQUEUE->owned_rotated_batches,
        SIGPU_RENDERQUEUE->owned_rotated_batch_count,
        SIGPU_GPUCONTEXT->rotated_capacity,
        sizeof(sigpu_rotated_instance_t),
        false
    );
    SDL_EndGPURenderPass(pass);
}
