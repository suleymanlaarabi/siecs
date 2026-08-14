#ifndef SIECS_STORAGE_MODULE_INDEX_H
#define SIECS_STORAGE_MODULE_INDEX_H

#include "siecs.h"

typedef struct {
    ecs_module_id_t *id;
    const char *name;
    sicore_vec_t observers;
    sicore_vec_t systems;
    bool enabled;
} ecs_module_t;

#endif
