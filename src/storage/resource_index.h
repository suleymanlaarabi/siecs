#ifndef SIECS_STORAGE_RESOURCE_INDEX_H
#define SIECS_STORAGE_RESOURCE_INDEX_H

#include "siecs.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char *name;
    uint64_t size;
    ecs_type_ops_t ops;
    ecs_resource_hook_t on_set;
    ecs_resource_hook_t on_remove;
} ecs_resource_record_t;

typedef struct {
    ecs_resource_record_t *records;
    void **data;
    bool *present;
    sicore_vec_t registration_order; // ecs_resource_t
    uint64_t capacity;
    uint64_t count;
} ecs_resource_index_t;

void ecs_resource_index_init();
void ecs_resource_index_fini();

ecs_resource_t ecs_resource_index_register(
    ecs_resource_t id,
    const ecs_resource_desc_t *desc
);
ecs_resource_t ecs_resource_index_find(const char *name);
bool ecs_resource_index_is_registered(ecs_resource_t id);
void ecs_resource_index_set(
    ecs_resource_t id,
    const void *data
);
void ecs_resource_index_move(
    ecs_resource_t id,
    void *data
);
void *ecs_resource_index_get(ecs_resource_t id);
bool ecs_resource_index_has(ecs_resource_t id);
void ecs_resource_index_remove(ecs_resource_t id);

#endif
