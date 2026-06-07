#pragma once
#include "../datastructure/vec.h"
#include "../world.h"
#include "ecs/datastructure/map.h"
#include <stdint.h>

typedef struct {
    const char *name;
    uint16_t *required;
    uint32_t required_count;
    uint32_t size;
    ecs_component_hook_t on_set;
    ecs_component_hook_t on_remove;
    ecs_vec_t tables; // uint16_t
} ecs_component_record_t;

typedef struct ecs_component_index_s {
    ecs_vec_t components; // ecs_component_record_t
    #ifndef NDEBUG
    ecs_map_t component_name_map;
    #endif
} ecs_component_index_t;

ecs_component_t ecs_component_index_create(
    ecs_component_index_t *index,
    const char *name,
    uint64_t size,
    ecs_component_hook_t on_set,
    ecs_component_hook_t on_remove
);

#define ecs_component_index_get(index, id)                                                         \
    ecs_vec_get(&(index)->components, id, ecs_component_record_t)
#define ecs_component_index_get_mut(index, id)                                                         \
    ecs_vec_get_mut(&(index)->components, id, ecs_component_record_t)

void ecs_component_index_init(ecs_component_index_t *index);
void ecs_component_index_fini(ecs_component_index_t *index);
