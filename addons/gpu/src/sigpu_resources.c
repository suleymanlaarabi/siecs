#include "sigpu_internal.h"

#include <string.h>

static const sigpu_mesh_vertex_t cube_vertices[24] = {
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

typedef struct {
    sigpu_mesh_vertex_t *vertices;
    Uint16 *indices;
    Uint32 vertex_count, index_count;
} mesh_builder_t;

static void
add_vertex(mesh_builder_t *builder, float x, float y, float z, float nx, float ny, float nz) {
    builder->vertices[builder->vertex_count++] = (sigpu_mesh_vertex_t){ x,
                                                                        y,
                                                                        z,
                                                                        (int8_t)roundf(nx * 127.0f),
                                                                        (int8_t)roundf(ny * 127.0f),
                                                                        (int8_t)roundf(nz * 127.0f),
                                                                        0 };
}

static void add_triangle(mesh_builder_t *builder, Uint16 a, Uint16 b, Uint16 c) {
    /* Match the cube winding used by the back-face-culling pipelines. */
    builder->indices[builder->index_count++] = a;
    builder->indices[builder->index_count++] = c;
    builder->indices[builder->index_count++] = b;
}

static void add_cylinder(mesh_builder_t *builder, Uint32 segments) {
    const Uint16 side = (Uint16)builder->vertex_count;
    for (Uint32 i = 0; i <= segments; i++) {
        float angle = 2.0f * SIGPU_PI * i / segments;
        float x = cosf(angle), z = sinf(angle);
        add_vertex(builder, x * 0.5f, -0.5f, z * 0.5f, x, 0, z);
        add_vertex(builder, x * 0.5f, 0.5f, z * 0.5f, x, 0, z);
    }
    for (Uint32 i = 0; i < segments; i++) {
        Uint16 n = side + (Uint16)(2 * i);
        add_triangle(builder, n, n + 1, n + 3);
        add_triangle(builder, n, n + 3, n + 2);
    }
    for (int cap = 0; cap < 2; cap++) {
        float y = cap == 0 ? 0.5f : -0.5f;
        float ny = cap == 0 ? 1.0f : -1.0f;
        Uint16 center = (Uint16)builder->vertex_count;
        add_vertex(builder, 0, y, 0, 0, ny, 0);
        for (Uint32 i = 0; i < segments; i++) {
            float angle = 2.0f * SIGPU_PI * i / segments;
            add_vertex(builder, cosf(angle) * 0.5f, y, sinf(angle) * 0.5f, 0, ny, 0);
        }
        for (Uint32 i = 0; i < segments; i++) {
            Uint16 a = center + 1 + (Uint16)i;
            Uint16 b = center + 1 + (Uint16)((i + 1) % segments);
            if (cap == 0)
                add_triangle(builder, center, b, a);
            else
                add_triangle(builder, center, a, b);
        }
    }
}

static void add_sphere(mesh_builder_t *builder, Uint32 slices, Uint32 stacks) {
    Uint16 base = (Uint16)builder->vertex_count;
    for (Uint32 row = 0; row <= stacks; row++) {
        float phi = SIGPU_PI * row / stacks;
        float y = cosf(phi), ring = sinf(phi);
        for (Uint32 col = 0; col <= slices; col++) {
            float angle = 2.0f * SIGPU_PI * col / slices;
            float x = ring * cosf(angle), z = ring * sinf(angle);
            add_vertex(builder, x * 0.5f, y * 0.5f, z * 0.5f, x, y, z);
        }
    }
    for (Uint32 row = 0; row < stacks; row++) {
        for (Uint32 col = 0; col < slices; col++) {
            Uint16 a = base + (Uint16)(row * (slices + 1) + col);
            Uint16 b = a + (Uint16)(slices + 1);
            if (row > 0)
                add_triangle(builder, a, a + 1, b);
            if (row + 1 < stacks)
                add_triangle(builder, a + 1, b + 1, b);
        }
    }
}

static void create_mesh_atlas(void) {
    /* Upper bounds cover all seven meshes; only the populated portion is uploaded. */
    mesh_builder_t builder = {
        .vertices = SDL_malloc(2048 * sizeof(sigpu_mesh_vertex_t)),
        .indices = SDL_malloc(8192 * sizeof(Uint16)),
    };
    const Uint32 cylinder_segments[3] = { 8, 12, 24 };
    const Uint32 sphere_slices[3] = { 8, 12, 32 };
    const Uint32 sphere_stacks[3] = { 4, 6, 16 };
    for (Uint32 mesh_id = 0; mesh_id < SIGPU_MESH_COUNT; mesh_id++) {
        sigpu_mesh_t *mesh = &g_sigpu.meshes[mesh_id];
        mesh->first_index = builder.index_count;
        mesh->vertex_offset = (Sint32)builder.vertex_count;
        if (mesh_id == SIGPU_MESH_CUBE) {
            memcpy(builder.vertices + builder.vertex_count, cube_vertices, sizeof(cube_vertices));
            memcpy(builder.indices + builder.index_count, cube_indices, sizeof(cube_indices));
            builder.vertex_count += SDL_arraysize(cube_vertices);
            builder.index_count += SDL_arraysize(cube_indices);
        } else if (mesh_id <= SIGPU_MESH_CYLINDER_HIGH) {
            add_cylinder(&builder, cylinder_segments[mesh_id - SIGPU_MESH_CYLINDER_LOW]);
        } else {
            Uint32 lod = mesh_id - SIGPU_MESH_SPHERE_LOW;
            add_sphere(&builder, sphere_slices[lod], sphere_stacks[lod]);
        }
        mesh->index_count = builder.index_count - mesh->first_index;
        /* Generated indices are relative to each mesh's vertex_offset. */
        if (mesh_id != SIGPU_MESH_CUBE) {
            for (Uint32 i = mesh->first_index; i < builder.index_count; i++)
                builder.indices[i] -= (Uint16)mesh->vertex_offset;
        }
    }
    Uint32 vertex_size = builder.vertex_count * sizeof(sigpu_mesh_vertex_t);
    Uint32 index_size = builder.index_count * sizeof(Uint16);
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        g_sigpu.device,
        &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                            .size = vertex_size + index_size }
    );
    g_sigpu.vertex_buffer = SDL_CreateGPUBuffer(
        g_sigpu.device,
        &(SDL_GPUBufferCreateInfo){ .usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = vertex_size }
    );
    g_sigpu.index_buffer = SDL_CreateGPUBuffer(
        g_sigpu.device,
        &(SDL_GPUBufferCreateInfo){ .usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = index_size }
    );
    uint8_t *data = SDL_MapGPUTransferBuffer(g_sigpu.device, transfer, false);
    memcpy(data, builder.vertices, vertex_size);
    memcpy(data + vertex_size, builder.indices, index_size);
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
    SDL_free(builder.vertices);
    SDL_free(builder.indices);
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
    Uint32 transfer_size = 0;
    for (Uint32 primitive = 0; primitive < SIGPU_PRIMITIVE_COUNT; primitive++) {
        if (g_sigpu.static_axis_buffer[primitive]) {
            SDL_ReleaseGPUBuffer(g_sigpu.device, g_sigpu.static_axis_buffer[primitive]);
            g_sigpu.static_axis_buffer[primitive] = NULL;
        }
        if (g_sigpu.static_rotated_buffer[primitive]) {
            SDL_ReleaseGPUBuffer(g_sigpu.device, g_sigpu.static_rotated_buffer[primitive]);
            g_sigpu.static_rotated_buffer[primitive] = NULL;
        }
        transfer_size += upload->axis_count[primitive] * sizeof(sigpu_axis_instance_t);
        transfer_size += upload->rotated_count[primitive] * sizeof(sigpu_rotated_instance_t);
        if (upload->axis_count[primitive])
            g_sigpu.static_axis_buffer[primitive] =
                create_static_buffer(upload->axis_count[primitive] * sizeof(sigpu_axis_instance_t));
        if (upload->rotated_count[primitive])
            g_sigpu.static_rotated_buffer[primitive] = create_static_buffer(
                upload->rotated_count[primitive] * sizeof(sigpu_rotated_instance_t)
            );
    }
    if (transfer_size) {
        SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
            g_sigpu.device,
            &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                .size = transfer_size }
        );
        uint8_t *mapped = SDL_MapGPUTransferBuffer(g_sigpu.device, transfer, false);
        Uint32 offset = 0;
        for (Uint32 primitive = 0; primitive < SIGPU_PRIMITIVE_COUNT; primitive++) {
            Uint32 axis_size = upload->axis_count[primitive] * sizeof(sigpu_axis_instance_t);
            Uint32 rotated_size =
                upload->rotated_count[primitive] * sizeof(sigpu_rotated_instance_t);
            if (axis_size)
                memcpy(mapped + offset, upload->axis[primitive], axis_size);
            offset += axis_size;
            if (rotated_size)
                memcpy(mapped + offset, upload->rotated[primitive], rotated_size);
            offset += rotated_size;
        }
        SDL_UnmapGPUTransferBuffer(g_sigpu.device, transfer);
        SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(g_sigpu.device);
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command_buffer);
        offset = 0;
        for (Uint32 primitive = 0; primitive < SIGPU_PRIMITIVE_COUNT; primitive++) {
            Uint32 axis_size = upload->axis_count[primitive] * sizeof(sigpu_axis_instance_t);
            Uint32 rotated_size =
                upload->rotated_count[primitive] * sizeof(sigpu_rotated_instance_t);
            upload_static_buffer(
                copy,
                transfer,
                g_sigpu.static_axis_buffer[primitive],
                offset,
                axis_size
            );
            offset += axis_size;
            upload_static_buffer(
                copy,
                transfer,
                g_sigpu.static_rotated_buffer[primitive],
                offset,
                rotated_size
            );
            offset += rotated_size;
        }
        SDL_EndGPUCopyPass(copy);
        SDL_SubmitGPUCommandBuffer(command_buffer);
        SDL_ReleaseGPUTransferBuffer(g_sigpu.device, transfer);
    }
    SDL_free(g_sigpu.static_chunks);
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
            .format = g_sigpu.depth_format,
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
            !SDL_GPUTextureSupportsSampleCount(g_sigpu.device, g_sigpu.depth_format, selected))) {
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
    g_sigpu.depth_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    if (SDL_GPUTextureSupportsFormat(
            g_sigpu.device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
        ))
        g_sigpu.depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    else if (
        SDL_GPUTextureSupportsFormat(
            g_sigpu.device,
            SDL_GPU_TEXTUREFORMAT_D24_UNORM,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
        )
    )
        g_sigpu.depth_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    g_sigpu.sample_count = supported_sample_count(samples);
    create_mesh_atlas();
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
    for (Uint32 primitive = 0; primitive < SIGPU_PRIMITIVE_COUNT; primitive++) {
        if (g_sigpu.static_axis_buffer[primitive])
            SDL_ReleaseGPUBuffer(g_sigpu.device, g_sigpu.static_axis_buffer[primitive]);
        if (g_sigpu.static_rotated_buffer[primitive])
            SDL_ReleaseGPUBuffer(g_sigpu.device, g_sigpu.static_rotated_buffer[primitive]);
    }
    SDL_free(g_sigpu.owned_axis_batches);
    SDL_free(g_sigpu.owned_rotated_batches);
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
