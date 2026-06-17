#ifndef SIECS_MODULE_H
#define SIECS_MODULE_H

#include "siecs.h"
#include "storage/module_index.h"

void ecs_module_record_component(ecs_world_t *world, ecs_component_t component);
void ecs_module_record_system(ecs_world_t *world, ecs_system_id_t system);
void ecs_module_record_observer(ecs_world_t *world, ecs_observer_id_t observer);

#endif
