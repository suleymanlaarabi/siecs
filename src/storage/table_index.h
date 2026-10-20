#ifndef SIECS_STORAGE_TABLE_INDEX_H
#define SIECS_STORAGE_TABLE_INDEX_H
#include "../table.h"
#include "../type.h"
#include <stdint.h>

struct ecs_world_s;

typedef struct {
    uint16_t table_index; // UINT16_MAX for empty
    uint16_t hash;
} ecs_type_slot_t;

typedef struct {
    uint64_t value; // zero marks an empty slot
    uint16_t *tables;
    uint16_t key; // zero indexes IsA base tables
    uint16_t table_count;
    uint16_t table_capacity;
    uint16_t first_table;
} ecs_pair_table_slot_t;

typedef struct {
    const uint16_t *ids;
    uint16_t count;
} ecs_pair_tables_t;

#define ECS_PAIR_TARGET_BLOOM_BITS 4096

typedef struct {
    ecs_table_t *tables;
    ecs_type_slot_t *slots;
    uint16_t table_count;
    uint32_t table_capacity;
    uint8_t slot_shift; // slot_count = 1 << slot_shift
    ecs_pair_table_slot_t *pair_slots;
    uint32_t pair_slot_count;
    uint8_t pair_slot_shift;
    // One bit per entity index (modulo the size), set once a pair slot exists
    // for a target with that index. A clear bit proves the entity is the target
    // of no pair table, so destroying it needs no source cleanup.
    uint64_t pair_target_bloom[ECS_PAIR_TARGET_BLOOM_BITS / 64];
} ecs_table_index_t;

extern ecs_table_index_t table_index;

void ecs_table_index_init();
void ecs_table_index_fini();

#define ecs_table_index_at(index) (&table_index.tables[index])

uint16_t ecs_table_index_get_or_create(ecs_type_t type);

static inline bool ecs_table_index_may_be_pair_target(uint64_t target) {
    uint32_t bit = ecs_first(target) & (ECS_PAIR_TARGET_BLOOM_BITS - 1);
    return (table_index.pair_target_bloom[bit / 64] >> (bit % 64)) & 1;
}
ecs_pair_tables_t ecs_table_index_pair_tables(uint16_t key, uint64_t value);

#endif
