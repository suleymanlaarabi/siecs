#ifndef SIECS_STORAGE_RESOURCE_INDEX_H
#define SIECS_STORAGE_RESOURCE_INDEX_H

#include "siecs.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    ecs_resource_desc_t *records;
    void **data;
    bool *present;
    uint64_t capacity;
    uint64_t count;
} ecs_resource_index_t;

void ecs_resource_index_init(ecs_resource_index_t *index);
void ecs_resource_index_fini(ecs_resource_index_t *index, ecs_world_t *world);

ecs_resource_t ecs_resource_index_register(
    ecs_resource_index_t *index,
    const ecs_resource_desc_t *desc
);

void ecs_resource_index_set(
    ecs_resource_index_t *index,
    ecs_world_t *world,
    ecs_resource_t id,
    const void *data
);
void *ecs_resource_index_get(ecs_resource_index_t *index, ecs_resource_t id);
const void *ecs_resource_index_get_const(const ecs_resource_index_t *index, ecs_resource_t id);
bool ecs_resource_index_has(const ecs_resource_index_t *index, ecs_resource_t id);
void ecs_resource_index_remove(ecs_resource_index_t *index, ecs_world_t *world, ecs_resource_t id);

#endif
