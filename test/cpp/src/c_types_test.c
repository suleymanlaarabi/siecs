#include "c_types_test.h"

uint32_t cpp_c_position_on_set_calls;
uint32_t cpp_c_time_on_set_calls;

static void cpp_c_position_on_set(
    ecs_entity_t entity,
    ecs_component_t component,
    const void *new_value,
    void *current_value
) {
    (void)entity;
    (void)component;
    (void)new_value;
    (void)current_value;
    cpp_c_position_on_set_calls++;
}

static void cpp_c_time_on_set(const void *ptr) {
    (void)ptr;
    cpp_c_time_on_set_calls++;
}

ECS_COMPONENT_DEFINE(
    cpp_c_position,
    .on_set = cpp_c_position_on_set
);

ECS_RESOURCE_DEFINE(
    cpp_c_time,
    .on_set = cpp_c_time_on_set
);

ECS_RELATION_DEFINE(cpp_c_parent, {
    .storage = EcsRelationByTarget,
    .on_delete_target = EcsRemoveRelation,
    .acyclic = true,
});
