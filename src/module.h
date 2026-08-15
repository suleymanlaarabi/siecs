#ifndef SIECS_MODULE_H
#define SIECS_MODULE_H

#include "siecs.h"
typedef struct {
    ecs_module_id_t *id;
    const char *name;
    ecs_observer_id_t observer;
    ecs_system_id_t system;
    bool enabled;
} ecs_module_t;

void ecs_module_record_system(ecs_system_id_t system);
void ecs_module_record_observer(ecs_observer_id_t observer);

#endif
