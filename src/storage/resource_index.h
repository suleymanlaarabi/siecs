#ifndef SIECS_STORAGE_RESOURCE_INDEX_H
#define SIECS_STORAGE_RESOURCE_INDEX_H

#include "component_index.h"
#include "siecs.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void **data;
    bool *present;
    uint64_t capacity;
} ecs_resource_index_t;

void ecs_resource_index_init(ecs_resource_index_t *index);
void ecs_resource_index_fini(
    ecs_resource_index_t *index,
    ecs_world_t *world,
    const ecs_component_index_t *component_index
);

void ecs_resource_index_set(
    ecs_resource_index_t *index,
    ecs_world_t *world,
    const ecs_component_index_t *component_index,
    ecs_component_t id,
    const void *data
);
void *ecs_resource_index_get(ecs_resource_index_t *index, ecs_component_t id);
const void *ecs_resource_index_get_const(const ecs_resource_index_t *index, ecs_component_t id);
bool ecs_resource_index_has(const ecs_resource_index_t *index, ecs_component_t id);
void ecs_resource_index_remove(
    ecs_resource_index_t *index,
    ecs_world_t *world,
    const ecs_component_index_t *component_index,
    ecs_component_t id
);

#endif
