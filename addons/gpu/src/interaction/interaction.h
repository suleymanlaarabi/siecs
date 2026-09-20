#ifndef SIGPU_INTERACTION_H
#define SIGPU_INTERACTION_H

#include <sigpu.h>

void sigpu_interaction_init(ecs_system_id_t after);
uint16_t sigpu_interaction_component_id(const char *name);

#endif
