#include "observer_index.h"
#include "../table.h"
#include "../world_internal.h"
#include "query_index.h"
#include "siecs.h"
#include <stdint.h>

ecs_observer_index_t observer_index;

#define ECS_BUILTIN_EVENT_COUNT 5 // component and relation events

void ecs_observer_index_init() {
    ecs_observer_index_t *index = &observer_index;
    sicore_vec_init(&index->observers, sizeof(ecs_observer_t));
    index->event_count = ECS_BUILTIN_EVENT_COUNT;
}

void ecs_observer_index_fini() {
    ecs_observer_index_t *index = &observer_index;
    for (uint32_t i = 0; i < index->observers.size; i++) {
        ecs_observer_t *obs = sicore_vec_get_mut(&index->observers, i, ecs_observer_t);
        ecs_query_fini(obs->query);
    }
    sicore_vec_fini(&index->observers);
    *index = (ecs_observer_index_t){ 0 };
}

uint16_t ecs_observer_index_create(const ecs_observer_desc_t *desc) {
    ecs_observer_index_t *index = &observer_index;
    ecs_observer_t *obs = sicore_vec_push_empty(&index->observers, sizeof(ecs_observer_t));
    obs->event = desc->on;
    obs->callback = desc->callback;
    obs->user_data = desc->user_data;
    obs->enabled = true;
    obs->query = ecs_query_init(&desc->query);
    return index->observers.size - 1;
}

void ecs_observer_index_match_tables(uint16_t observer_id) {
    ecs_observer_t *obs = sicore_vec_get_mut(&observer_index.observers, observer_id, ecs_observer_t);
    const ecs_query_cache_t *cache = sicore_vec_get(&query_index.queries, obs->query, ecs_query_cache_t);
    const uint16_t *table_ids = cache->table_ids.data;
    for (uint32_t i = 0; i < cache->table_ids.size; i++) {
        ecs_table_add_observer(&table_index.tables[table_ids[i]], obs->event, observer_id);
    }
}

void ecs_observer_index_add_table(ecs_table_t *table) {
    for (uint32_t i = 0; i < observer_index.observers.size; i++) {
        ecs_observer_t *obs = sicore_vec_get_mut(&observer_index.observers, i, ecs_observer_t);
        const ecs_query_cache_t *cache = sicore_vec_get(&query_index.queries, obs->query, ecs_query_cache_t);
        const uint16_t table_id = (uint16_t)(table - table_index.tables);
        const uint16_t *ids = cache->table_ids.data;
        if (cache->table_ids.size && ids[cache->table_ids.size - 1] == table_id) {
            ecs_table_add_observer(table, obs->event, i);
        }
    }
}
