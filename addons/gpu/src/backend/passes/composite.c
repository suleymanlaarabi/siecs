#include "backend/passes/passes.h"
void sigpu_composite_pass(void) {
    SDL_GPUTexture *half = SIGPU_RENDERQUEUE->any_bloom && SIGPU_RENDERSETTINGS->bloom_enabled
                               ? SIGPU_GPUTARGETS->bloom_half_scratch
                               : SIGPU_GPUTARGETS->bloom_texture;
    SDL_GPUTexture *quarter = SIGPU_RENDERQUEUE->any_bloom && SIGPU_RENDERSETTINGS->bloom_enabled
                                  ? SIGPU_GPUTARGETS->bloom_quarter_scratch
                                  : SIGPU_GPUTARGETS->bloom_texture;
    SDL_GPUTextureSamplerBinding samplers[3] = {
        { .texture = SIGPU_GPUTARGETS->scene_texture, .sampler = SIGPU_GPUTARGETS->bloom_sampler },
        { .texture = half, .sampler = SIGPU_GPUTARGETS->bloom_sampler },
        { .texture = quarter, .sampler = SIGPU_GPUTARGETS->bloom_sampler },
    };
    float uniform[4] = {
        SIGPU_RENDERSETTINGS->bloom_enabled && SIGPU_RENDERQUEUE->any_bloom
            ? SIGPU_RENDERSETTINGS->bloom_intensity
            : 0.0f,
        SIGPU_GPUCONTEXT->linear_swapchain ? 0.0f : 1.0f,
        0.0f,
        0.0f,
    };
    SDL_GPUColorTargetInfo color_target = {
        .texture = SIGPU_FRAMECONTEXT->swapchain,
        .load_op = SDL_GPU_LOADOP_DONT_CARE,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *pass =
        SDL_BeginGPURenderPass(SIGPU_FRAMECONTEXT->command_buffer, &color_target, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, SIGPU_GPUPIPELINES->bloom_composite_pipeline);
    SDL_BindGPUFragmentSamplers(pass, 0, samplers, 3);
    SDL_PushGPUFragmentUniformData(SIGPU_FRAMECONTEXT->command_buffer, 0, uniform, sizeof(uniform));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}
