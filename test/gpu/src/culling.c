#include "util.h"

#define RED 0xff0000ffu
#define GREEN 0xff00ff00u
#define BLUE 0xffff0000u
#define BLACK 0xff000000u

void culling_all_or_none_visible(void) {
    gpu_test_start(0);
    ecs_entity_t view = gpu_test_view(0.0f);
    scene_t s = gpu_test_scene(GpuOrderBatched, true, view);
    for (uint32_t i = 0; i < 4; i++) {
        gpu_test_quad(-1 + i * 0.5f, -1, 0.5f, 2, RED);
    }
    gpu_test_frames(3);
    test_uint(4, gpu_test_stats()->candidates);
    test_uint(4, gpu_test_stats()->visible);
    GpuView desc = *ecs_get(view, GpuView);
    desc.view_projection[12] = 10.0f;
    ecs_set_cid(view, ecs_id(GpuView), &desc);
    gpu_test_frames(1);
    test_uint(4, gpu_test_stats()->candidates);
    test_uint(0, gpu_test_stats()->visible);
    test_uint(BLACK, gpu_test_pixel(s.target, 8, 8));
    ecs_fini();
}

void culling_partition_on_change_only(void) {
    gpu_test_start(0);
    ecs_entity_t view = gpu_test_view(0.0f);
    scene_t s = gpu_test_scene(GpuOrderBatched, true, view);
    ecs_entity_t first = 0;
    for (uint32_t i = 0; i < 400; i++) {
        const ecs_entity_t quad =
            gpu_test_quad(-1 + (i % 20) * 0.1f, -1 + (i / 20) * 0.1f, 0.1f, 0.1f, RED);
        first = first ? first : quad;
    }
    uint32_t built = 0;
    for (uint32_t i = 0; i < 8; i++) {
        gpu_test_frames(1);
        built += gpu_test_stats()->partitions_built;
    }
    test_uint(1, built);
    gpu_feature_t *feature = gpu_feature_of(s.feature);
    test_true(feature->ranges.items[0].partitioned);
    test_assert(gpu_test_stats()->cells_tested > 0);
    test_uint(400, gpu_test_stats()->visible);
    for (uint32_t i = 0; i < 8; i++) {
        gpu_test_frames(1);
        test_uint(0, gpu_test_stats()->partitions_built);
    }
    /* A change drops the partition; it comes back once stable again. */
    ecs_set(first, Quad, { .offset = { -1, -1 }, .size = { 0.1f, 0.1f }, .color = { 1, 1, 1, 1 } });
    gpu_test_frames(1);
    test_false(feature->ranges.items[0].partitioned);
    gpu_test_frames(6);
    test_true(feature->ranges.items[0].partitioned);
    test_uint(RED, gpu_test_pixel(s.target, 8, 8));
    ecs_fini();
}

void culling_lod_without_mutation(void) {
    gpu_test_start(0);
    ecs_entity_t view = gpu_test_view(0.0f);
    scene_t s = gpu_test_scene(GpuOrderBatched, true, view);
    GpuGeometry geometry = *ecs_get(s.geometry, GpuGeometry);
    geometry.lod_count = 2;
    geometry.lods[0] = (GpuGeometryLod){ .index_count = 3, .max_pixels = 4.0f };
    geometry.lods[1] = (GpuGeometryLod){ .index_count = 6 };
    ecs_set_cid(s.geometry, ecs_id(GpuGeometry), &geometry);
    ecs_entity_t small = gpu_test_quad(-1, -1, 0.2f, 0.2f, RED);
    ecs_entity_t large = gpu_test_quad(0, 0, 1, 1, GREEN);
    gpu_test_frames(3);
    /* Two LODs, two batches; the entities keep their table. */
    test_uint(2, gpu_test_stats()->batches);
    test_false(ecs_has_relation(small, DrawGeometry));
    test_false(ecs_has_relation(large, DrawGeometry));
    ecs_fini();
}

void culling_merge_materials_same_textures(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    ecs_entity_t shader = ecs_get(s.program, GpuProgram)->shader;
    ecs_entity_t textured = ecs_new();
    ecs_set(textured, GpuProgram, { .shader = shader, .vertex = "quad_vertex",
                                    .fragment = "quad_material", .phase = s.phase });
    sigpu_program_params(textured, ecs_id(QuadMaterial));
    ecs_entity_t white = gpu_test_color_texture(0xffffffffu);
    ecs_entity_t a = ecs_new();
    ecs_set(a, QuadMaterial, { .tint = { 255, 0, 0, 255 }, .flag = true, .albedo = white });
    ecs_entity_t b = ecs_new();
    ecs_set(b, QuadMaterial, { .tint = { 0, 255, 0, 255 }, .flag = true, .albedo = white });
    ecs_relate(gpu_test_quad(-1, -1, 1, 2, 0), DrawMaterial, a);
    ecs_relate(gpu_test_quad(0, -1, 1, 2, 0), DrawMaterial, b);
    gpu_test_frames(3);
    /* Uniforms differ, textures do not: one batch. */
    test_uint(1, gpu_test_stats()->batches);
    test_uint(RED, gpu_test_pixel(s.target, 2, 8));
    test_uint(GREEN, gpu_test_pixel(s.target, 13, 8));
    ecs_fini();
}

void culling_layered_order(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderLayered, false, 0);
    /* Later keys draw on top, whatever the creation order. */
    ecs_entity_t top = gpu_test_quad(-1, -1, 2, 2, GREEN);
    ecs_add(top, QuadLayer);
    ecs_set(top, QuadOrder, { 2 });
    ecs_entity_t bottom = gpu_test_quad(-1, -1, 2, 2, RED);
    ecs_add(bottom, QuadLayer);
    ecs_set(bottom, QuadOrder, { 1 });
    gpu_test_frames(3);
    test_uint(GREEN, gpu_test_pixel(s.target, 8, 8));
    ecs_set(bottom, QuadOrder, { 3 });
    gpu_test_frames(1);
    test_uint(RED, gpu_test_pixel(s.target, 8, 8));
    ecs_fini();
}

