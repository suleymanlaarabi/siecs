#include "backend/passes/passes.h"
static void bloom_downsample(
    SDL_GPUTexture *source,
    Uint32 source_width,
    Uint32 source_height,
    SDL_GPUTexture *target,
    Uint32 target_width,
    Uint32 target_height,
    float extract
) {
    SDL_GPUTextureSamplerBinding binding = {
        .texture = source,
        .sampler = SIGPU_GPUTARGETS->bloom_sampler,
    };
    float uniform[4] = {
        SIGPU_RENDERSETTINGS->bloom_threshold,
        extract,
        1.0f / (float)source_width,
        1.0f / (float)source_height,
    };
    sigpu_draw_fullscreen(
        SIGPU_GPUPIPELINES->bloom_down_pipeline,
        target,
        target_width,
        target_height,
        &binding,
        1,
        uniform,
        sizeof(uniform)
    );
}

static void bloom_blur(
    SDL_GPUTexture *source,
    SDL_GPUTexture *scratch,
    SDL_GPUTexture *target,
    Uint32 width,
    Uint32 height
) {
    SDL_GPUTextureSamplerBinding binding = {
        .texture = source,
        .sampler = SIGPU_GPUTARGETS->bloom_sampler,
    };
    float horizontal[4] = { 1.0f / (float)width, 0.0f, 0.0f, 0.0f };
    sigpu_draw_fullscreen(
        SIGPU_GPUPIPELINES->bloom_blur_pipeline,
        scratch,
        width,
        height,
        &binding,
        1,
        horizontal,
        sizeof(horizontal)
    );
    binding.texture = scratch;
    float vertical[4] = { 0.0f, 1.0f / (float)height, 0.0f, 0.0f };
    sigpu_draw_fullscreen(
        SIGPU_GPUPIPELINES->bloom_blur_pipeline,
        target,
        width,
        height,
        &binding,
        1,
        vertical,
        sizeof(vertical)
    );
}

void sigpu_bloom_pass(void) {
    if (!SIGPU_RENDERQUEUE->any_bloom) {
        return;
    }

    bloom_downsample(
        SIGPU_GPUTARGETS->bloom_texture,
        SIGPU_FRAMECONTEXT->frame_width,
        SIGPU_FRAMECONTEXT->frame_height,
        SIGPU_GPUTARGETS->bloom_half_scratch,
        SIGPU_GPUTARGETS->bloom_half_width,
        SIGPU_GPUTARGETS->bloom_half_height,
        1.0f
    );
    bloom_blur(
        SIGPU_GPUTARGETS->bloom_half_scratch,
        SIGPU_GPUTARGETS->bloom_half,
        SIGPU_GPUTARGETS->bloom_half_scratch,
        SIGPU_GPUTARGETS->bloom_half_width,
        SIGPU_GPUTARGETS->bloom_half_height
    );
    bloom_downsample(
        SIGPU_GPUTARGETS->bloom_half_scratch,
        SIGPU_GPUTARGETS->bloom_half_width,
        SIGPU_GPUTARGETS->bloom_half_height,
        SIGPU_GPUTARGETS->bloom_quarter_scratch,
        SIGPU_GPUTARGETS->bloom_quarter_width,
        SIGPU_GPUTARGETS->bloom_quarter_height,
        0.0f
    );
    bloom_blur(
        SIGPU_GPUTARGETS->bloom_quarter_scratch,
        SIGPU_GPUTARGETS->bloom_quarter,
        SIGPU_GPUTARGETS->bloom_quarter_scratch,
        SIGPU_GPUTARGETS->bloom_quarter_width,
        SIGPU_GPUTARGETS->bloom_quarter_height
    );
}
