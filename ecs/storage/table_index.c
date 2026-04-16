#include "table_index.h"
#include "../table.h"
#include "../datastructure/vec.h"
#include "ecs/type.h"
#include <stdint.h>
#include <stdlib.h>

#define INITIAL_CAPACITY 64
#define LOAD_FACTOR 0.75

static inline uint32_t ecs_type_hash(ecs_type_t type) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < type.count; ++i) {
        h ^= (uint32_t)type.ids[i];
        h *= 16777619u;
    }
    // Final mix to improve distribution
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

void ecs_table_index_init(ecs_table_index_t *map) {
    ecs_vec_init(&map->tables, sizeof(ecs_table_t));
    map->slot_count = INITIAL_CAPACITY;
    map->slot_mask = map->slot_count - 1;
    map->slots = malloc(sizeof(ecs_type_slot_t) * map->slot_count);
    for (uint32_t i = 0; i < map->slot_count; ++i) {
        map->slots[i].table_index = UINT32_MAX;
    }
}

void ecs_table_index_fini(ecs_table_index_t *map) {
    for (uint32_t i = 0; i < map->tables.size; i++) {
        ecs_table_fini(ecs_vec_get_mut(&map->tables, i, ecs_table_t));
    }
    ecs_vec_fini(&map->tables);
    free(map->slots);
}

static void ecs_table_index_resize(ecs_table_index_t *map) {
    uint32_t old_count = map->slot_count;
    ecs_type_slot_t *old_slots = map->slots;

    map->slot_count *= 2;
    map->slot_mask = map->slot_count - 1;
    map->slots = malloc(sizeof(ecs_type_slot_t) * map->slot_count);
    for (uint32_t i = 0; i < map->slot_count; ++i) {
        map->slots[i].table_index = UINT32_MAX;
    }

    for (uint32_t i = 0; i < old_count; ++i) {
        if (old_slots[i].table_index != UINT32_MAX) {
            uint32_t hash = old_slots[i].hash;
            uint32_t slot_idx = hash & map->slot_mask;
            while (map->slots[slot_idx].table_index != UINT32_MAX) {
                slot_idx = (slot_idx + 1) & map->slot_mask;
            }
            map->slots[slot_idx] = old_slots[i];
        }
    }
    free(old_slots);
}

uint16_t ecs_table_index_get_or_create(ecs_table_index_t *map, ecs_type_t type, const struct ecs_component_index_s *component_index) {
    uint32_t hash = ecs_type_hash(type);
    uint32_t slot_idx = hash & map->slot_mask;

    // Fast path: lookup
    while (map->slots[slot_idx].table_index != UINT32_MAX) {
        if (ECS_LIKELY(map->slots[slot_idx].hash == hash)) {
            ecs_table_t *table = ecs_table_index_at(map, map->slots[slot_idx].table_index);
            if (ECS_LIKELY(ecs_type_equals(table->type.ids, table->type.count, type.ids, type.count))) {
                ecs_type_fini(&type);
                return (uint16_t)map->slots[slot_idx].table_index;
            }
        }
        slot_idx = (slot_idx + 1) & map->slot_mask;
    }

    // Slow path: creation
    if (ECS_UNLIKELY(map->tables.size >= map->slot_count * LOAD_FACTOR)) {
        ecs_table_index_resize(map);
        // re-calculate slot_idx after resize
        slot_idx = hash & map->slot_mask;
        while (map->slots[slot_idx].table_index != UINT32_MAX) {
            slot_idx = (slot_idx + 1) & map->slot_mask;
        }
    }

    uint32_t table_idx = map->tables.size;
    ecs_table_t new_table;
    ecs_table_init(&new_table, type, table_idx, component_index);
    ecs_vec_push(&map->tables, &new_table, sizeof(ecs_table_t));

    map->slots[slot_idx].hash = hash;
    map->slots[slot_idx].table_index = table_idx;

    return (uint16_t)table_idx;
}
