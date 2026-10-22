#include "util.h"

#define RED 0xff0000ffu
#define GREEN 0xff00ff00u
#define BLUE 0xffff0000u
#define BLACK 0xff000000u


void invariant_still_scene(void) {
    gpu_test_start(0);
    ecs_entity_t view = gpu_test_view(0.0f);
    gpu_test_scene(GpuOrderBatched, true, view);
    for (uint32_t i = 0; i < 300; i++) {
        gpu_test_quad(-1 + (i % 20) * 0.1f, -1 + (i / 20) * 0.1f, 0.1f, 0.1f, RED);
    }
    gpu_test_frames(10);
    for (uint32_t i = 0; i < 5; i++) {
        gpu_test_frames(1);
        const GpuStats *stats = gpu_test_stats();
        test_uint(0, stats->instance_uploads);
        test_uint(0, stats->uploads);
        test_uint(0, stats->graph_compiles);
        test_uint(0, stats->pipelines_created);
        test_uint(0, stats->created);
        test_uint(0, stats->range_growths);
        test_uint(0, stats->tables_extracted);
    }
    ecs_fini();
}

void invariant_moving_view(void) {
    gpu_test_start(0);
    ecs_entity_t view = gpu_test_view(0.0f);
    gpu_test_scene(GpuOrderBatched, true, view);
    for (uint32_t i = 0; i < 300; i++) {
        gpu_test_quad(-1 + (i % 20) * 0.1f, -1 + (i / 20) * 0.1f, 0.1f, 0.1f, RED);
    }
    gpu_test_frames(10);
    /* Warm-up: each frame slot gets its staging once. */
    for (uint32_t i = 0; i < 4; i++) {
        GpuView desc = *ecs_get(view, GpuView);
        desc.view_projection[12] = i % 2 ? 0.0f : 10.0f;
        ecs_set_cid(view, ecs_id(GpuView), &desc);
        gpu_test_frames(1);
    }
    uint32_t uploads = 0;
    for (uint32_t i = 0; i < 6; i++) {
        GpuView desc = *ecs_get(view, GpuView);
        desc.view_projection[12] = i % 2 ? 0.0f : 10.0f;
        ecs_set_cid(view, ecs_id(GpuView), &desc);
        gpu_test_frames(1);
        const GpuStats *stats = gpu_test_stats();
        /* Only the visible list moves: no instance, graph or object work. */
        test_uint(0, stats->instance_uploads);
        test_assert(stats->uploads <= 1);
        test_uint(0, stats->graph_compiles);
        test_uint(0, stats->created);
        test_uint(i % 2 ? 300 : 0, stats->visible);
        uploads += stats->uploads;
    }
    test_uint(3, uploads);
    ecs_fini();
}
