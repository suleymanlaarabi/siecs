#include "util.h"

#define RED 0xff0000ffu
#define GREEN 0xff00ff00u
#define BLUE 0xffff0000u
#define BLACK 0xff000000u

void feature_draws_instances(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    gpu_test_quad(-1, -1, 1, 2, RED);
    gpu_test_quad(0, -1, 1, 2, GREEN);
    gpu_test_frames(3);
    test_uint(GpuReady, ecs_get(s.graph, GpuGraphState)->status);
    test_uint(RED, gpu_test_pixel(s.target, 2, 8));
    test_uint(GREEN, gpu_test_pixel(s.target, 13, 8));
    test_uint(1, gpu_test_stats()->batches);
    ecs_fini();
}

void feature_unchanged_table_not_extracted(void) {
    gpu_test_start(0);
    gpu_test_scene(GpuOrderBatched, false, 0);
    for (uint32_t i = 0; i < 8; i++) {
        gpu_test_quad(-1 + i * 0.25f, -1, 0.25f, 2, RED);
    }
    gpu_test_frames(3);
    for (uint32_t i = 0; i < 4; i++) {
        gpu_test_frames(1);
        test_uint(0, gpu_test_stats()->tables_extracted);
        test_uint(1, gpu_test_stats()->tables_skipped);
        test_uint(0, gpu_test_stats()->instance_uploads);
    }
    ecs_fini();
}

void feature_change_reextracts(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    ecs_entity_t quad = gpu_test_quad(-1, -1, 2, 2, RED);
    gpu_test_frames(3);
    test_uint(RED, gpu_test_pixel(s.target, 8, 8));
    ecs_set(quad, Quad, { .offset = { -1, -1 }, .size = { 2, 2 }, .color = { 0, 0, 1, 1 } });
    gpu_test_frames(1);
    test_uint(1, gpu_test_stats()->tables_extracted);
    test_uint(1, gpu_test_stats()->instance_uploads);
    test_uint(BLUE, gpu_test_pixel(s.target, 8, 8));
    ecs_fini();
}

void feature_swap_remove_reextracts(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    ecs_entity_t left = gpu_test_quad(-1, -1, 1, 2, RED);
    gpu_test_quad(0, -1, 1, 2, GREEN);
    gpu_test_frames(3);
    ecs_kill(left);
    gpu_test_frames(1);
    test_uint(1, gpu_test_stats()->tables_extracted);
    test_uint(BLACK, gpu_test_pixel(s.target, 2, 8));
    test_uint(GREEN, gpu_test_pixel(s.target, 13, 8));
    gpu_feature_t *feature = gpu_feature_of(s.feature);
    test_uint(1, feature->ranges.count);
    test_uint(1, feature->ranges.items[0].count);
    ecs_fini();
}

void feature_range_by_table(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    const ecs_component_t moved = ecs_tag_init("Moved");
    ecs_entity_t a = gpu_test_quad(-1, -1, 1, 2, RED);
    gpu_test_quad(0, -1, 1, 2, GREEN);
    gpu_test_frames(2);
    gpu_feature_t *feature = gpu_feature_of(s.feature);
    test_uint(1, feature->ranges.count);
    /* Moving a row to another table gives that table its own range; the
     * new range is found by table id, not by position in the query. */
    ecs_add_cid(a, moved);
    gpu_test_frames(1);
    test_uint(2, feature->ranges.count);
    for (uint32_t r = 0; r < feature->ranges.count; r++) {
        const gpu_range_t *range = &feature->ranges.items[r];
        test_uint(r + 1, feature->range_of_table[range->table]);
        test_uint(1, range->count);
    }
    test_uint(RED, gpu_test_pixel(s.target, 2, 8));
    test_uint(GREEN, gpu_test_pixel(s.target, 13, 8));
    ecs_fini();
}

void feature_empty_range_released(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    const ecs_component_t other = ecs_tag_init("Other");
    gpu_test_quad(-1, -1, 1, 2, RED);
    ecs_entity_t moved = gpu_test_quad(0, -1, 1, 2, GREEN);
    ecs_add_cid(moved, other);
    gpu_test_frames(2);
    gpu_feature_t *feature = gpu_feature_of(s.feature);
    test_uint(2, feature->ranges.count);
    ecs_kill(moved);
    gpu_test_frames(1);
    test_uint(2, feature->ranges.count);
    gpu_test_frames(60);
    test_uint(1, feature->ranges.count);
    ecs_fini();
}

void feature_parallel_portions(void) {
    ecs_init_w_features(&(ecs_world_feat_desc_t){ .worker_threads = 3 });
    ECS_MODULE_IMPORT(sigpu, { 0 });
    size_t size;
    const unsigned char *pack = gpu_test_pack(&size);
    sigpu_asset_register(TEST_PACK, pack, size);
    ECS_COMPONENT_REGISTER(Quad, QuadInstance, QuadMaterial, QuadLayer, QuadOrder);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    for (uint32_t i = 0; i < 1000; i++) {
        ecs_set(ecs_new(), Quad, { .offset = { (float)i, 0 }, .size = { 1, 1 } });
    }
    gpu_test_frames(2);
    gpu_feature_t *feature = gpu_feature_of(s.feature);
    test_uint(1, feature->ranges.count);
    const gpu_range_t *range = &feature->ranges.items[0];
    test_uint(1000, range->count);
    const QuadInstance *instances = (const QuadInstance *)feature->instances + range->offset;
    for (uint32_t row = 0; row < 1000; row++) {
        const Quad *quad = ecs_get(feature->entities[range->offset + row], Quad);
        test_flt(quad->offset[0], instances[row].offset[0]);
    }
    ecs_fini();
}

void feature_material_retarget(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    ecs_entity_t shader = ecs_get(s.program, GpuProgram)->shader;
    ecs_entity_t textured = ecs_new();
    ecs_set(textured, GpuProgram, { .shader = shader, .vertex = "quad_vertex",
                                    .fragment = "quad_material", .phase = s.phase });
    test_true(sigpu_program_params(textured, ecs_id(QuadMaterial)));
    ecs_entity_t white = gpu_test_color_texture(0xffffffffu);
    ecs_entity_t red = ecs_new();
    ecs_set(red, QuadMaterial, { .tint = { 255, 0, 0, 255 }, .flag = true, .albedo = white });
    ecs_entity_t blue = ecs_new();
    ecs_set(blue, QuadMaterial, { .tint = { 0, 0, 255, 255 }, .flag = true, .albedo = white });
    ecs_entity_t quad = gpu_test_quad(-1, -1, 2, 2, GREEN);
    ecs_relate(quad, DrawMaterial, red);
    gpu_test_frames(3);
    test_uint(GpuReady, gpu_test_state(red)->status);
    test_uint(RED, gpu_test_pixel(s.target, 8, 8));
    ecs_relate(quad, DrawMaterial, blue);
    gpu_test_frames(1);
    test_uint(BLUE, gpu_test_pixel(s.target, 8, 8));
    ecs_fini();
}

