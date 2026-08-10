#include "relation.h"
#include "siecs.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "storage/module_index.h"
#include "storage/query_index.h"
#include "storage/resource_index.h"
#include "storage/system_index.h"
#include "storage/table_index.h"
#include "utils.h"
#include "world_internal.h"
#include <string.h>

ecs_world_t ecs_world;
#ifndef NDEBUG
static bool ecs_world_started;
#endif
static bool ecs_world_finished;

void ecs_init_w_features(const ecs_world_feat_desc_t *features) {
    ecs_assert(!ecs_world_started, "ecs_init called while ECS is already running\n");
    if (ecs_world_finished) {
        memset(&ecs_world, 0, sizeof ecs_world);
        ecs_world_finished = false;
    }
#ifndef NDEBUG
    ecs_world_started = true;
#endif
    ecs_entity_index_init();
    ecs_component_index_init();
    ecs_relation_index_init();
    ecs_table_index_init();
    ecs_query_index_init();
    ecs_observer_index_init();
    ecs_system_index_init();
    ecs_module_index_init();
    ecs_resource_index_init();
    ecs_execution_context_init(&ecs_world.main_context);
    ecs_world.active_module = 0;
    ecs_world.features = *features;
    ecs_world.did_start = false;
    ecs_world.exit = false;
    ecs_world.delta_time = 0;
    ecs_world.last_time = 0;
    ecs_bootstrap();
    ecs_worker_pool_init(&ecs_world.worker_pool, ecs_world.features.worker_threads);
}

void ecs_init(void) { ecs_init_w_features(&(ecs_world_feat_desc_t){ 0 }); }

void ecs_fini(void) {
    ecs_assert(ecs_world_started && !ecs_world_finished, "ecs_fini called outside ECS lifetime\n");
    ecs_world_finished = true;

    ecs_worker_pool_fini(&ecs_world.worker_pool);

    /* Live component teardown must finish while world resources are available. */
    ecs_table_index_fini();
    ecs_observer_index_fini();
    ecs_system_index_fini();
    ecs_module_index_fini();
    ecs_query_index_fini();
    ecs_resource_index_fini();
    ecs_entity_index_fini();
    ecs_execution_context_fini(&ecs_world.main_context);
    ecs_component_index_fini();
    ecs_relation_index_fini();
    sicore_map_fini(&name_map);
#ifndef NDEBUG
    ecs_world_started = false;
#endif
}

void ecs_quit(void) { ecs_world.exit = true; }
