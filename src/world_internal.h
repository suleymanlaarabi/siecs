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
    atomic_bool exit;
    double delta_time;
    double last_time;
    ecs_arena_t scene_strings;
    _Atomic uint64_t change_tick;
};

extern ecs_world_t ecs_world;

/* Change detection clock. Every query run takes a tick of its own; any other
 * write carries the current value, which is newer than every tick taken. */
static inline uint64_t ecs_change_tick(void) {
    return atomic_load_explicit(&ecs_world.change_tick, memory_order_relaxed);
}

static inline uint64_t ecs_change_tick_take(void) {
    return atomic_fetch_add_explicit(&ecs_world.change_tick, 1, memory_order_relaxed);
}

/* Parallel portions of one system store the same tick, so relaxed stores are
 * enough. */
static inline void ecs_table_column_changed(ecs_table_t *table, uint16_t column, uint64_t tick) {
    atomic_store_explicit(&table->cls[column].tick, tick, memory_order_relaxed);
}

static inline uint64_t ecs_table_column_tick(const ecs_table_t *table, uint16_t column) {
    return atomic_load_explicit(&table->cls[column].tick, memory_order_relaxed);
}

static inline ecs_execution_context_t *ecs_execution_context_current(void) {
    ecs_execution_context_t *context = ecs_tls_context;
    return context ? context : &ecs_world.main_context;
}

void ecs_resource_storage_init(void);
void ecs_resource_storage_fini(void);
void ecs_module_storage_init(void);
void ecs_module_storage_fini(void);

typedef ecs_relation_target_t RelationTarget;

typedef struct {
    sicore_vec_t entities;
} RelationSource;

#define ecs_get_record(entity) ecs_entity_index_get_record(ecs_first(entity))
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

/* True when emitting `event` on this table could run an observer. */
static inline bool ecs_table_observed(const ecs_table_t *table, ecs_event_t event) {
    if (ecs_observer_has_targets(event))
        return true;
    return table->observers_by_event.size > event &&
           sicore_vec_get(&table->observers_by_event, event, sicore_vec_t)->size != 0;
}

/* Deliver an event to the table's observers and to observers targeting the
 * entity or one of its bases. Callers test ecs_table_observed() first. */
void ecs_emit_observers(
    ecs_table_t *table,
    ecs_entity_t entity,
    ecs_event_t event,
    ecs_component_t component,
    const void *trigger_data
);

/* Most tables have no observer for most events: test that inline and keep the
 * delivery code (and its stack frame) out of every structural fast path. */
static inline void ecs_emit(
    ecs_table_t *table,
    ecs_entity_t entity,
    ecs_event_t event,
    ecs_component_t component,
    const void *trigger_data
) {
    if (ECS_UNLIKELY(ecs_table_observed(table, event)))
        ecs_emit_observers(table, entity, event, component, trigger_data);
}

static inline bool ecs_is_deferred(void) {
    ecs_execution_context_t *context = ecs_execution_context_current();
    return context->defer_depth != 0 || context->flushing_commands || context->scheduler_parallel;
}

static inline void ecs_assert_not_scheduler_parallel(const char *operation) {
    ecs_fatal_if(
        ecs_execution_context_current()->scheduler_parallel,
        "%s is not allowed from a parallel system execution\n",
        operation
    );
}

void ecs_system_run_job(
    ecs_system_id_t system,
    uint16_t table,
    uint32_t offset,
    uint32_t count,
    uint64_t tick
);

void ecs_type_slots_reset(void);
void ecs_bootstrap(void);

#endif
