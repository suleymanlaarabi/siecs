#pragma once
#include "ecs/datastructure/vec.h"
#include "ecs/world.h"
#include <stdint.h>

typedef struct {
    uint64_t bloom;
    ecs_component_t *required;
    ecs_component_t *excluded;
    uint16_t required_count;
    uint16_t excluded_count;
} ecs_query_t;

typedef struct {
    ecs_query_t query;
    ecs_vec_t matches; // uint16_t table ids
} ecs_query_cache_t;

typedef struct {
    ecs_vec_t queries;
} ecs_query_index_t;

void ecs_query_index_init(ecs_query_index_t *index);
void ecs_query_index_fini(ecs_query_index_t *index);
uint32_t ecs_query_index_create(ecs_query_index_t *index,  const ecs_query_desc_t *desc);
