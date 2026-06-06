#include "ecs/storage/table_index.h"
#include "ecs/world.h"

ecs_component_t ecs_id(Name) = 0;

ECS_RELATION_DEFINE(ChildOf);

void ecs_bootstrap(ecs_world_t *world) {
    // Create empty table (index 0)
    ecs_table_index_get_or_create(world, (ecs_type_t){ 0 });

    // Reserve entity id 0
    ecs_new(world);

    // Reserve component id 0
    ecs_component(world, {});

    ecs_id(Name) = ecs_component(
        world,
        {
            .name = "Name",
            .size = sizeof(void *),
        }
    );

    ECS_COMPONENT_REGISTER(world, ChildOf);
}
