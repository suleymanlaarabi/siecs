#include "backend/backend.h"
static SDL_GPUTexture *create_hdr_texture(
    Uint32 width,
    Uint32 height,
    SDL_GPUTextureUsageFlags usage,
    SDL_GPUSampleCount sample_count
) {
    return SDL_CreateGPUTexture(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SIGPU_HDR_FORMAT,
            .usage = usage,
            .width = width,
            .height = height,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .sample_count = sample_count,
        }
    );
}

static void release_texture(SDL_GPUTexture **texture) {
    if (*texture) {
        SDL_ReleaseGPUTexture(SIGPU_GPUCONTEXT->device, *texture);
        *texture = NULL;
    }
}

void sigpu_frame_targets_release(void) {
    release_texture(&SIGPU_GPUTARGETS->depth_texture);
    release_texture(&SIGPU_GPUTARGETS->msaa_texture);
    release_texture(&SIGPU_GPUTARGETS->bloom_msaa_texture);
    release_texture(&SIGPU_GPUTARGETS->scene_texture);
    release_texture(&SIGPU_GPUTARGETS->bloom_texture);
    release_texture(&SIGPU_GPUTARGETS->bloom_half);
    release_texture(&SIGPU_GPUTARGETS->bloom_half_scratch);
    release_texture(&SIGPU_GPUTARGETS->bloom_quarter);
    release_texture(&SIGPU_GPUTARGETS->bloom_quarter_scratch);

    SIGPU_GPUTARGETS->target_width = 0;
    SIGPU_GPUTARGETS->target_height = 0;
}

void sigpu_frame_targets_prepare(void) {
    if (SIGPU_GPUTARGETS->scene_texture &&
        SIGPU_GPUTARGETS->target_width == SIGPU_FRAMECONTEXT->frame_width &&
        SIGPU_GPUTARGETS->target_height == SIGPU_FRAMECONTEXT->frame_height) {
        return;
    }

    sigpu_frame_targets_release();
    SIGPU_GPUTARGETS->depth_texture = SDL_CreateGPUTexture(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SIGPU_GPUTARGETS->depth_format,
            .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
            .width = SIGPU_FRAMECONTEXT->frame_width,
            .height = SIGPU_FRAMECONTEXT->frame_height,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .sample_count = SIGPU_GPUTARGETS->sample_count,
        }
    );
    SIGPU_GPUTARGETS->scene_texture = create_hdr_texture(
        SIGPU_FRAMECONTEXT->frame_width,
        SIGPU_FRAMECONTEXT->frame_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );

    if (SIGPU_GPUTARGETS->sample_count != SDL_GPU_SAMPLECOUNT_1) {
        SIGPU_GPUTARGETS->msaa_texture = create_hdr_texture(
            SIGPU_FRAMECONTEXT->frame_width,
            SIGPU_FRAMECONTEXT->frame_height,
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
            SIGPU_GPUTARGETS->sample_count
        );
        SIGPU_GPUTARGETS->bloom_msaa_texture = create_hdr_texture(
            SIGPU_FRAMECONTEXT->frame_width,
            SIGPU_FRAMECONTEXT->frame_height,
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
            SIGPU_GPUTARGETS->sample_count
        );
    }

    SIGPU_GPUTARGETS->bloom_texture = create_hdr_texture(
        SIGPU_FRAMECONTEXT->frame_width,
        SIGPU_FRAMECONTEXT->frame_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );
    SIGPU_GPUTARGETS->bloom_half_width =
        SIGPU_FRAMECONTEXT->frame_width > 1 ? SIGPU_FRAMECONTEXT->frame_width / 2 : 1;
    SIGPU_GPUTARGETS->bloom_half_height =
        SIGPU_FRAMECONTEXT->frame_height > 1 ? SIGPU_FRAMECONTEXT->frame_height / 2 : 1;
    SIGPU_GPUTARGETS->bloom_quarter_width =
        SIGPU_GPUTARGETS->bloom_half_width > 1 ? SIGPU_GPUTARGETS->bloom_half_width / 2 : 1;
    SIGPU_GPUTARGETS->bloom_quarter_height =
        SIGPU_GPUTARGETS->bloom_half_height > 1 ? SIGPU_GPUTARGETS->bloom_half_height / 2 : 1;
    SIGPU_GPUTARGETS->bloom_half = create_hdr_texture(
        SIGPU_GPUTARGETS->bloom_half_width,
        SIGPU_GPUTARGETS->bloom_half_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );
    SIGPU_GPUTARGETS->bloom_half_scratch = create_hdr_texture(
        SIGPU_GPUTARGETS->bloom_half_width,
        SIGPU_GPUTARGETS->bloom_half_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );
    SIGPU_GPUTARGETS->bloom_quarter = create_hdr_texture(
        SIGPU_GPUTARGETS->bloom_quarter_width,
        SIGPU_GPUTARGETS->bloom_quarter_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );
    SIGPU_GPUTARGETS->bloom_quarter_scratch = create_hdr_texture(
        SIGPU_GPUTARGETS->bloom_quarter_width,
        SIGPU_GPUTARGETS->bloom_quarter_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );

    SIGPU_GPUTARGETS->target_width = SIGPU_FRAMECONTEXT->frame_width;
    SIGPU_GPUTARGETS->target_height = SIGPU_FRAMECONTEXT->frame_height;
}

static SDL_GPUSampleCount sample_count_from_int(int samples) {
    switch (samples) {
    case 8:
        return SDL_GPU_SAMPLECOUNT_8;
    case 4:
        return SDL_GPU_SAMPLECOUNT_4;
    case 2:
        return SDL_GPU_SAMPLECOUNT_2;
    default:
        return SDL_GPU_SAMPLECOUNT_1;
    }
}

SDL_GPUSampleCount sigpu_supported_sample_count(int samples) {
    SDL_GPUSampleCount selected = sample_count_from_int(samples);

    while (
        selected != SDL_GPU_SAMPLECOUNT_1 &&
        (!SDL_GPUTextureSupportsSampleCount(SIGPU_GPUCONTEXT->device, SIGPU_HDR_FORMAT, selected) ||
         !SDL_GPUTextureSupportsSampleCount(
             SIGPU_GPUCONTEXT->device,
             SIGPU_GPUTARGETS->depth_format,
             selected
         ))) {
        switch (selected) {
        case SDL_GPU_SAMPLECOUNT_8:
            selected = SDL_GPU_SAMPLECOUNT_4;
            break;
        case SDL_GPU_SAMPLECOUNT_4:
            selected = SDL_GPU_SAMPLECOUNT_2;
            break;
        default:
            selected = SDL_GPU_SAMPLECOUNT_1;
        }
    }
    return selected;
}

void sigpu_sample_count_set(int samples) {
    SDL_GPUSampleCount selected = sigpu_supported_sample_count(samples);
    if (selected != SIGPU_GPUTARGETS->sample_count) {
        SIGPU_GPUTARGETS->sample_count = selected;
        sigpu_frame_targets_release();
        sigpu_main_pipelines_recreate();
    }
}
