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
    ecs_entity_t target;
    uint16_t *tables;
    ecs_relation_id_t relation;
    uint16_t table_count;
    uint16_t table_capacity;
    uint16_t first_table;
} ecs_relation_table_slot_t;

typedef struct {
    const uint16_t *ids;
    uint16_t count;
} ecs_relation_tables_t;

typedef struct {
    ecs_table_t *tables;
    ecs_type_slot_t *slots;
    uint16_t table_count;
    uint16_t table_capacity;
    uint8_t slot_shift; // slot_count = 1 << slot_shift
    ecs_relation_table_slot_t *relation_slots;
    uint32_t relation_slot_count;
    uint8_t relation_slot_shift;
} ecs_table_index_t;

void ecs_table_index_init();
void ecs_table_index_fini();

#define ecs_table_index_at(index) (&ecs_world.table_index.tables[index])

uint16_t ecs_table_index_get_or_create(
    ecs_type_t type
);
ecs_relation_tables_t
ecs_table_index_relation_tables(ecs_relation_id_t relation, ecs_entity_t target);

#endif
