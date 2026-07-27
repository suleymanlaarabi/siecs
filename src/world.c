#include "siecs.h"
#ifndef SIHTTP_H
#include "sihttp.h"
#endif
#ifndef SIREFLECT_H
#include "sireflect.h"
#endif
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
static bool ecs_world_started;
static bool ecs_world_finished;

void ecs_init_w_features(const ecs_world_feat_desc_t *features) {
    ecs_assert(!ecs_world_started, "ecs_init called while ECS is already running\n");
    if (ecs_world_finished) {
        memset(&ecs_world, 0, sizeof ecs_world);
        ecs_world_finished = false;
    }
    ecs_world_started = true;

    ecs_entity_index_init(&ecs_world.entity_index);
    ecs_component_index_init(&ecs_world.component_index);
    ecs_table_index_init(&ecs_world.table_index);
    ecs_query_index_init(&ecs_world.query_index);
    ecs_observer_index_init(&ecs_world.observer_index);
    ecs_system_index_init(&ecs_world.system_index);
    ecs_module_index_init(&ecs_world.module_index);
    ecs_resource_index_init(&ecs_world.resource_index);
    ecs_arena_init(&ecs_world.arena_allocator);
    ecs_command_buffer_init(&ecs_world.commands);
    ecs_world.active_module = 0;
    ecs_world.features = *features;
    ecs_world.defer_depth = 0;
    ecs_world.flushing_commands = false;
    ecs_world.did_start = false;
    ecs_world.exit = false;
    ecs_world.server = NULL;
    ecs_world.delta_time = 0;
    ecs_world.last_time = 0;
    ecs_world.sireflect_registry = sireflect_registry_init();

    ecs_bootstrap();
}

void ecs_init(void) { ecs_init_w_features(&(ecs_world_feat_desc_t){}); }

void ecs_fini(void) {
    ecs_assert(ecs_world_started && !ecs_world_finished, "ecs_fini called outside ECS lifetime\n");
    ecs_world_finished = true;

    ecs_resource_index_fini(&ecs_world.resource_index);
    ecs_table_index_fini(&ecs_world.table_index);
    ecs_entity_index_fini(&ecs_world.entity_index);
    ecs_component_index_fini(&ecs_world.component_index);
    ecs_query_index_fini(&ecs_world.query_index);
    ecs_observer_index_fini(&ecs_world.observer_index);
    ecs_system_index_fini(&ecs_world.system_index);
    ecs_module_index_fini(&ecs_world.module_index);
    ecs_command_buffer_fini(&ecs_world.commands);
    ecs_arena_fini(&ecs_world.arena_allocator);
    sireflect_registry_fini(ecs_world.sireflect_registry);

    if (ecs_world.features.rest) {
        sihttp_server_stop(ecs_world.server);
    }
    sihttp_server_fini(ecs_world.server);
    ecs_world_started = false;
}

void ecs_quit(void) { ecs_world.exit = true; }
