#ifndef SIECS_COMPONENT_REQUIRE_H
#define SIECS_COMPONENT_REQUIRE_H

#include "table.h"

void ecs_collect_required_components(
    const ecs_table_t *table,
    ecs_component_t component,
    ecs_component_t *ids,
    uint16_t *count,
    uint16_t capacity
);

#endif
