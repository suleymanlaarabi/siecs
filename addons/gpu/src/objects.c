#include "sigpu_internal.h"
#include <stdlib.h>

static const struct {
    SDL_GPUTextureFormat sdl;
    bool depth;
} formats[GpuFormatCount] = {
    [GpuFormatInvalid] = { SDL_GPU_TEXTUREFORMAT_INVALID, false },
    [GpuFormatRgba8] = { SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, false },
    [GpuFormatRgba8Srgb] = { SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB, false },
    [GpuFormatBgra8] = { SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, false },
    [GpuFormatBgra8Srgb] = { SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB, false },
    [GpuFormatR8] = { SDL_GPU_TEXTUREFORMAT_R8_UNORM, false },
    [GpuFormatRg8] = { SDL_GPU_TEXTUREFORMAT_R8G8_UNORM, false },
    [GpuFormatR16f] = { SDL_GPU_TEXTUREFORMAT_R16_FLOAT, false },
    [GpuFormatRg16f] = { SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT, false },
    [GpuFormatRgba16f] = { SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, false },
    [GpuFormatR32f] = { SDL_GPU_TEXTUREFORMAT_R32_FLOAT, false },
    [GpuFormatRg32f] = { SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT, false },
    [GpuFormatRgba32f] = { SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT, false },
    [GpuFormatR32u] = { SDL_GPU_TEXTUREFORMAT_R32_UINT, false },
    [GpuFormatRg11b10f] = { SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT, false },
    [GpuFormatD16] = { SDL_GPU_TEXTUREFORMAT_D16_UNORM, true },
    [GpuFormatD24S8] = { SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT, true },
    [GpuFormatD32f] = { SDL_GPU_TEXTUREFORMAT_D32_FLOAT, true },
    [GpuFormatD32fS8] = { SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT, true },
};

SDL_GPUTextureFormat gpu_sdl_format(uint8_t format) {
    return formats[format < GpuFormatCount ? format : GpuFormatInvalid].sdl;
}

/* SDL asserts on an invalid format, so it never sees one. */
uint32_t gpu_format_size(uint8_t format) {
    const SDL_GPUTextureFormat sdl = gpu_sdl_format(format);
    return sdl == SDL_GPU_TEXTUREFORMAT_INVALID ? 0 : SDL_GPUTextureFormatTexelBlockSize(sdl);
}

bool gpu_format_depth(uint8_t format) {
    return format < GpuFormatCount && formats[format].depth;
}

void gpu_counted_create(const void *object, const char *what) {
    GpuStats *stats = gpu_stats();
    if (object) {
        stats->created++;
        stats->objects++;
    } else {
        stats->create_failures++;
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: %s creation: %s", what, SDL_GetError());
    }
}

static void *created(void *object, const char *what) {
    gpu_counted_create(object, what);
    return object;
}

uint32_t sigpu_max_samples(uint8_t format, uint32_t wanted) {
    const SDL_GPUTextureFormat sdl = gpu_sdl_format(format);
    if (!gpu_session.gpu || sdl == SDL_GPU_TEXTUREFORMAT_INVALID) {
        return 1;
    }
    for (uint32_t samples = wanted >= 8 ? 8 : wanted >= 4 ? 4 : wanted >= 2 ? 2 : 1; samples > 1;
         samples /= 2) {
        if (SDL_GPUTextureSupportsSampleCount(gpu_session.gpu, sdl, gpu_sample_count(samples))) {
            return samples;
        }
    }
    return 1;
}

SDL_GPUSampleCount gpu_sample_count(uint32_t samples) {
    return samples >= 8   ? SDL_GPU_SAMPLECOUNT_8
           : samples >= 4 ? SDL_GPU_SAMPLECOUNT_4
           : samples >= 2 ? SDL_GPU_SAMPLECOUNT_2
                          : SDL_GPU_SAMPLECOUNT_1;
}

SDL_GPUBuffer *gpu_create_buffer_flags(uint32_t size, SDL_GPUBufferUsageFlags flags) {
    return created(
        SDL_CreateGPUBuffer(gpu_session.gpu,
                            &(SDL_GPUBufferCreateInfo){ .usage = flags, .size = size }),
        "buffer"
    );
}

void gpu_release_buffer(SDL_GPUBuffer *buffer) {
    if (buffer) {
        gpu_counted_release(buffer);
        SDL_ReleaseGPUBuffer(gpu_session.gpu, buffer);
    }
}

SDL_GPUBuffer *gpu_create_buffer(uint32_t size, uint32_t usage) {
    SDL_GPUBufferUsageFlags flags = 0;
    flags |= usage & GpuBufferVertex ? SDL_GPU_BUFFERUSAGE_VERTEX : 0;
    flags |= usage & GpuBufferIndex ? SDL_GPU_BUFFERUSAGE_INDEX : 0;
    flags |= usage & GpuBufferIndirect ? SDL_GPU_BUFFERUSAGE_INDIRECT : 0;
    flags |= usage & GpuBufferGraphicsRead ? SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ : 0;
    flags |= usage & GpuBufferComputeRead ? SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ : 0;
    flags |= usage & GpuBufferComputeWrite ? SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE : 0;
    return gpu_create_buffer_flags(size, flags);
}

SDL_GPUTexture *gpu_create_texture(const gpu_texture_info_t *info) {
    static const SDL_GPUTextureType types[] = {
        [GpuTexture2d] = SDL_GPU_TEXTURETYPE_2D,
        [GpuTexture2dArray] = SDL_GPU_TEXTURETYPE_2D_ARRAY,
        [GpuTexture3d] = SDL_GPU_TEXTURETYPE_3D,
        [GpuTextureCube] = SDL_GPU_TEXTURETYPE_CUBE,
    };
    SDL_GPUTextureUsageFlags usage = 0;
    usage |= info->usage & GpuTextureSampled ? SDL_GPU_TEXTUREUSAGE_SAMPLER : 0;
    usage |= info->usage & GpuTextureColorTarget ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET : 0;
    usage |= info->usage & GpuTextureDepthTarget ? SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET : 0;
    usage |= info->usage & GpuTextureGraphicsRead ? SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ : 0;
    usage |= info->usage & GpuTextureComputeRead ? SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ : 0;
    usage |= info->usage & GpuTextureComputeWrite ? SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE : 0;
    const SDL_GPUSampleCount samples = gpu_sample_count(info->samples);
    return created(
        SDL_CreateGPUTexture(
            gpu_session.gpu,
            &(SDL_GPUTextureCreateInfo){
                .type = types[info->dimension],
                .format = gpu_sdl_format(info->format),
                .usage = usage,
                .width = info->width,
                .height = info->height,
                .layer_count_or_depth = info->depth,
                .num_levels = info->mips,
                .sample_count = samples,
            }
        ),
        "texture"
    );
}

SDL_GPUSampler *gpu_create_sampler(const GpuSampler *desc) {
    static const SDL_GPUSamplerAddressMode modes[] = {
        [GpuAddressRepeat] = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        [GpuAddressMirror] = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,
        [GpuAddressClamp] = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    static const SDL_GPUCompareOp compares[] = {
        [GpuCompareNone] = SDL_GPU_COMPAREOP_INVALID,
        [GpuCompareNever] = SDL_GPU_COMPAREOP_NEVER,
        [GpuCompareLess] = SDL_GPU_COMPAREOP_LESS,
        [GpuCompareEqual] = SDL_GPU_COMPAREOP_EQUAL,
        [GpuCompareLessEqual] = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
        [GpuCompareGreater] = SDL_GPU_COMPAREOP_GREATER,
        [GpuCompareNotEqual] = SDL_GPU_COMPAREOP_NOT_EQUAL,
        [GpuCompareGreaterEqual] = SDL_GPU_COMPAREOP_GREATER_OR_EQUAL,
        [GpuCompareAlways] = SDL_GPU_COMPAREOP_ALWAYS,
    };
    const bool anisotropic = desc->max_anisotropy > 1;
    return created(
        SDL_CreateGPUSampler(
            gpu_session.gpu,
            &(SDL_GPUSamplerCreateInfo){
                .min_filter = desc->min_filter ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST,
                .mag_filter = desc->mag_filter ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST,
                .mipmap_mode = desc->mip_filter ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
                                                : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
                .address_mode_u = modes[desc->address_u],
                .address_mode_v = modes[desc->address_v],
                .address_mode_w = modes[desc->address_w],
                .max_anisotropy = anisotropic ? (float)desc->max_anisotropy : 1.0f,
                .compare_op = compares[desc->compare],
                .max_lod = 1000.0f,
                .enable_anisotropy = anisotropic,
                .enable_compare = desc->compare != GpuCompareNone,
            }
        ),
        "sampler"
    );
}

SDL_GPUShader *gpu_create_shader(const gpu_pack_t *pack, const gpu_pack_entry_t *entry) {
    return created(
        SDL_CreateGPUShader(
            gpu_session.gpu,
            &(SDL_GPUShaderCreateInfo){
                .code_size = entry->code_size,
                .code = gpu_pack_code(pack, entry),
                .entrypoint = entry->name,
                .format = SDL_GPU_SHADERFORMAT_SPIRV,
                .stage = entry->stage == GpuPackVertex ? SDL_GPU_SHADERSTAGE_VERTEX
                                                       : SDL_GPU_SHADERSTAGE_FRAGMENT,
                .num_samplers = entry->counts[GpuPackSampler],
                .num_storage_textures = entry->counts[GpuPackStorageTexture],
                .num_storage_buffers = entry->counts[GpuPackStorageBuffer],
                .num_uniform_buffers = entry->counts[GpuPackUniformBuffer],
            }
        ),
        "shader"
    );
}

SDL_GPUComputePipeline *gpu_create_compute_pipeline(
    const gpu_pack_t *pack,
    const gpu_pack_entry_t *entry
) {
    return created(
        SDL_CreateGPUComputePipeline(
            gpu_session.gpu,
            &(SDL_GPUComputePipelineCreateInfo){
                .code_size = entry->code_size,
                .code = gpu_pack_code(pack, entry),
                .entrypoint = entry->name,
                .format = SDL_GPU_SHADERFORMAT_SPIRV,
                .num_samplers = entry->counts[GpuPackSampler],
                .num_readonly_storage_textures = entry->counts[GpuPackStorageTexture],
                .num_readonly_storage_buffers = entry->counts[GpuPackStorageBuffer],
                .num_readwrite_storage_textures = entry->counts[GpuPackRWStorageTexture],
                .num_readwrite_storage_buffers = entry->counts[GpuPackRWStorageBuffer],
                .num_uniform_buffers = entry->counts[GpuPackUniformBuffer],
                .threadcount_x = entry->threads[0],
                .threadcount_y = entry->threads[1],
                .threadcount_z = entry->threads[2],
            }
        ),
        "compute pipeline"
    );
}

void gpu_counted_release(const void *object) {
    if (object) {
        GpuStats *stats = gpu_stats();
        stats->released++;
        stats->objects--;
    }
}

#define released gpu_counted_release

/* SDL keeps a released object alive until the submissions that use it
 * completed, so the release is immediate. */
void gpu_record_release(gpu_record_t *record) {
    SDL_GPUDevice *gpu = gpu_session.gpu;
    switch (record->kind) {
    case GPU_RESOURCE_BUFFER:
        released(record->as.buffer.buffer);
        SDL_ReleaseGPUBuffer(gpu, record->as.buffer.buffer);
        break;
    case GPU_RESOURCE_TEXTURE:
        released(record->as.texture.texture);
        SDL_ReleaseGPUTexture(gpu, record->as.texture.texture);
        break;
    case GPU_RESOURCE_SAMPLER:
        released(record->as.sampler);
        SDL_ReleaseGPUSampler(gpu, record->as.sampler);
        break;
    case GPU_RESOURCE_SHADER:
        gpu_pack_close(&record->as.pack);
        break;
    case GPU_RESOURCE_PROGRAM:
        for (uint32_t i = 0; i < GPU_PROGRAM_STAGES; i++) {
            gpu_program_stage_t *stage = &record->as.program.stages[i];
            released(stage->vertex);
            released(stage->fragment);
            if (stage->vertex) {
                SDL_ReleaseGPUShader(gpu, stage->vertex);
            }
            if (stage->fragment) {
                SDL_ReleaseGPUShader(gpu, stage->fragment);
            }
            stage->vertex = stage->fragment = NULL;
        }
        released(record->as.program.compute);
        if (record->as.program.compute) {
            SDL_ReleaseGPUComputePipeline(gpu, record->as.program.compute);
        }
        record->as.program.compute = NULL;
        break;
    case GPU_RESOURCE_SURFACE:
        gpu_surface_release_window(record);
        break;
    default:
        break;
    }
    record->kind = GPU_RESOURCE_NONE;
}

/* SDL GPU needs the video subsystem even without a window (RFC section 11).
 * Without one, it is started on the offscreen driver; the hint is restored
 * so a later siwindow import picks the real driver. */
static bool start_video(void) {
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        return true;
    }
    const bool hinted = SDL_GetHint(SDL_HINT_VIDEO_DRIVER) != NULL;
    if (!hinted) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    }
    const bool started = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (!hinted) {
        SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
    }
    gpu_session.video_owned = started;
    return started;
}

bool gpu_open_device(bool debug) {
    if (!start_video()) {
        return false;
    }
    gpu_session.gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug, NULL);
    if (!gpu_session.gpu && gpu_session.video_owned) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        gpu_session.video_owned = false;
    }
    return gpu_session.gpu != NULL;
}

SDL_GPUSampler *gpu_default_sampler(void) {
    if (!gpu_session.default_sampler && gpu_session.gpu) {
        gpu_session.default_sampler = gpu_create_sampler(&(GpuSampler){
            .min_filter = GpuFilterLinear, .mag_filter = GpuFilterLinear,
            .address_u = GpuAddressClamp, .address_v = GpuAddressClamp,
            .address_w = GpuAddressClamp });
    }
    return gpu_session.default_sampler;
}

/* Uploaded once with its own submission: SDL orders it before any later
 * command buffer that samples it. */
SDL_GPUTexture *gpu_default_texture(void) {
    if (gpu_session.default_texture || !gpu_session.gpu || gpu_session.lost) {
        return gpu_session.default_texture;
    }
    const gpu_texture_info_t info = { .format = GpuFormatRgba8, .mips = 1, .samples = 1,
                                      .usage = GpuTextureSampled, .width = 1, .height = 1,
                                      .depth = 1 };
    SDL_GPUTexture *texture = gpu_create_texture(&info);
    SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(
        gpu_session.gpu,
        &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = 4 });
    uint32_t *mapped = staging ? SDL_MapGPUTransferBuffer(gpu_session.gpu, staging, false) : NULL;
    SDL_GPUCommandBuffer *cmd = mapped ? SDL_AcquireGPUCommandBuffer(gpu_session.gpu) : NULL;
    if (!texture || !cmd) {
        if (mapped) {
            SDL_UnmapGPUTransferBuffer(gpu_session.gpu, staging);
        }
        SDL_ReleaseGPUTransferBuffer(gpu_session.gpu, staging);
        if (texture) {
            gpu_counted_release(texture);
            SDL_ReleaseGPUTexture(gpu_session.gpu, texture);
        }
        return NULL;
    }
    *mapped = 0xffffffffu;
    SDL_UnmapGPUTransferBuffer(gpu_session.gpu, staging);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_UploadToGPUTexture(copy, &(SDL_GPUTextureTransferInfo){ .transfer_buffer = staging },
                           &(SDL_GPUTextureRegion){ .texture = texture, .w = 1, .h = 1, .d = 1 },
                           false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(gpu_session.gpu, staging);
    gpu_session.default_texture = texture;
    return texture;
}

void gpu_close_device(void) {
    if (!gpu_session.gpu) {
        return;
    }
    if (gpu_session.default_texture) {
        gpu_counted_release(gpu_session.default_texture);
        SDL_ReleaseGPUTexture(gpu_session.gpu, gpu_session.default_texture);
        gpu_session.default_texture = NULL;
    }
    if (gpu_session.default_sampler) {
        gpu_counted_release(gpu_session.default_sampler);
        SDL_ReleaseGPUSampler(gpu_session.gpu, gpu_session.default_sampler);
        gpu_session.default_sampler = NULL;
    }
    if (gpu_session.cmd) {
        SDL_CancelGPUCommandBuffer(gpu_session.cmd);
        gpu_session.cmd = NULL;
    }
    if (!gpu_session.lost) {
        SDL_WaitForGPUIdle(gpu_session.gpu);
    }
    for (uint32_t i = 0; i < GPU_MAX_FRAMES_IN_FLIGHT; i++) {
        if (gpu_session.fences[i]) {
            SDL_ReleaseGPUFence(gpu_session.gpu, gpu_session.fences[i]);
            gpu_session.fences[i] = NULL;
        }
    }
    SDL_DestroyGPUDevice(gpu_session.gpu);
    gpu_session.gpu = NULL;
    if (gpu_session.video_owned) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        gpu_session.video_owned = false;
    }
}
