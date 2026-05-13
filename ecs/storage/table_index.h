#pragma once
#include "../datastructure/vec.h"
#include "../storage/component_index.h"
#include "../type.h"
#include <stdint.h>

typedef struct {
    uint32_t hash;
    uint32_t table_index; // UINT32_MAX for empty
} ecs_type_slot_t;

typedef struct {
    ecs_vec_t tables; // ecs_table_t
    ecs_type_slot_t *slots;
    uint32_t slot_count;
    uint32_t slot_mask;
} ecs_table_index_t;

void ecs_table_index_init(ecs_table_index_t *map);
void ecs_table_index_fini(ecs_table_index_t *map);

#define ecs_table_index_at(map, index) ecs_vec_get_mut(&(map)->tables, index, ecs_table_t)

uint16_t ecs_table_index_get_or_create(
    ecs_table_index_t *map,
    ecs_type_t type,
    const struct ecs_component_index_s *component_index
);
