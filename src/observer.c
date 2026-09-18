#include "module.h"
#include "siecs.h"
#include "storage/observer_index.h"
#include "storage/query_index.h"
#include "utils.h"
#include "world_internal.h"
#include <string.h>

#define ECS_BUILTIN_EVENT_COUNT 5

ecs_observer_index_t observer_index;

void ecs_observer_index_init(void) {
    sicore_vec_init(&observer_index.observers, sizeof(ecs_observer_t));
    sicore_vec_init(&observer_index.target_keys, sizeof(uint64_t));
    sicore_vec_init(&observer_index.target_observers, sizeof(ecs_observer_id_t));
    observer_index.first_free = UINT32_MAX;
    observer_index.event_count = ECS_BUILTIN_EVENT_COUNT;
}

void ecs_observer_index_fini(void) {
    ecs_observer_t *observers = observer_index.observers.data;
    for (uint32_t i = 0; i < observer_index.observers.size; i++) {
        if (observers[i].query != ECS_OBSERVER_NO_QUERY)
            ecs_query_fini(observers[i].query);
    }
    sicore_vec_fini(&observer_index.target_observers);
    sicore_vec_fini(&observer_index.target_keys);
    sicore_vec_fini(&observer_index.observers);
    observer_index = (ecs_observer_index_t){ 0 };
}

static ecs_observer_id_t ecs_observer_alloc(void) {
    if (observer_index.first_free != UINT32_MAX) {
        ecs_observer_id_t id = observer_index.first_free;
        ecs_observer_t *slot = sicore_vec_get_mut(&observer_index.observers, id, ecs_observer_t);
        observer_index.first_free = slot->next_module;
        return id;
    }
    sicore_vec_push_empty(&observer_index.observers, sizeof(ecs_observer_t));
    return observer_index.observers.size - 1;
}

static void
ecs_observer_target_insert(uint32_t entity_id, ecs_event_t event, ecs_observer_id_t observer) {
    uint64_t key = ecs_observer_target_key(entity_id, event);
    uint32_t at = ecs_observer_target_lower_bound(key);
    uint64_t *keys = observer_index.target_keys.data;
    while (at < observer_index.target_keys.size && keys[at] == key)
        at++;

    uint32_t old_size = observer_index.target_keys.size;
    sicore_vec_push_empty(&observer_index.target_keys, sizeof(uint64_t));
    keys = observer_index.target_keys.data;
    memmove(&keys[at + 1], &keys[at], (old_size - at) * sizeof(*keys));
    keys[at] = key;

    old_size = observer_index.target_observers.size;
    sicore_vec_push_empty(&observer_index.target_observers, sizeof(ecs_observer_id_t));
    ecs_observer_id_t *ids = observer_index.target_observers.data;
    memmove(&ids[at + 1], &ids[at], (old_size - at) * sizeof(*ids));
    ids[at] = observer;
}

static void
ecs_observer_target_remove(uint32_t entity_id, ecs_event_t event, ecs_observer_id_t observer) {
    uint64_t key = ecs_observer_target_key(entity_id, event);
    uint32_t at = ecs_observer_target_lower_bound(key);
    uint64_t *keys = observer_index.target_keys.data;
    ecs_observer_id_t *ids = observer_index.target_observers.data;
    uint32_t count = observer_index.target_keys.size;
    while (at < count && keys[at] == key) {
        if (ids[at] == observer) {
            memmove(&keys[at], &keys[at + 1], (count - at - 1) * sizeof(*keys));
            memmove(&ids[at], &ids[at + 1], (count - at - 1) * sizeof(*ids));
            observer_index.target_keys.size--;
            observer_index.target_observers.size--;
            return;
        }
        at++;
    }
}

static inline bool ecs_observer_has_entity_filter(const ecs_query_desc_t *query) {
    return query->components[0].id != 0 || query->relations[0].id != 0 || query->is_a != 0;
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
    if (observer_index.event_count <= *id)
        observer_index.event_count = *id + 1;
    return *id;
}

ecs_observer_id_t ecs_observer_init(const ecs_observer_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("observer registration");
    ecs_assert(desc->callback != NULL, "Observer callback cannot be NULL");
    if (desc->entity != 0)
        ecs_assert_entity_alive(desc->entity);

    ecs_observer_id_t oid = ecs_observer_alloc();
    ecs_observer_t *observer = sicore_vec_get_mut(&observer_index.observers, oid, ecs_observer_t);
    *observer = (ecs_observer_t){
        .callback = desc->callback,
        .user_data = desc->user_data,
        .next_module = UINT32_MAX,
        .target_entity =
            desc->entity != 0 ? ecs_entity_id(desc->entity) : ECS_OBSERVER_GLOBAL_ENTITY,
        .event = desc->on,
        .query = ECS_OBSERVER_NO_QUERY,
        .module = 0,
        .enabled = true,
    };

    if (desc->entity == 0) {
        observer->query = ecs_query_init(&desc->query);
        ecs_query_cache_t *cache = ecs_query_cache(observer->query);
        cache->observer = oid;
        if (cache->active_index == UINT32_MAX && !desc->query.resources[0].id)
            ecs_query_index_activate(observer->query, NULL, table_index.table_count);
        for (uint16_t i = 0; i < cache->table_count; i++) {
            uint16_t table_id = ecs_query_table_id(cache, i);
            ecs_table_add_observer(&table_index.tables[table_id], observer->event, oid);
        }
    } else {
        ecs_observer_target_insert(ecs_entity_id(desc->entity), desc->on, oid);
        if (ecs_observer_has_entity_filter(&desc->query)) {
            observer->query = ecs_query_init(&desc->query);
        }
    }

    ecs_module_record_observer(oid);
    return oid;
}

void ecs_observer_fini(ecs_observer_id_t id) {
    ecs_observer_t *observer = sicore_vec_get_mut(&observer_index.observers, id, ecs_observer_t);
    ecs_assert(observer->callback != NULL, "Observer is not alive");
    uint32_t target_entity = observer->target_entity;
    ecs_event_t event = observer->event;
    ecs_query_id_t query = observer->query;

    if (target_entity != ECS_OBSERVER_GLOBAL_ENTITY) {
        ecs_observer_target_remove(target_entity, event, id);
    } else {
        ecs_query_cache_t *cache = ecs_query_cache(query);
        for (uint16_t i = 0; i < cache->table_count; i++) {
            uint16_t table_id = ecs_query_table_id(cache, i);
            ecs_table_remove_observer(&table_index.tables[table_id], event, id);
        }
    }
    ecs_module_forget_observer(id);
    if (query != ECS_OBSERVER_NO_QUERY)
        ecs_query_fini(query);

    ecs_observer_id_t next_free = observer_index.first_free;
    *observer = (ecs_observer_t){ 0 };
    observer->next_module = next_free;
    observer->target_entity = ECS_OBSERVER_GLOBAL_ENTITY;
    observer->query = ECS_OBSERVER_NO_QUERY;
    observer_index.first_free = id;
}

void ecs_observer_fini_entity(ecs_entity_t entity) {
    uint32_t entity_id = ecs_entity_id(entity);
    uint64_t first_key = (uint64_t)entity_id << 16;
    uint64_t after_key = ((uint64_t)entity_id + 1) << 16;
    uint32_t at = ecs_observer_target_lower_bound(first_key);
    while (at < observer_index.target_keys.size &&
           ((const uint64_t *)observer_index.target_keys.data)[at] < after_key) {
        ecs_observer_id_t id =
            ((const ecs_observer_id_t *)observer_index.target_observers.data)[at];
        ecs_observer_fini(id);
    }
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
    ecs_emit(table, entity, event, 0, trigger_data);
}
