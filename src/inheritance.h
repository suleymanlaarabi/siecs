#ifndef SIECS_INHERITANCE_H
#define SIECS_INHERITANCE_H

#include "siecs.h"
#include "table.h"
#include "type.h"
#include <stdint.h>

typedef struct {
    ecs_component_t *ids;
    uint16_t count;
} ecs_inheritance_plan_t;

/*
 * Instantiate base's owned ChildOf subtree below entity as an IsA snapshot.
 */
void ecs_inheritance_instantiate_children(ecs_entity_t entity, ecs_entity_t base);

/* Consume type and add the owned components supplied by base. */
ecs_type_t
ecs_inheritance_materialize(ecs_type_t type, ecs_entity_t base, ecs_inheritance_plan_t *plan);

void ecs_inheritance_plan_fini(ecs_inheritance_plan_t *plan);

/* Copy the effective values from base into newly materialized child columns. */
void ecs_inheritance_plan_copy(
    const ecs_inheritance_plan_t *plan,
    ecs_entity_t base,
    ecs_table_t *child_table,
    uint32_t child_row
);

#endif
