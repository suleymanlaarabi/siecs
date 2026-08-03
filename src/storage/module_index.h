#ifndef SIECS_STORAGE_MODULE_INDEX_H
#define SIECS_STORAGE_MODULE_INDEX_H

#include "siecs.h"

typedef struct {
    ecs_module_id_t *id;
    const char *name;

    sicore_vec_t observers; // ecs_observer_id_t
    sicore_vec_t systems;   // ecs_system_id_t
    bool enabled;
} ecs_module_t;

typedef struct {
    sicore_vec_t modules; // ecs_module_t
} ecs_module_index_t;

void ecs_module_index_init();
void ecs_module_index_fini();

ecs_module_id_t ecs_module_index_create(ecs_module_id_t *id, const char *name);
ecs_module_t *ecs_module_index_get(ecs_module_id_t module);
const ecs_module_t *ecs_module_index_get_const(ecs_module_id_t module);
ecs_module_id_t ecs_module_index_find(const ecs_module_id_t *id);

#endif
