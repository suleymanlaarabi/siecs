#ifndef SIECS_COMPONENT_REQUIRE_H
#define SIECS_COMPONENT_REQUIRE_H
#include "siecs.h"
#include "storage/component_index.h"
#include "table.h"
#include "type.h"
#include <stdint.h>

#define ECS_ADD_PLAN_MAX_COMPONENTS 16

ecs_type_t ecs_type_with_requirements(
    ecs_world_t *world,
    ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec
);

#ifndef NDEBUG
bool ecs_component_requires(
    const ecs_world_t *world,
    ecs_component_t component,
    ecs_component_t require
);
#endif

#endif
