#include "sigpu_internal.h"

#include <string.h>

static const sigpu_cube_vertex_t cube_vertices[24] = {
    { -0.5f, -0.5f, -0.5f, 0, 0, -127, 0 }, { 0.5f, -0.5f, -0.5f, 0, 0, -127, 0 },
    { 0.5f, 0.5f, -0.5f, 0, 0, -127, 0 },   { -0.5f, 0.5f, -0.5f, 0, 0, -127, 0 },
    { 0.5f, -0.5f, 0.5f, 0, 0, 127, 0 },    { -0.5f, -0.5f, 0.5f, 0, 0, 127, 0 },
    { -0.5f, 0.5f, 0.5f, 0, 0, 127, 0 },    { 0.5f, 0.5f, 0.5f, 0, 0, 127, 0 },
    { -0.5f, -0.5f, 0.5f, -127, 0, 0, 0 },  { -0.5f, -0.5f, -0.5f, -127, 0, 0, 0 },
    { -0.5f, 0.5f, -0.5f, -127, 0, 0, 0 },  { -0.5f, 0.5f, 0.5f, -127, 0, 0, 0 },
    { 0.5f, -0.5f, -0.5f, 127, 0, 0, 0 },   { 0.5f, -0.5f, 0.5f, 127, 0, 0, 0 },
    { 0.5f, 0.5f, 0.5f, 127, 0, 0, 0 },     { 0.5f, 0.5f, -0.5f, 127, 0, 0, 0 },
    { -0.5f, 0.5f, -0.5f, 0, 127, 0, 0 },   { 0.5f, 0.5f, -0.5f, 0, 127, 0, 0 },
    { 0.5f, 0.5f, 0.5f, 0, 127, 0, 0 },     { -0.5f, 0.5f, 0.5f, 0, 127, 0, 0 },
    { -0.5f, -0.5f, 0.5f, 0, -127, 0, 0 },  { 0.5f, -0.5f, 0.5f, 0, -127, 0, 0 },
    { 0.5f, -0.5f, -0.5f, 0, -127, 0, 0 },  { -0.5f, -0.5f, -0.5f, 0, -127, 0, 0 }
};

static const Uint16 cube_indices[36] = { 0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,
                                         8,  9,  10, 10, 11, 8,  12, 13, 14, 14, 15, 12,
                                         16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20 };

static void create_cube_mesh(void) {
    Uint32 vertex_size = sizeof(cube_vertices);
    Uint32 index_size = sizeof(cube_indices);
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        g_sigpu.device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = vertex_size + index_size,
        }
    );
    g_sigpu.vertex_buffer = SDL_CreateGPUBuffer(
        g_sigpu.device,
        &(SDL_GPUBufferCreateInfo){
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = vertex_size,
        }
    );
    g_sigpu.index_buffer = SDL_CreateGPUBuffer(
        g_sigpu.device,
        &(SDL_GPUBufferCreateInfo){
            .usage = SDL_GPU_BUFFERUSAGE_INDEX,
            .size = index_size,
        }
    );
    uint8_t *data = SDL_MapGPUTransferBuffer(g_sigpu.device, transfer, false);
    memcpy(data, cube_vertices, vertex_size);
    memcpy(data + vertex_size, cube_indices, index_size);
    SDL_UnmapGPUTransferBuffer(g_sigpu.device, transfer);
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(g_sigpu.device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command_buffer);
    SDL_UploadToGPUBuffer(
        copy,
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = transfer },
        &(SDL_GPUBufferRegion){ .buffer = g_sigpu.vertex_buffer, .size = vertex_size },
        false
    );
    SDL_UploadToGPUBuffer(
        copy,
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = transfer, .offset = vertex_size },
        &(SDL_GPUBufferRegion){ .buffer = g_sigpu.index_buffer, .size = index_size },
        false
    );
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(command_buffer);
    SDL_ReleaseGPUTransferBuffer(g_sigpu.device, transfer);
}

static void resize_instances(
    SDL_GPUBuffer **buffer,
    SDL_GPUTransferBuffer **transfer,
    Uint32 capacity,
    Uint32 stride
) {
    if (*buffer) {
        SDL_ReleaseGPUBuffer(g_sigpu.device, *buffer);
        SDL_ReleaseGPUTransferBuffer(g_sigpu.device, *transfer);
    }

    Uint32 size = stride * capacity;
    *buffer = SDL_CreateGPUBuffer(
        g_sigpu.device,
        &(SDL_GPUBufferCreateInfo){
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = size,
        }
    );
    *transfer = SDL_CreateGPUTransferBuffer(
        g_sigpu.device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = size,
        }
    );
}

static void grow_instances(
    SDL_GPUBuffer **buffer,
    SDL_GPUTransferBuffer **transfer,
    void **mapped,
    Uint32 *capacity,
    Uint32 shared_stride,
    Uint32 shared_count,
    Uint32 owned_stride,
    Uint32 owned_count
) {
    const Uint32 old_capacity = *capacity;
    const Uint32 new_capacity = old_capacity * 2;
    SDL_GPUBuffer *old_buffer = *buffer;
    SDL_GPUTransferBuffer *old_transfer = *transfer;
    void *old_mapped = *mapped;

    *buffer = NULL;
    *transfer = NULL;
    resize_instances(buffer, transfer, new_capacity, owned_stride);
    *mapped = SDL_MapGPUTransferBuffer(g_sigpu.device, *transfer, true);

    memcpy(*mapped, old_mapped, shared_count * shared_stride);
    memcpy(
        (uint8_t *)*mapped + (new_capacity - owned_count) * owned_stride,
        (uint8_t *)old_mapped + (old_capacity - owned_count) * owned_stride,
        owned_count * owned_stride
    );

    SDL_UnmapGPUTransferBuffer(g_sigpu.device, old_transfer);
    SDL_ReleaseGPUBuffer(g_sigpu.device, old_buffer);
    SDL_ReleaseGPUTransferBuffer(g_sigpu.device, old_transfer);
    *capacity = new_capacity;
}

static void resize_axis_instances(Uint32 capacity) {
    resize_instances(
        &g_sigpu.axis_buffer,
        &g_sigpu.axis_transfer,
        capacity,
        sizeof(sigpu_axis_instance_t)
    );
    g_sigpu.axis_capacity = capacity;
}

static void resize_rotated_instances(Uint32 capacity) {
    resize_instances(
        &g_sigpu.rotated_buffer,
        &g_sigpu.rotated_transfer,
        capacity,
        sizeof(sigpu_rotated_instance_t)
    );
    g_sigpu.rotated_capacity = capacity;
}

static SDL_GPUBuffer *create_static_buffer(Uint32 size) {
    return SDL_CreateGPUBuffer(
        g_sigpu.device,
        &(SDL_GPUBufferCreateInfo){
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = size,
        }
    );
}

static void upload_static_buffer(
    SDL_GPUCopyPass *copy,
    SDL_GPUTransferBuffer *transfer,
    SDL_GPUBuffer *buffer,
    Uint32 offset,
    Uint32 size
) {
    if (size == 0) {
        return;
    }
    SDL_UploadToGPUBuffer(
        copy,
        &(SDL_GPUTransferBufferLocation){
            .transfer_buffer = transfer,
            .offset = offset,
        },
        &(SDL_GPUBufferRegion){ .buffer = buffer, .size = size },
        false
    );
}

static void *copy_static_data(const void *source, Uint32 count, Uint32 stride) {
    if (count == 0) {
        return NULL;
    }
    void *copy = SDL_malloc(count * stride);
    memcpy(copy, source, count * stride);
    return copy;
}

void sigpu_static_upload(const sigpu_static_upload_t *upload) {
    const Uint32 axis_size = upload->axis_count * sizeof(sigpu_axis_instance_t);
    const Uint32 rotated_size = upload->rotated_count * sizeof(sigpu_rotated_instance_t);
    const Uint32 transfer_size = axis_size + rotated_size;

    if (axis_size) {
        g_sigpu.static_axis_buffer = create_static_buffer(axis_size);
    }
    if (rotated_size) {
        g_sigpu.static_rotated_buffer = create_static_buffer(rotated_size);
    }

    if (transfer_size) {
        SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
            g_sigpu.device,
            &(SDL_GPUTransferBufferCreateInfo){
                .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                .size = transfer_size,
            }
        );
        uint8_t *mapped = SDL_MapGPUTransferBuffer(g_sigpu.device, transfer, false);
        if (axis_size) {
            memcpy(mapped, upload->axis, axis_size);
        }
        if (rotated_size) {
            memcpy(mapped + axis_size, upload->rotated, rotated_size);
        }
        SDL_UnmapGPUTransferBuffer(g_sigpu.device, transfer);

        SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(g_sigpu.device);
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command_buffer);
        upload_static_buffer(copy, transfer, g_sigpu.static_axis_buffer, 0, axis_size);
        upload_static_buffer(
            copy,
            transfer,
            g_sigpu.static_rotated_buffer,
            axis_size,
            rotated_size
        );
        SDL_EndGPUCopyPass(copy);
        SDL_SubmitGPUCommandBuffer(command_buffer);
        SDL_ReleaseGPUTransferBuffer(g_sigpu.device, transfer);
    }

    g_sigpu.static_chunks =
        copy_static_data(upload->chunks, upload->chunk_count, sizeof(sigpu_static_chunk_t));
    g_sigpu.static_chunk_count = upload->chunk_count;
}

static SDL_GPUTexture *create_hdr_texture(
    Uint32 width,
    Uint32 height,
    SDL_GPUTextureUsageFlags usage,
    SDL_GPUSampleCount sample_count
) {
    return SDL_CreateGPUTexture(
        g_sigpu.device,
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
        SDL_ReleaseGPUTexture(g_sigpu.device, *texture);
        *texture = NULL;
    }
}

static void release_frame_targets(void) {
    release_texture(&g_sigpu.depth_texture);
    release_texture(&g_sigpu.msaa_texture);
    release_texture(&g_sigpu.bloom_msaa_texture);
    release_texture(&g_sigpu.scene_texture);
    release_texture(&g_sigpu.bloom_texture);
    release_texture(&g_sigpu.bloom_half);
    release_texture(&g_sigpu.bloom_half_scratch);
    release_texture(&g_sigpu.bloom_quarter);
    release_texture(&g_sigpu.bloom_quarter_scratch);

    g_sigpu.target_width = 0;
    g_sigpu.target_height = 0;
}

static void ensure_frame_targets(void) {
    if (g_sigpu.scene_texture && g_sigpu.target_width == g_sigpu.frame_width &&
        g_sigpu.target_height == g_sigpu.frame_height) {
        return;
    }

    release_frame_targets();
    g_sigpu.depth_texture = SDL_CreateGPUTexture(
        g_sigpu.device,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
            .width = g_sigpu.frame_width,
            .height = g_sigpu.frame_height,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .sample_count = g_sigpu.sample_count,
        }
    );
    g_sigpu.scene_texture = create_hdr_texture(
        g_sigpu.frame_width,
        g_sigpu.frame_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );

    if (g_sigpu.sample_count != SDL_GPU_SAMPLECOUNT_1) {
        g_sigpu.msaa_texture = create_hdr_texture(
            g_sigpu.frame_width,
            g_sigpu.frame_height,
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
            g_sigpu.sample_count
        );
        g_sigpu.bloom_msaa_texture = create_hdr_texture(
            g_sigpu.frame_width,
            g_sigpu.frame_height,
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
            g_sigpu.sample_count
        );
    }

    g_sigpu.bloom_texture = create_hdr_texture(
        g_sigpu.frame_width,
        g_sigpu.frame_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );
    g_sigpu.bloom_half_width = g_sigpu.frame_width > 1 ? g_sigpu.frame_width / 2 : 1;
    g_sigpu.bloom_half_height = g_sigpu.frame_height > 1 ? g_sigpu.frame_height / 2 : 1;
    g_sigpu.bloom_quarter_width = g_sigpu.bloom_half_width > 1 ? g_sigpu.bloom_half_width / 2 : 1;
    g_sigpu.bloom_quarter_height =
        g_sigpu.bloom_half_height > 1 ? g_sigpu.bloom_half_height / 2 : 1;
    g_sigpu.bloom_half = create_hdr_texture(
        g_sigpu.bloom_half_width,
        g_sigpu.bloom_half_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );
    g_sigpu.bloom_half_scratch = create_hdr_texture(
        g_sigpu.bloom_half_width,
        g_sigpu.bloom_half_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );
    g_sigpu.bloom_quarter = create_hdr_texture(
        g_sigpu.bloom_quarter_width,
        g_sigpu.bloom_quarter_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );
    g_sigpu.bloom_quarter_scratch = create_hdr_texture(
        g_sigpu.bloom_quarter_width,
        g_sigpu.bloom_quarter_height,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_SAMPLECOUNT_1
    );

    g_sigpu.target_width = g_sigpu.frame_width;
    g_sigpu.target_height = g_sigpu.frame_height;
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

static SDL_GPUSampleCount supported_sample_count(int samples) {
    SDL_GPUSampleCount selected = sample_count_from_int(samples);

    while (selected != SDL_GPU_SAMPLECOUNT_1 &&
           (!SDL_GPUTextureSupportsSampleCount(g_sigpu.device, SIGPU_HDR_FORMAT, selected) ||
            !SDL_GPUTextureSupportsSampleCount(
                g_sigpu.device,
                SDL_GPU_TEXTUREFORMAT_D16_UNORM,
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

void sigpu_resources_create(int samples) {
    g_sigpu.sample_count = supported_sample_count(samples);
    create_cube_mesh();
    resize_axis_instances(SIGPU_AXIS_CAPACITY);
    resize_rotated_instances(SIGPU_ROTATED_CAPACITY);
    sigpu_pipelines_create();
}

void sigpu_resources_destroy(void) {
    sigpu_pipelines_destroy();
    SDL_ReleaseGPUBuffer(g_sigpu.device, g_sigpu.vertex_buffer);
    SDL_ReleaseGPUBuffer(g_sigpu.device, g_sigpu.index_buffer);
    SDL_ReleaseGPUBuffer(g_sigpu.device, g_sigpu.axis_buffer);
    SDL_ReleaseGPUBuffer(g_sigpu.device, g_sigpu.rotated_buffer);
    if (g_sigpu.static_axis_buffer) {
        SDL_ReleaseGPUBuffer(g_sigpu.device, g_sigpu.static_axis_buffer);
    }
    if (g_sigpu.static_rotated_buffer) {
        SDL_ReleaseGPUBuffer(g_sigpu.device, g_sigpu.static_rotated_buffer);
    }
    SDL_ReleaseGPUTransferBuffer(g_sigpu.device, g_sigpu.axis_transfer);
    SDL_ReleaseGPUTransferBuffer(g_sigpu.device, g_sigpu.rotated_transfer);
    release_frame_targets();
    SDL_free(g_sigpu.static_chunks);
}

void sigpu_axis_instances_grow(void) {
    grow_instances(
        &g_sigpu.axis_buffer,
        &g_sigpu.axis_transfer,
        &g_sigpu.axis_mapped,
        &g_sigpu.axis_capacity,
        sizeof(sigpu_shared_axis_instance_t),
        g_sigpu.shared_axis_count,
        sizeof(sigpu_axis_instance_t),
        g_sigpu.owned_axis_count
    );
}

void sigpu_rotated_instances_grow(void) {
    grow_instances(
        &g_sigpu.rotated_buffer,
        &g_sigpu.rotated_transfer,
        &g_sigpu.rotated_mapped,
        &g_sigpu.rotated_capacity,
        sizeof(sigpu_shared_rotated_instance_t),
        g_sigpu.shared_rotated_count,
        sizeof(sigpu_rotated_instance_t),
        g_sigpu.owned_rotated_count
    );
}

void sigpu_frame_targets_prepare(void) { ensure_frame_targets(); }

void sigpu_sample_count_set(int samples) {
    SDL_GPUSampleCount selected = supported_sample_count(samples);

    if (selected != g_sigpu.sample_count) {
        g_sigpu.sample_count = selected;
        release_frame_targets();
        sigpu_main_pipelines_recreate();
    }
}
