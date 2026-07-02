#ifndef SIECS_COMPONENT_REQUIRE_H
#define SIECS_COMPONENT_REQUIRE_H
#include "siecs.h"
#include "storage/component_index.h"
#include "table.h"
#include "type.h"
#include <stdint.h>

typedef struct {
    ecs_type_t type;
    ecs_component_t inline_added[32];
    ecs_component_t *added;
    uint16_t added_count;
    uint16_t added_capacity;
} ecs_add_plan_t;

void ecs_add_plan_init(ecs_add_plan_t *plan);
void ecs_add_plan_fini(ecs_add_plan_t *plan);

void ecs_add_plan_build_type(
    ecs_world_t *world,
    ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec,
    ecs_add_plan_t *plan
);

void ecs_add_plan_build_added_only(
    ecs_world_t *world,
    ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec,
    ecs_add_plan_t *plan
);

#ifndef NDEBUG
bool ecs_component_requires(
    const ecs_world_t *world,
    ecs_component_t component,
    ecs_component_t require
);
#endif

#endif
