#include "siecs.h"
#include "storage/table_index.h"

ECS_RELATION_DEFINE(ChildOf);

void ecs_bootstrap(ecs_world_t *world) {
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create(world, (ecs_type_t){ 0 });
    ecs_new(world);
    ecs_component(world, {});

    ECS_COMPONENT_REGISTER(world, ChildOf);
}
