#include "observer_index.h"
#include "../datastructure/vec.h"
#include "../table.h"
#include "../world_internal.h"
#include "query_index.h"
#include "siecs.h"
#include <stdint.h>

#define ECS_BUILTIN_EVENT_COUNT 3 // EcsOnAdd, EcsOnRemove, EcsOnSet

void ecs_observer_index_init(ecs_observer_index_t *index) {
    ecs_vec_init(&index->observers, sizeof(ecs_observer_t));
    index->event_count = ECS_BUILTIN_EVENT_COUNT;
}

void ecs_observer_index_fini(ecs_observer_index_t *index) {
    for (uint32_t i = 0; i < index->observers.size; i++) {
        ecs_observer_t *obs = ecs_vec_get_mut(&index->observers, i, ecs_observer_t);
        ecs_query_index_destroy(&obs->query);
    }
    ecs_vec_fini(&index->observers);
}

uint16_t ecs_observer_index_create(ecs_observer_index_t *index, const ecs_observer_desc_t *desc) {
    ecs_observer_t *obs = ecs_vec_push_empty(&index->observers, sizeof(ecs_observer_t));
    obs->event = desc->on;
    obs->callback = desc->callback;
    obs->user_data = desc->user_data;
    obs->enabled = true;
    ecs_query_from_desc(&desc->query, &obs->query);
    return index->observers.size - 1;
}

void ecs_observer_index_match_tables(
        ecs_table_t *tables,
    uint16_t table_count,
    uint16_t observer_id
) {
    ecs_observer_t *obs =
        ecs_vec_get_mut(&ecs_world.observer_index.observers, observer_id, ecs_observer_t);
    for (uint16_t i = 0; i < table_count; i++) {
        if (ecs_query_match_table(&obs->query, &tables[i])) {
            ecs_table_add_observer(&tables[i], obs->event, observer_id);
        }
    }
}

void ecs_observer_index_add_table(ecs_table_t *table) {
    for (uint32_t i = 0; i < ecs_world.observer_index.observers.size; i++) {
        ecs_observer_t *obs = ecs_vec_get_mut(&ecs_world.observer_index.observers, i, ecs_observer_t);
        if (ecs_query_match_table(&obs->query, table)) {
            ecs_table_add_observer(table, obs->event, i);
        }
    }
}
