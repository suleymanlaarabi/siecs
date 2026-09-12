#ifndef SIECS_STORAGE_OBSERVER_INDEX_H
#define SIECS_STORAGE_OBSERVER_INDEX_H
#include "../table.h"
#include "query_index.h"
#include "siecs.h"
#include <stdint.h>

typedef struct {
    ecs_observer_callback_t callback;
    uintptr_t user_data;

    ecs_observer_id_t next_module;
    uint32_t target_entity;

    ecs_event_t event;
    ecs_query_id_t query;
    ecs_module_id_t module;

    bool enabled;
} ecs_observer_t;

#define ECS_OBSERVER_GLOBAL_ENTITY UINT32_MAX
#define ECS_OBSERVER_NO_QUERY UINT16_MAX

typedef struct {
    sicore_vec_t observers;        // ecs_observer_t
    sicore_vec_t target_keys;      // uint64_t
    sicore_vec_t target_observers; // ecs_observer_id_t

    ecs_observer_id_t first_free;
    uint16_t event_count;
} ecs_observer_index_t;

extern ecs_observer_index_t observer_index;

static inline uint64_t ecs_observer_target_key(uint32_t entity_id, ecs_event_t event) {
    return ((uint64_t)entity_id << 16) | event;
}

static inline uint32_t ecs_observer_target_lower_bound(uint64_t key) {
    const uint64_t *keys = observer_index.target_keys.data;
    uint32_t first = 0;
    uint32_t count = observer_index.target_keys.size;
    while (count) {
        uint32_t step = count / 2;
        uint32_t at = first + step;
        if (keys[at] < key) {
            first = at + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

void ecs_observer_index_init();
void ecs_observer_index_fini();
void ecs_observer_fini_entity(ecs_entity_t entity);

#endif
