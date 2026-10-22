#include "../../gpu/src/test_pack.generated.h"
#include <sigpu/cpp.hpp>
#include <SDL3/SDL.h>
#include <siecs/cpp/entity.hpp>
#include <siecs/cpp/world.hpp>
#include <bake_test.h>
#include <vector>

/* Same layout as the test pack's QuadInstance. */
struct Quad {
    float offset[2];
    float size[2];
    float color[4];
};

struct QuadInstance {
    float offset[2];
    float size[2];
    float color[4];
};

static uint32_t extracted_rows;

static ecs_entity_t scene(ecs_entity_t *target) {
    ecs::init();
    ecs::import<sigpu>();
    sigpu_asset_register("test.gpushader", test_gpushader, sizeof(test_gpushader));
    ecs::spawn().set(GpuDevice{ .debug = true });
    ecs_entity_t phase = ecs::spawn().id();
    GpuShader shader_desc{};
    SDL_strlcpy(shader_desc.path, "test.gpushader", sizeof(shader_desc.path));
    ecs_entity_t shader = ecs::spawn().set(shader_desc).id();
    GpuProgram program_desc{ .shader = shader, .phase = phase };
    SDL_strlcpy(program_desc.vertex, "quad_vertex", sizeof(program_desc.vertex));
    SDL_strlcpy(program_desc.fragment, "quad_plain", sizeof(program_desc.fragment));
    ecs_entity_t program = ecs::spawn().set(program_desc).id();
    const float corners[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
    ecs_entity_t vertices = ecs::spawn().set(GpuBuffer{ sizeof(corners), GpuBufferVertex }).id();
    sigpu_buffer_upload(vertices, 0, corners, sizeof(corners));
    GpuGeometry geometry_desc{ .vertices = vertices, .stride = sizeof(corners[0]), .lod_count = 1,
                               .attribute_count = 1 };
    SDL_strlcpy(geometry_desc.attributes[0].name, "corner", sizeof(geometry_desc.attributes[0].name));
    geometry_desc.attributes[0].format = GpuVertexFloat2;
    geometry_desc.lods[0].index_count = 6;
    ecs_entity_t geometry = ecs::spawn().set(geometry_desc).id();
    gpu::feature<QuadInstance>("test::cpp_quads")
        .geometry(geometry)
        .program(program)
        .extract([](ecs::changed<const Quad> quad, gpu::out<QuadInstance> out, gpu::bounds b) {
            std::memcpy(&*out, &*quad, sizeof(QuadInstance));
            *b = GpuSphere{ quad->offset[0], quad->offset[1], 0.5f, 2.0f };
            extracted_rows++;
        });
    *target = ecs::spawn()
                  .set(GpuTexture{ .format = GpuFormatRgba8,
                                   .usage = GpuTextureColorTarget | GpuTextureSampled,
                                   .width = 8,
                                   .height = 8 })
                  .id();
    ecs_entity_t graph = sigpu_graph(0, nullptr, 0);
    GpuPassDesc pass_desc{};
    pass_desc.kind = GpuPassRender;
    pass_desc.phase = phase;
    pass_desc.execute = [](gpu_pass_ctx_t *ctx, ecs_entity_t, void *) {
        const float tint[4] = { 1, 1, 1, 1 };
        gpu_cmd_uniform(ctx, "view_tint", tint, sizeof(tint));
    };
    ecs_entity_t pass = sigpu_pass(graph, &pass_desc);
    const float black[4] = { 0, 0, 0, 1 };
    sigpu_pass_color(pass, *target, GpuLoadClear, GpuStoreStore, black, 0);
    sigpu_graph_export(graph, *target);
    return graph;
}

/* Through the public capture: the texture saved as BMP, then read back. */
static uint32_t center_pixel(ecs_entity_t target) {
    const char *path = "sigpu_cpp_test.bmp";
    if (!sigpu_capture_bmp(target, path)) {
        return 0xdeadbeef;
    }
    SDL_Surface *loaded = SDL_LoadBMP(path);
    SDL_Surface *surface = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ABGR8888) : nullptr;
    uint32_t pixel = 0xdeadbeef;
    if (surface) {
        const auto *row = static_cast<const uint8_t *>(surface->pixels) +
                          (surface->h / 2) * surface->pitch;
        std::memcpy(&pixel, row + (surface->w / 2) * 4, 4);
    }
    SDL_DestroySurface(surface);
    SDL_DestroySurface(loaded);
    SDL_RemovePath(path);
    return pixel;
}

void feature_extract_draws(void) {
    ecs_entity_t target;
    scene(&target);
    ecs::spawn().set(Quad{ { -1, -1 }, { 2, 2 }, { 0, 1, 0, 1 } });
    for (int i = 0; i < 3; i++) {
        ecs_progress();
    }
    test_uint(0xff00ff00u, center_pixel(target));
    ecs::fini();
}

void feature_unchanged_not_extracted(void) {
    ecs_entity_t target;
    scene(&target);
    for (int i = 0; i < 10; i++) {
        ecs::spawn().set(Quad{ { -1, -1 }, { 0.2f, 0.2f }, { 1, 0, 0, 1 } });
    }
    for (int i = 0; i < 3; i++) {
        ecs_progress();
    }
    extracted_rows = 0;
    for (int i = 0; i < 3; i++) {
        ecs_progress();
    }
    test_uint(0, extracted_rows);
    test_uint(0, ecs_get_resource_read(GpuStats)->tables_extracted);
    ecs::fini();
}
