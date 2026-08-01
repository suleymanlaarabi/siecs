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
    ecs_table_t *tables;
    ecs_type_slot_t *slots;
    uint16_t table_count;
    uint16_t table_capacity;
    uint8_t slot_shift; // slot_count = 1 << slot_shift
} ecs_table_index_t;

void ecs_table_index_init();
void ecs_table_index_fini();

#define ecs_table_index_at(index) (&ecs_world.table_index.tables[index])

ECS_INTERNAL_API uint16_t ecs_table_index_get_or_create(
    ecs_type_t type
);

#endif
