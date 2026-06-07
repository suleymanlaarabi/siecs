#include "../table.h"
#include "ecs/storage/component_index.h"
#include "ecs/storage/observer_index.h"
#include "ecs/storage/query_index.h"
#include "ecs/type.h"
#include "ecs/world.h"
#include "ecs/world_internal.h"
#include <stdint.h>
#include <stdlib.h>

#define INITIAL_SLOT_SHIFT 4
#define LOAD_FACTOR 0.75
#define ECS_TABLE_SLOT_EMPTY UINT16_MAX

static inline uint32_t ecs_type_hash(ecs_type_t type) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < type.count; ++i) {
        h ^= (uint32_t)type.ids[i];
        h *= 16777619u;
    }

    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

static inline uint16_t ecs_type_hash_fingerprint(uint32_t hash) {
    return (uint16_t)(hash ^ (hash >> 16));
}

static inline uint32_t ecs_table_index_slot_count(const ecs_table_index_t *map) {
    return 1u << map->slot_shift;
}

static inline void ecs_table_index_init_slots(ecs_table_index_t *map) {
    uint32_t slot_count = ecs_table_index_slot_count(map);
    map->slots = malloc(sizeof(ecs_type_slot_t) * slot_count);
    for (uint32_t i = 0; i < slot_count; ++i) {
        map->slots[i].table_index = ECS_TABLE_SLOT_EMPTY;
    }
}

static inline void
ecs_table_index_insert_slot(ecs_table_index_t *map, uint32_t hash, uint16_t table_index) {
    uint32_t slot_mask = ecs_table_index_slot_count(map) - 1;
    uint32_t slot_idx = hash & slot_mask;
    while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
        slot_idx = (slot_idx + 1) & slot_mask;
    }
    map->slots[slot_idx].hash = ecs_type_hash_fingerprint(hash);
    map->slots[slot_idx].table_index = table_index;
}

void ecs_table_index_init(ecs_table_index_t *map) {
    map->table_count = 0;
    map->table_capacity = 1;
    map->tables = malloc(sizeof(ecs_table_t) * map->table_capacity);
    map->slot_shift = INITIAL_SLOT_SHIFT;
    ecs_table_index_init_slots(map);
}

void ecs_table_index_fini(ecs_table_index_t *map) {
    for (uint16_t i = 0; i < map->table_count; i++) {
        ecs_table_fini(&map->tables[i]);
    }
    free(map->tables);
    free(map->slots);
}

static void ecs_table_index_resize(ecs_table_index_t *map) {
    ecs_type_slot_t *old_slots = map->slots;

    map->slot_shift += 1;
    ecs_table_index_init_slots(map);
    for (uint16_t i = 0; i < map->table_count; ++i) {
        ecs_table_index_insert_slot(map, ecs_type_hash(map->tables[i].type), i);
    }
    free(old_slots);
}

static void ecs_table_index_grow_tables(ecs_table_index_t *map) {
    map->table_capacity *= 2;
    map->tables = realloc(map->tables, sizeof(ecs_table_t) * map->table_capacity);
}

uint16_t ecs_table_index_get_or_create(ecs_world_t *world, ecs_type_t type) {
    const ecs_component_index_t *component_index = &world->component_index;
    ecs_table_index_t *map = &world->table_index;
    uint32_t hash = ecs_type_hash(type);
    uint16_t hash_fingerprint = ecs_type_hash_fingerprint(hash);
    uint32_t slot_mask = ecs_table_index_slot_count(map) - 1;
    uint32_t slot_idx = hash & slot_mask;

    // Fast path: lookup
    while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
        if (ECS_LIKELY(map->slots[slot_idx].hash == hash_fingerprint)) {
            const ecs_table_t *table = ecs_table_index_at(map, map->slots[slot_idx].table_index);
            if (ECS_LIKELY(
                    ecs_type_equals(table->type.ids, table->type.count, type.ids, type.count)
                )) {
                ecs_type_fini(&type);
                return (uint16_t)map->slots[slot_idx].table_index;
            }
        }
        slot_idx = (slot_idx + 1) & slot_mask;
    }

    // Slow path: creation
    if (ECS_UNLIKELY(map->table_count >= ecs_table_index_slot_count(map) * LOAD_FACTOR)) {
        ecs_table_index_resize(map);
        // re-calculate slot_idx after resize
        slot_mask = ecs_table_index_slot_count(map) - 1;
        slot_idx = hash & slot_mask;
    }
    if (ECS_UNLIKELY(map->table_count >= map->table_capacity)) {
        ecs_table_index_grow_tables(map);
    }

    uint16_t table_idx = map->table_count++;
    ecs_table_t new_table;
    ecs_table_init(&new_table, type, component_index, table_idx);
    map->tables[table_idx] = new_table;

    map->slots[slot_idx].hash = hash_fingerprint;
    map->slots[slot_idx].table_index = table_idx;

    ecs_query_index_add_table(&world->query_index, ecs_table_index_at(map, table_idx), table_idx);
    ecs_observer_index_add_table(&world->observer_index, ecs_table_index_at(map, table_idx));
    return (uint16_t)table_idx;
}
