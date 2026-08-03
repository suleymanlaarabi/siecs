#include "../table.h"
#include "../type.h"
#include "../world_internal.h"
#include "component_index.h"
#include "observer_index.h"
#include "query_index.h"
#include "../relation.h"
#include <stdint.h>
#include <stdlib.h>

#define INITIAL_SLOT_SHIFT 12
#define INITIAL_RELATION_SLOT_SHIFT 3
#define LOAD_FACTOR 0.75
#define ECS_TABLE_SLOT_EMPTY UINT16_MAX

static inline uint32_t ecs_type_hash(ecs_type_t type) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < type.component_count; ++i) {
        h ^= (uint32_t)type.ids[i];
        h *= 16777619u;
    }
    const ecs_relation_t *relations = ecs_type_relations(&type);
    for (uint16_t i = 0; i < type.relation_count; i++) {
        h ^= relations[i].relation_id;
        h *= 16777619u;
        h ^= relations[i].target_generation;
        h *= 16777619u;
        h ^= relations[i].target_index;
        h *= 16777619u;
    }
    h ^= (uint32_t)type.base;
    h *= 16777619u;
    h ^= (uint32_t)(type.base >> 32);
    h *= 16777619u;

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

static uint32_t ecs_relation_hash(ecs_relation_id_t relation, ecs_entity_t target) {
    uint64_t value = target ^ ((uint64_t)relation * UINT64_C(0x9e3779b97f4a7c15));
    value ^= value >> 33;
    value *= UINT64_C(0xff51afd7ed558ccd);
    return (uint32_t)(value ^ (value >> 33));
}

static uint32_t ecs_relation_slot_capacity(const ecs_table_index_t *index) {
    return index->relation_slot_shift ? 1u << index->relation_slot_shift : 0;
}

static void ecs_relation_slot_insert(
    ecs_relation_table_slot_t *slots,
    uint32_t mask,
    ecs_relation_table_slot_t slot
) {
    uint32_t i = ecs_relation_hash(slot.relation, slot.target) & mask;
    while (slots[i].relation) {
        i = (i + 1) & mask;
    }
    slots[i] = slot;
}

static void ecs_relation_slots_grow(ecs_table_index_t *index) {
    uint32_t old_capacity = ecs_relation_slot_capacity(index);
    ecs_relation_table_slot_t *old = index->relation_slots;
    index->relation_slot_shift = index->relation_slot_shift
                                     ? index->relation_slot_shift + 1
                                     : INITIAL_RELATION_SLOT_SHIFT;
    uint32_t capacity = ecs_relation_slot_capacity(index);
    index->relation_slots = calloc(capacity, sizeof(ecs_relation_table_slot_t));
    for (uint32_t i = 0; i < old_capacity; i++) {
        if (old[i].relation) {
            ecs_relation_slot_insert(index->relation_slots, capacity - 1, old[i]);
        }
    }
    free(old);
}

static ecs_relation_table_slot_t *ecs_relation_slot(
    ecs_table_index_t *index,
    ecs_relation_id_t relation,
    ecs_entity_t target,
    bool create
) {
    if (!index->relation_slot_shift ||
        (create && (index->relation_slot_count + 1) * 4 >=
                       ecs_relation_slot_capacity(index) * 3)) {
        if (!create) {
            return NULL;
        }
        ecs_relation_slots_grow(index);
    }
    uint32_t mask = ecs_relation_slot_capacity(index) - 1;
    uint32_t i = ecs_relation_hash(relation, target) & mask;
    while (index->relation_slots[i].relation &&
           (index->relation_slots[i].relation != relation ||
            index->relation_slots[i].target != target)) {
        i = (i + 1) & mask;
    }
    ecs_relation_table_slot_t *slot = &index->relation_slots[i];
    if (!slot->relation && create) {
        slot->relation = relation;
        slot->target = target;
        index->relation_slot_count++;
    }
    return slot->relation ? slot : NULL;
}

ecs_relation_tables_t
ecs_table_index_relation_tables(ecs_relation_id_t relation, ecs_entity_t target) {
    ecs_relation_table_slot_t *slot =
        ecs_relation_slot(&ecs_world.table_index, relation, target, false);
    if (!slot) {
        return (ecs_relation_tables_t){ 0 };
    }
    return (ecs_relation_tables_t){
        .ids = slot->tables ? slot->tables : &slot->first_table,
        .count = slot->table_count,
    };
}

static void ecs_relation_slot_add_table(ecs_relation_table_slot_t *slot, uint16_t table) {
    if (!slot->table_count) {
        slot->first_table = table;
    } else {
        if (!slot->tables) {
            slot->table_capacity = 4;
            slot->tables = malloc(sizeof(uint16_t) * slot->table_capacity);
            slot->tables[0] = slot->first_table;
        } else if (slot->table_count == slot->table_capacity) {
            slot->table_capacity *= 2;
            slot->tables = realloc(slot->tables, sizeof(uint16_t) * slot->table_capacity);
        }
        slot->tables[slot->table_count] = table;
    }
    slot->table_count++;
}

static void ecs_table_index_relations(const ecs_table_t *table, uint16_t table_id) {
    const ecs_relation_t *relations = ecs_type_relations(&table->type);
    for (uint16_t i = 0; i < table->type.relation_count; i++) {
        const ecs_relation_record_t *record = ecs_relation_record(relations[i].relation_id);
        if (record->storage == EcsRelationByTarget) {
            ecs_relation_table_slot_t *slot = ecs_relation_slot(
                &ecs_world.table_index,
                relations[i].relation_id,
                ecs_relation_key_target(&relations[i]),
                true
            );
            ecs_relation_slot_add_table(slot, table_id);
        }
    }
}

static inline void ecs_table_index_init_slots(ecs_table_index_t *map) {
    uint32_t slot_count = ecs_table_index_slot_count(map);
    map->slots = malloc(sizeof(ecs_type_slot_t) * slot_count);
    memset(map->slots, 0xFF, sizeof(ecs_type_slot_t) * slot_count);
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

void ecs_table_index_init() {
    ecs_table_index_t *map = &ecs_world.table_index;
    map->table_count = 0;
    map->table_capacity = 1;
    map->tables = malloc(sizeof(ecs_table_t) * map->table_capacity);
    map->slot_shift = INITIAL_SLOT_SHIFT;
    ecs_table_index_init_slots(map);
    map->relation_slots = NULL;
    map->relation_slot_count = 0;
    map->relation_slot_shift = 0;
}

void ecs_table_index_fini() {
    ecs_table_index_t *map = &ecs_world.table_index;
    for (uint16_t i = 0; i < map->table_count; i++) {
        ecs_table_fini(&map->tables[i]);
    }
    uint32_t relation_capacity = ecs_relation_slot_capacity(map);
    for (uint32_t i = 0; i < relation_capacity; i++) {
        if (map->relation_slots[i].relation) {
            free(map->relation_slots[i].tables);
        }
    }
    free(map->relation_slots);
    free(map->tables);
    free(map->slots);
}

static void ecs_table_index_resize(ecs_table_index_t *map) {
    ecs_type_slot_t *old_slots = map->slots;

    map->slot_shift += 1;
    ecs_table_index_init_slots(map);
    for (uint16_t i = 0; i < map->table_count; ++i) {
        // Table-owned types retain their full hash, so resize never scans their ids again.
        ecs_table_index_insert_slot(map, map->tables[i].type.hash, i);
    }
    free(old_slots);
}

static void ecs_table_index_grow_tables(ecs_table_index_t *map) {
    map->table_capacity *= 2;
    map->tables = realloc(map->tables, sizeof(ecs_table_t) * map->table_capacity);
}

static bool ecs_table_index_inherits_component_before(
    const ecs_table_t *table,
    ecs_entity_t stop_base,
    ecs_component_t component
) {
    ecs_entity_t base = table->type.base;
    while (base != 0 && base != stop_base) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);
        if (ecs_table_column_or_invalid(base_table, component) != UINT16_MAX) {
            return true;
        }
        base = base_table->type.base;
    }
    return false;
}

static void ecs_table_index_register_inherited_components(ecs_table_t *table, uint16_t table_id) {
    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);

        for (uint16_t i = 0; i < base_table->type.component_count; i++) {
            ecs_component_t component = base_table->type.ids[i];
            if (ecs_table_column_or_invalid(table, component) != UINT16_MAX ||
                ecs_table_index_inherits_component_before(table, base, component)) {
                continue;
            }

            table->bloom |= 1ull << (component % 64);
            ecs_component_record_t *record = ecs_component_index_get_mut(component);
            sicore_vec_push_u16(&record->tables, table_id);
        }

        base = base_table->type.base;
    }
}

uint16_t ecs_table_index_get_or_create(ecs_type_t type) {
    ecs_table_index_t *map = &ecs_world.table_index;
    uint32_t hash = ecs_type_hash(type);
    uint16_t hash_fingerprint = ecs_type_hash_fingerprint(hash);
    uint32_t slot_mask = ecs_table_index_slot_count(map) - 1;
    uint32_t slot_idx = hash & slot_mask;

    // Fast path: lookup
    while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
        if (ECS_LIKELY(map->slots[slot_idx].hash == hash_fingerprint)) {
            const ecs_table_t *table = ecs_table_index_at(map->slots[slot_idx].table_index);
            if (ECS_LIKELY(ecs_type_equals(&table->type, &type))) {
                ecs_type_fini(&type);
                return (uint16_t)map->slots[slot_idx].table_index;
            }
        }
        slot_idx = (slot_idx + 1) & slot_mask;
    }

    // Slow path: creation
    if (ECS_UNLIKELY(map->table_count >= ecs_table_index_slot_count(map) * LOAD_FACTOR)) {
        ecs_table_index_resize(map);
        // Recalculate and probe again: rehashing existing tables may occupy the
        // new ideal slot even though the old table had an empty slot there.
        slot_mask = ecs_table_index_slot_count(map) - 1;
        slot_idx = hash & slot_mask;
        while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
            slot_idx = (slot_idx + 1) & slot_mask;
        }
    }
    if (ECS_UNLIKELY(map->table_count >= map->table_capacity)) {
        ecs_table_index_grow_tables(map);
    }

    uint16_t table_idx = map->table_count++;
    ecs_table_t new_table;
    // Persist the hash while transferring ownership of the type to the table.
    type.hash = hash;
    ecs_table_init(&new_table, type, table_idx);
    map->tables[table_idx] = new_table;
    ecs_table_index_relations(&map->tables[table_idx], table_idx);
    ecs_table_index_register_inherited_components(&map->tables[table_idx], table_idx);

    map->slots[slot_idx].hash = hash_fingerprint;
    map->slots[slot_idx].table_index = table_idx;

    ecs_query_index_add_table(ecs_table_index_at(table_idx), table_idx);
    ecs_observer_index_add_table(ecs_table_index_at(table_idx));
    return (uint16_t)table_idx;
}
