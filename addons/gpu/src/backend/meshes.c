#include "backend/backend.h"
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

void sigpu_meshes_create(void) {
    /* Upper bounds cover all seven meshes; only the populated portion is uploaded. */
    mesh_builder_t builder = {
        .vertices = SDL_malloc(2048 * sizeof(sigpu_mesh_vertex_t)),
        .indices = SDL_malloc(8192 * sizeof(Uint16)),
    };
    const Uint32 cylinder_segments[3] = { 8, 12, 24 };
    const Uint32 sphere_slices[3] = { 8, 12, 32 };
    const Uint32 sphere_stacks[3] = { 4, 6, 16 };
    for (Uint32 mesh_id = 0; mesh_id < SIGPU_MESH_COUNT; mesh_id++) {
        sigpu_mesh_t *mesh = &SIGPU_GPUCONTEXT->meshes[mesh_id];
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
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                            .size = vertex_size + index_size }
    );
    SIGPU_GPUCONTEXT->vertex_buffer = SDL_CreateGPUBuffer(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUBufferCreateInfo){ .usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = vertex_size }
    );
    SIGPU_GPUCONTEXT->index_buffer = SDL_CreateGPUBuffer(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUBufferCreateInfo){ .usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = index_size }
    );
    uint8_t *data = SDL_MapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, transfer, false);
    memcpy(data, builder.vertices, vertex_size);
    memcpy(data + vertex_size, builder.indices, index_size);
    SDL_UnmapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, transfer);
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(SIGPU_GPUCONTEXT->device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command_buffer);
    SDL_UploadToGPUBuffer(
        copy,
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = transfer },
        &(SDL_GPUBufferRegion){ .buffer = SIGPU_GPUCONTEXT->vertex_buffer, .size = vertex_size },
        false
    );
    SDL_UploadToGPUBuffer(
        copy,
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = transfer, .offset = vertex_size },
        &(SDL_GPUBufferRegion){ .buffer = SIGPU_GPUCONTEXT->index_buffer, .size = index_size },
        false
    );
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(command_buffer);
    SDL_ReleaseGPUTransferBuffer(SIGPU_GPUCONTEXT->device, transfer);
    SDL_free(builder.vertices);
    SDL_free(builder.indices);
}
