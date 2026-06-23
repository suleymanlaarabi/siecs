#ifndef SIECS_STORAGE_COMPONENT_INDEX_H
#define SIECS_STORAGE_COMPONENT_INDEX_H
#include "../datastructure/map.h"
#include "../datastructure/vec.h"
#include "siecs.h"
#include "sireflect.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool registered;
    char *name;
    uint16_t *required;
    uint32_t required_count;
    uint32_t size;
    ecs_component_on_set_t on_set;
    ecs_component_on_remove_t on_remove;
    ecs_component_on_add_t on_add;
    uint32_t relation_flags;
    ecs_vec_t tables; // uint16_t
    sireflect_handle_t reflection;
} ecs_component_record_t;

typedef struct ecs_component_index_s {
    ecs_vec_t components; // ecs_component_record_t
#ifndef NDEBUG
    ecs_map_t component_name_map;
#endif
} ecs_component_index_t;

void ecs_component_index_register(
    ecs_component_index_t *index,
    ecs_component_t id,
    char *name,
    uint64_t size,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    uint32_t relation_flags,
    sireflect_handle_t reflection
);

#define ecs_component_index_get(index, id)                                                         \
    ecs_vec_get(&(index)->components, id, ecs_component_record_t)
#define ecs_component_index_get_mut(index, id)                                                     \
    ecs_vec_get_mut(&(index)->components, id, ecs_component_record_t)

void ecs_component_index_init(ecs_component_index_t *index);
void ecs_component_index_fini(ecs_component_index_t *index);

#endif
