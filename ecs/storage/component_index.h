#pragma once
#include "../datastructure/vec.h"
#include "../world.h"
#include <stdint.h>


typedef struct {
    const char *name;
    uint16_t *required;
    uint32_t required_count;
    uint32_t size;
} ecs_component_record_t;

typedef struct ecs_component_index_s {
    ecs_vec_t components; // ecs_component_record_t
} ecs_component_index_t;

ecs_component_t ecs_component_index_create(
    ecs_component_index_t *index,
    const char *name,
    uint64_t size
);

#define ecs_component_index_get(index, id)                                                         \
    ecs_vec_get(&(index)->components, id, ecs_component_record_t)

void ecs_component_index_init(ecs_component_index_t *index);
void ecs_component_index_fini(ecs_component_index_t *index);
