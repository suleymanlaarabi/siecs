#include "util.h"
#include "test_pack.generated.h"
#include <stdlib.h>
#include <string.h>

ECS_COMPONENT_DEFINE(Quad);
ECS_COMPONENT_DEFINE(QuadInstance);
ECS_COMPONENT_DEFINE(QuadMaterial, .inheritance = EcsInheritShared);
ECS_TAG_DEFINE(QuadLayer);
ECS_COMPONENT_DEFINE(QuadOrder);

void gpu_test_start(uint32_t budget) {
    ecs_init();
    ECS_MODULE_IMPORT(sigpu, { .budget = budget });
    ECS_COMPONENT_REGISTER(Quad, QuadInstance, QuadMaterial, QuadLayer, QuadOrder);
    test_true(sigpu_asset_register(TEST_PACK, test_gpushader, sizeof(test_gpushader)));
}

const unsigned char *gpu_test_pack(size_t *size) {
    *size = sizeof(test_gpushader);
    return test_gpushader;
}

ecs_entity_t gpu_test_device(uint8_t frames_in_flight) {
    ecs_entity_t device = ecs_new();
    ecs_set(device, GpuDevice, { .debug = true, .frames_in_flight = frames_in_flight });
    return device;
}

ecs_entity_t gpu_test_buffer(uint32_t size) {
    ecs_entity_t buffer = ecs_new();
    ecs_set(buffer, GpuBuffer, { .size = size, .usage = GpuBufferVertex });
    return buffer;
}

ecs_entity_t gpu_test_texture(uint32_t width, uint32_t height) {
    ecs_entity_t texture = ecs_new();
    ecs_set(texture, GpuTexture, {
        .format = GpuFormatRgba8,
        .usage = GpuTextureSampled | GpuTextureColorTarget,
        .width = width,
        .height = height,
    });
    return texture;
}

ecs_entity_t gpu_test_shader(const char *path) {
    ecs_entity_t shader = ecs_new();
    GpuShader desc = { 0 };
    SDL_strlcpy(desc.path, path, sizeof(desc.path));
    ecs_set_cid(shader, ecs_id(GpuShader), &desc);
    return shader;
}

ecs_entity_t gpu_test_compute(ecs_entity_t shader, const char *entry) {
    ecs_entity_t program = ecs_new();
    GpuProgram desc = { .shader = shader };
    SDL_strlcpy(desc.compute, entry, sizeof(desc.compute));
    ecs_set_cid(program, ecs_id(GpuProgram), &desc);
    return program;
}

void gpu_test_frames(uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        ecs_progress();
    }
}

const GpuResourceState *gpu_test_state(ecs_entity_t resource) {
    return ecs_get(resource, GpuResourceState);
}

const GpuStats *gpu_test_stats(void) { return ecs_get_resource_read(GpuStats); }

const GpuSession *gpu_test_session(void) { return ecs_get_resource_read(GpuSession); }

typedef void (*download_fn)(SDL_GPUCopyPass *copy, SDL_GPUTransferBuffer *transfer, void *ctx);

static bool download(uint32_t size, download_fn record, void *ctx, void *out) {
    SDL_GPUDevice *gpu = gpu_session_device();
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        gpu,
        &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                            .size = size }
    );
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu);
    if (!transfer || !cmd) {
        return false;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    record(copy, transfer, ctx);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    const bool done = fence && SDL_WaitForGPUFences(gpu, true, &fence, 1);
    SDL_ReleaseGPUFence(gpu, fence);
    const void *mapped = done ? SDL_MapGPUTransferBuffer(gpu, transfer, false) : NULL;
    if (mapped) {
        memcpy(out, mapped, size);
        SDL_UnmapGPUTransferBuffer(gpu, transfer);
    }
    SDL_ReleaseGPUTransferBuffer(gpu, transfer);
    return mapped != NULL;
}

typedef struct {
    SDL_GPUBuffer *buffer;
    uint32_t offset;
    uint32_t size;
} buffer_read_t;

static void record_buffer(SDL_GPUCopyPass *copy, SDL_GPUTransferBuffer *transfer, void *ctx) {
    const buffer_read_t *read = ctx;
    SDL_DownloadFromGPUBuffer(
        copy,
        &(SDL_GPUBufferRegion){ .buffer = read->buffer, .offset = read->offset, .size = read->size },
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = transfer }
    );
}

bool gpu_test_read_buffer(ecs_entity_t buffer, uint32_t offset, void *out, uint32_t size) {
    buffer_read_t read = { gpu_resource_buffer(buffer), offset, size };
    return read.buffer && download(size, record_buffer, &read, out);
}

typedef struct {
    SDL_GPUTexture *texture;
    uint32_t width;
    uint32_t height;
} texture_read_t;

static void record_texture(SDL_GPUCopyPass *copy, SDL_GPUTransferBuffer *transfer, void *ctx) {
    const texture_read_t *read = ctx;
    SDL_DownloadFromGPUTexture(
        copy,
        &(SDL_GPUTextureRegion){ .texture = read->texture, .w = read->width, .h = read->height,
                                 .d = 1 },
        &(SDL_GPUTextureTransferInfo){ .transfer_buffer = transfer }
    );
}

bool gpu_test_read_texture(ecs_entity_t texture, uint32_t width, uint32_t height, void *out) {
    gpu_texture_info_t info;
    texture_read_t read = { gpu_resource_texture(texture, &info), width, height };
    const uint32_t size =
        SDL_CalculateGPUTextureFormatSize(gpu_sdl_format(info.format), width, height, 1);
    return read.texture && download(size, record_texture, &read, out);
}

bool gpu_test_read_raw(SDL_GPUBuffer *buffer, uint32_t offset, void *out, uint32_t size) {
    buffer_read_t read = { buffer, offset, size };
    return buffer && download(size, record_buffer, &read, out);
}

uint32_t gpu_test_pixel(ecs_entity_t texture, uint32_t x, uint32_t y) {
    gpu_texture_info_t info;
    if (!gpu_resource_texture(texture, &info)) {
        return 0xdeadbeef;
    }
    uint32_t *pixels = malloc((size_t)info.width * info.height * 4);
    uint32_t pixel = 0xdeadbeef;
    if (pixels && gpu_test_read_texture(texture, info.width, info.height, pixels)) {
        pixel = pixels[y * info.width + x];
    }
    free(pixels);
    return pixel;
}

ecs_entity_t gpu_test_graphics(ecs_entity_t shader, const char *vertex, const char *fragment) {
    ecs_entity_t program = ecs_new();
    GpuProgram desc = { .shader = shader };
    SDL_strlcpy(desc.vertex, vertex, sizeof(desc.vertex));
    SDL_strlcpy(desc.fragment, fragment, sizeof(desc.fragment));
    ecs_set_cid(program, ecs_id(GpuProgram), &desc);
    return program;
}

ecs_entity_t gpu_test_target(uint32_t width, uint32_t height) {
    ecs_entity_t texture = ecs_new();
    ecs_set(texture, GpuTexture, {
        .format = GpuFormatRgba8,
        .usage = GpuTextureSampled | GpuTextureColorTarget,
        .width = width,
        .height = height,
    });
    return texture;
}

ecs_entity_t gpu_test_depth(uint32_t width, uint32_t height) {
    ecs_entity_t texture = ecs_new();
    ecs_set(texture, GpuTexture, {
        .format = GpuFormatD32f,
        .usage = GpuTextureDepthTarget | GpuTextureSampled,
        .width = width,
        .height = height,
    });
    return texture;
}

ecs_entity_t gpu_test_quad_geometry(void) {
    const float corners[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
    ecs_entity_t vertices = ecs_new();
    ecs_set(vertices, GpuBuffer, { .size = sizeof(corners), .usage = GpuBufferVertex });
    sigpu_buffer_upload(vertices, 0, corners, sizeof(corners));
    ecs_entity_t geometry = ecs_new();
    ecs_set(geometry, GpuGeometry, {
        .vertices = vertices,
        .stride = sizeof(corners[0]),
        .lod_count = 1,
        .attribute_count = 1,
        .attributes = { { .name = "corner", .format = GpuVertexFloat2 } },
        .lods = { { .index_count = 6 } },
        .bounds = { 0.5f, 0.5f, 0.0f, 0.75f },
    });
    return geometry;
}

ecs_entity_t gpu_test_sampler(void) {
    ecs_entity_t sampler = ecs_new();
    ecs_set(sampler, GpuSampler, { .address_u = GpuAddressClamp, .address_v = GpuAddressClamp });
    return sampler;
}

ecs_entity_t gpu_test_color_texture(uint32_t rgba) {
    ecs_entity_t texture = ecs_new();
    ecs_set(texture, GpuTexture, { .format = GpuFormatRgba8, .usage = GpuTextureSampled,
                                   .width = 1, .height = 1 });
    sigpu_texture_upload(texture, &(GpuTextureRegion){ .width = 1, .height = 1, .depth = 1 },
                         &rgba, 4, 0);
    return texture;
}

static void extract_quads(ecs_iter_t *it) {
    const Quad *quads = ecs_field(it, 0);
    QuadInstance *out = sigpu_instances(it);
    GpuSphere *bounds = sigpu_bounds(it);
    uint32_t *keys = sigpu_order_keys(it);
    const QuadOrder *order = it->ptrs[1];
    for (uint32_t i = 0; i < it->count; i++) {
        memcpy(&out[i], &quads[i], sizeof(QuadInstance));
        if (bounds) {
            const float w = quads[i].size[0], h = quads[i].size[1];
            bounds[i] = (GpuSphere){ quads[i].offset[0] + w * 0.5f, quads[i].offset[1] + h * 0.5f,
                                     0.5f, 0.5f * SDL_sqrtf(w * w + h * h) };
        }
        if (keys) {
            keys[i] = order ? order[i].key : 0;
        }
    }
}

ecs_entity_t gpu_test_quad_feature(const char *name, ecs_entity_t geometry, ecs_entity_t program,
                                   uint8_t ordering, bool culling) {
    return sigpu_feature(&(GpuFeatureDesc){
        .name = name,
        .instance = ecs_id(QuadInstance),
        .query = { .components = { ecs_in_changed(Quad),
                                   { ecs_id(QuadOrder), EcsInOptional | ECS_TERM_CHANGED },
                                   ordering == GpuOrderLayered ? ecs_filter(QuadLayer)
                                                               : ecs_not(QuadLayer) } },
        .extract = extract_quads,
        .geometry = geometry,
        .program = program,
        .ordering = ordering,
        .culling = culling ? GpuCullSphere : GpuCullNone,
        .chunk_size = 64,
    });
}

ecs_entity_t gpu_test_quad(float x, float y, float w, float h, uint32_t rgba) {
    ecs_entity_t quad = ecs_new();
    ecs_set(quad, Quad, { .offset = { x, y }, .size = { w, h },
                          .color = { (rgba & 0xff) / 255.0f, ((rgba >> 8) & 0xff) / 255.0f,
                                     ((rgba >> 16) & 0xff) / 255.0f, (rgba >> 24) / 255.0f } });
    return quad;
}

void gpu_test_tint(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    (void)user_data;
    const float tint[4] = { 1, 1, 1, 1 };
    gpu_cmd_uniform(ctx, "view_tint", tint, sizeof(tint));
}

ecs_entity_t gpu_test_draw_graph(ecs_entity_t target, ecs_entity_t phase, ecs_entity_t view,
                                 ecs_entity_t *pass) {
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    *pass = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender, .order = GPU_ORDER_SCENE,
                                              .phase = phase, .view = view,
                                              .execute = gpu_test_tint });
    const float black[4] = { 0, 0, 0, 1 };
    sigpu_pass_color(*pass, target, GpuLoadClear, GpuStoreStore, black, 0);
    sigpu_graph_export(graph, target);
    return graph;
}

/* A target, a quad feature drawn by quad_plain in phase "opaque", and a
 * graph drawing that phase into the target. */
scene_t gpu_test_scene(uint8_t ordering, bool culling, ecs_entity_t view) {
    scene_t s = { 0 };
    gpu_test_device(0);
    s.target = gpu_test_target(16, 16);
    s.phase = ecs_new_named(0, "opaque");
    ecs_entity_t shader = gpu_test_shader(TEST_PACK);
    s.program = ecs_new();
    ecs_set(s.program, GpuProgram, { .shader = shader, .vertex = "quad_vertex",
                                     .fragment = "quad_plain", .phase = s.phase });
    s.geometry = gpu_test_quad_geometry();
    s.feature = gpu_test_quad_feature("test::quads", s.geometry, s.program, ordering, culling);
    s.graph = gpu_test_draw_graph(s.target, s.phase, view, &s.pass);
    return s;
}



ecs_entity_t gpu_test_view(float shift_x) {
    ecs_entity_t view = ecs_new();
    GpuView desc = { .view_projection = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, shift_x, 0, 0, 1 },
                     .forward = { 0, 0, 1 }, .lod_scale = 8.0f, .orthographic = true };
    ecs_set_cid(view, ecs_id(GpuView), &desc);
    return view;
}

