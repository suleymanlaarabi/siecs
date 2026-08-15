#include "module.h"
#include "siecs.h"
#include "storage/observer_index.h"
#include "utils.h"
#include "world_internal.h"

#define ECS_BUILTIN_EVENT_COUNT 5

ecs_observer_index_t observer_index;

void ecs_observer_index_init(void) {
    sicore_vec_init(&observer_index.observers, sizeof(ecs_observer_t));
    observer_index.event_count = ECS_BUILTIN_EVENT_COUNT;
}

void ecs_observer_index_fini(void) {
    ecs_observer_t *observers = observer_index.observers.data;
    for (uint32_t i = 0; i < observer_index.observers.size; i++)
        ecs_query_fini(observers[i].query);
    sicore_vec_fini(&observer_index.observers);
    observer_index = (ecs_observer_index_t){ 0 };
}

ecs_event_t ecs_event(void) {
    ecs_assert_not_scheduler_parallel("event registration");
    return observer_index.event_count++;
}

ecs_event_t ecs_event_register(ecs_event_t *id) {
    ecs_assert_not_scheduler_parallel("event registration");
    ecs_assert_not_null(id);

    if (*id == UINT16_MAX) {
        *id = ecs_event();
        return *id;
    }

    if (observer_index.event_count <= *id) {
        observer_index.event_count = *id + 1;
    }

    return *id;
}

ecs_observer_id_t ecs_observer_init(const ecs_observer_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("observer registration");
    ecs_assert(desc->callback != NULL, "Observer callback cannot be NULL");
    ecs_observer_t *observer =
        sicore_vec_push_empty(&observer_index.observers, sizeof(ecs_observer_t));
    *observer = (ecs_observer_t){
        .event = desc->on,
        .query = ecs_query_init(&desc->query),
        .callback = desc->callback,
        .user_data = desc->user_data,
        .next_module = UINT32_MAX,
        .enabled = true,
    };
    ecs_observer_id_t oid = observer_index.observers.size - 1;
    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&query_index.queries, observer->query, ecs_query_cache_t);
    cache->observer = oid;
    const uint16_t *table_ids = cache->table_ids.data;
    for (uint32_t i = 0; i < cache->table_ids.size; i++)
        ecs_table_add_observer(&table_index.tables[table_ids[i]], observer->event, oid);
    ecs_module_record_observer(oid);
    return oid;
}

void ecs_observer_enable(ecs_observer_id_t id) {
    sicore_vec_get_mut(&observer_index.observers, id, ecs_observer_t)->enabled = true;
}

void ecs_observer_disable(ecs_observer_id_t id) {
    sicore_vec_get_mut(&observer_index.observers, id, ecs_observer_t)->enabled = false;
}

void ecs_observer_trigger(ecs_entity_t entity, ecs_event_t event, const void *trigger_data) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    ecs_emit(table, entity, event, trigger_data);
}
