#ifndef SIECS_COMPONENT_REQUIRE_H
#define SIECS_COMPONENT_REQUIRE_H

#include "table.h"

#define ECS_ADD_PLAN_MAX_COMPONENTS 32

ecs_type_t ecs_type_with_requirements(
    ecs_table_t *from_table,
    ecs_component_t cid
);

void ecs_collect_required_components(
    const ecs_table_t *table,
    ecs_component_t component,
    ecs_component_t *ids,
    uint16_t *count,
    uint16_t capacity
);

#endif
