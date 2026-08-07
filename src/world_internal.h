#ifndef SIECS_WORLD_INTERNAL_H
#define SIECS_WORLD_INTERNAL_H
#include "command_buffer.h"
#include "datastructure/arena.h"
#include "siecs.h"
#include "sireflect.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "storage/module_index.h"
#include "storage/observer_index.h"
#include "storage/query_index.h"
#include "storage/resource_index.h"
#include "storage/system_index.h"
#include "storage/table_index.h"
#include "relation.h"

typedef struct ecs_world_s ecs_world_t;

struct ecs_world_s {
    ecs_entity_index_t entity_index;
    ecs_component_index_t component_index;
    ecs_relation_index_t relation_index;
    ecs_table_index_t table_index;
    ecs_query_index_t query_index;
    ecs_observer_index_t observer_index;
    ecs_system_index_t system_index;
    ecs_module_index_t module_index;
    ecs_resource_index_t resource_index;
    ecs_module_id_t active_module;
    ecs_world_feat_desc_t features;
    ecs_arena_t arena_allocator;
    ecs_command_buffer_t commands;
    uint32_t defer_depth;
    bool flushing_commands;
    bool did_start;
    bool exit;
    double delta_time;
    double last_time;
};

extern ecs_world_t ecs_world;

typedef ecs_relation_target_t RelationTarget;

typedef struct {
    sicore_vec_t entities;
} RelationSource;

#define ecs_get_record(entity)                                                                     \
    sicore_vec_get_mut(&ecs_world.entity_index.entities, ecs_first(entity), ecs_entity_record_t)
#define ecs_get_table(tid) ecs_table_index_at(tid)

static inline void
ecs_emit(ecs_table_t *table, ecs_entity_t entity, ecs_event_t event, const void *trigger_data) {
    if (table->observers_by_event.size <= event) {
        return;
    }
    const sicore_vec_t *list = sicore_vec_get(&table->observers_by_event, event, sicore_vec_t);
    uint32_t n = list->size;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t oid = *sicore_vec_get(list, i, uint16_t);
        ecs_observer_t *obs =
            sicore_vec_get_mut(&ecs_world.observer_index.observers, oid, ecs_observer_t);
        if (!obs->enabled) {
            continue;
        }
        ecs_observer_event_t observer_event = {
            .entity = entity,
            .event = event,
            .user_data = obs->user_data,
            .trigger_data = trigger_data,
        };
        obs->callback(&observer_event);
    }
}

static inline bool ecs_is_deferred(void) {
    return ecs_world.defer_depth != 0 || ecs_world.flushing_commands;
}

void ecs_bootstrap(void);

extern sicore_map_t name_map;

#endif
