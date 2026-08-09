#ifndef SIECS_INHERITANCE_H
#define SIECS_INHERITANCE_H

#include "siecs.h"
#include "type.h"
#include "table.h"
#include <stdint.h>

typedef struct {
    ecs_component_t *ids;
    uint16_t count;
} ecs_inheritance_plan_t;

/* Collect components that must become owned when a type inherits from base. */
void ecs_inheritance_plan_build(
    const ecs_type_t *child_type,
    ecs_entity_t base,
    ecs_inheritance_plan_t *plan
);

void ecs_inheritance_plan_fini(ecs_inheritance_plan_t *plan);

/* Copy the effective values from base into newly materialized child columns. */
void ecs_inheritance_plan_copy(
    const ecs_inheritance_plan_t *plan,
    ecs_entity_t base,
    ecs_table_t *child_table,
    uint32_t child_row
);

#endif
