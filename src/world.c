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
#include "world_internal.h"
#include <stdlib.h>

ecs_world_t *ecs_init_w_features(const ecs_world_feat_desc_t *features) {
    ecs_world_t *world = malloc(sizeof(ecs_world_t));
    ecs_entity_index_init(&world->entity_index);
    ecs_component_index_init(&world->component_index);
    ecs_table_index_init(&world->table_index);
    ecs_query_index_init(&world->query_index);
    ecs_observer_index_init(&world->observer_index);
    ecs_system_index_init(&world->system_index);
    ecs_module_index_init(&world->module_index);
    ecs_resource_index_init(&world->resource_index);
    ecs_arena_init(&world->arena_allocator);
    world->active_module = 0;
    world->features = *features;
    world->did_start = false;
    world->exit = false;
    world->server = NULL;
    world->server_state = NULL;

    world->sireflect_registry = sireflect_registry_init();

    ecs_bootstrap(world);
    return world;
}

ecs_world_t *ecs_init() {
    ecs_world_t *world = ecs_with_features({});

    return world;
}

void ecs_fini(ecs_world_t *world) {
    ecs_resource_index_fini(&world->resource_index, world);
    ecs_table_index_fini(world, &world->table_index);
    ecs_entity_index_fini(&world->entity_index);
    ecs_component_index_fini(&world->component_index);
    ecs_query_index_fini(&world->query_index);
    ecs_observer_index_fini(&world->observer_index);
    ecs_system_index_fini(&world->system_index);
    ecs_module_index_fini(&world->module_index);
    ecs_arena_fini(&world->arena_allocator);
    sireflect_registry_fini(world->sireflect_registry);

    if (world->features.rest) {
        sihttp_server_stop(world->server);
    }
    sihttp_server_fini(world->server);
    free(world->server_state);

    free(world);
}
