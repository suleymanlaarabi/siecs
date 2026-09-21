#ifndef SIECS_WORLD_INTERNAL_H
#define SIECS_WORLD_INTERNAL_H
#include "command_buffer.h"
#include "datastructure/arena.h"
#include "module.h"
#include "relation.h"
#include "siecs.h"
#include "sireflect.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "storage/observer_index.h"
#include "storage/query_index.h"
#include "storage/system_index.h"
#include "storage/table_index.h"
#include "utils.h"
#include "worker_pool.h"

typedef struct ecs_world_s ecs_world_t;

struct ecs_world_s {
    ecs_module_id_t active_module;
    ecs_world_feat_desc_t features;
    ecs_execution_context_t main_context;
    ecs_worker_pool_t worker_pool;
    sicore_vec_t fini_callbacks;
    bool did_start;
    bool exit;
    double delta_time;
    double last_time;
    ecs_arena_t scene_strings;
};

extern ecs_world_t ecs_world;

void ecs_resource_storage_init(void);
void ecs_resource_storage_fini(void);
void ecs_module_storage_init(void);
void ecs_module_storage_fini(void);

typedef ecs_relation_target_t RelationTarget;

typedef struct {
    sicore_vec_t entities;
} RelationSource;

#define ecs_get_record(entity)                                                                     \
    sicore_vec_get_mut(&entity_index.entities, ecs_first(entity), ecs_entity_record_t)
#define ecs_get_table(tid) ecs_table_index_at(tid)

static inline void ecs_emit_target_observers(
    ecs_table_t *table,
    ecs_entity_t entity,
    ecs_entity_t observer_target,
    ecs_event_t event,
    ecs_component_t component,
    const void *trigger_data
) {
    uint64_t key = ecs_observer_target_key(ecs_entity_id(observer_target), event);
    uint32_t at = ecs_observer_target_lower_bound(key);
    const uint64_t *keys = observer_index.target_keys.data;
    if (at == observer_index.target_keys.size || keys[at] != key)
        return;

    uint16_t table_id = (uint16_t)(table - table_index.tables);
    const ecs_observer_id_t *ids = observer_index.target_observers.data;
    uint32_t count = observer_index.target_keys.size;
    while (at < count && keys[at] == key) {
        ecs_observer_id_t oid = ids[at++];
        ecs_observer_t *observer =
            sicore_vec_get_mut(&observer_index.observers, oid, ecs_observer_t);
        if (!observer->enabled)
            continue;
        if (observer->query != ECS_OBSERVER_NO_QUERY) {
            ecs_query_cache_t *cache = ecs_query_cache(observer->query);
            if (ecs_query_table_position(cache, table_id) == UINT16_MAX)
                continue;
        }
        ecs_observer_event_t observer_event = {
            .entity = entity,
            .event = event,
            .component = component,
            .user_data = observer->user_data,
            .trigger_data = trigger_data,
        };
        observer->callback(&observer_event);
    }
}

static inline void ecs_emit(
    ecs_table_t *table,
    ecs_entity_t entity,
    ecs_event_t event,
    ecs_component_t component,
    const void *trigger_data
) {
    if (table->observers_by_event.size > event) {
        const sicore_vec_t *list = sicore_vec_get(&table->observers_by_event, event, sicore_vec_t);
        uint32_t n = list->size;
        for (uint32_t i = 0; i < n; i++) {
            ecs_observer_id_t oid = *sicore_vec_get(list, i, ecs_observer_id_t);
            ecs_observer_t *obs =
                sicore_vec_get_mut(&observer_index.observers, oid, ecs_observer_t);
            if (!obs->enabled) {
                continue;
            }
            ecs_observer_event_t observer_event = {
                .entity = entity,
                .event = event,
                .component = component,
                .user_data = obs->user_data,
                .trigger_data = trigger_data,
            };
            obs->callback(&observer_event);
        }
    }

    if (observer_index.target_keys.size == 0) {
        return;
    }

    for (ecs_entity_t target = entity; target; target = ecs_entity_base_raw(target)) {
        ecs_emit_target_observers(table, entity, target, event, component, trigger_data);
    }
}

static inline bool ecs_is_deferred(void) {
    ecs_execution_context_t *context = ecs_execution_context_current();
    return context->defer_depth != 0 || context->flushing_commands || context->scheduler_parallel;
}

static inline void ecs_assert_not_scheduler_parallel(const char *operation) {
    ecs_assert(
        !ecs_execution_context_current()->scheduler_parallel,
        "%s is not allowed from a parallel system wave\n",
        operation
    );
}

void ecs_system_run_prepared(ecs_system_id_t system);

void ecs_bootstrap(void);

extern sicore_map_t name_map;

#endif
