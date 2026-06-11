#ifndef SIECS_WORLD_INTERNAL_H
#define SIECS_WORLD_INTERNAL_H
#include "datastructure/vec.h"
#include "sihttp.h"
#include "sireflect.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "storage/observer_index.h"
#include "storage/query_index.h"
#include "storage/system_index.h"
#include "storage/table_index.h"
#include <bits/pthreadtypes.h>

typedef struct ecs_world_s {
    ecs_entity_index_t entity_index;
    ecs_component_index_t component_index;
    ecs_table_index_t table_index;
    ecs_query_index_t query_index;
    ecs_observer_index_t observer_index;
    ecs_system_index_t system_index;
    sireflect_registry_t *sireflect_registry;
    #ifdef SIECS_REST
    sihttp_server_t *server;
    #endif
} ecs_world_t;

struct sihttp_app_state_s {
    ecs_world_t *world;
};

typedef struct {
    ecs_entity_t target;
} RelationTarget;

typedef struct {
    ecs_vec_t entities;
} RelationSource;

#define ecs_get_record(world, entity)                                                              \
    ecs_vec_get_mut(&world->entity_index.entities, ecs_first(entity), ecs_entity_record_t)
#define ecs_get_table(world, tid) ecs_table_index_at(&world->table_index, tid)

static inline void ecs_emit(
    ecs_world_t *world,
    ecs_table_t *table,
    ecs_entity_t entity,
    ecs_event_t event,
    const void *trigger_data
) {
    if (table->observers_by_event.size <= event) {
        return;
    }
    const ecs_vec_t *list = ecs_vec_get(&table->observers_by_event, event, ecs_vec_t);
    uint32_t n = list->size;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t oid = *ecs_vec_get(list, i, uint16_t);
        ecs_observer_t *obs =
            ecs_vec_get_mut(&world->observer_index.observers, oid, ecs_observer_t);
        ecs_observer_event_t observer_event = {
            .world = world,
            .entity = entity,
            .event = event,
            .user_data = obs->user_data,
            .trigger_data = trigger_data,
        };
        obs->callback(&observer_event);
    }
}

void ecs_bootstrap(ecs_world_t *world);
struct ecs_table_s *ecs_iter_table(ecs_iter_t *it);


#endif
