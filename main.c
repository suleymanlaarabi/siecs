#include "./ecs/world.h"
#include <stdint.h>

typedef struct {
    float x, y;
} Position, Velocity;

ECS_COMPONENT_DEFINE(Position)
ECS_COMPONENT_DEFINE(Velocity)

int main() {
    ecs_world_t *world = ecs_init();

    ecs_entity_t entity = ecs_new(world);

    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);

    ecs_add(world, entity, Position);

    ecs_query(world, {
        .required = {
            ecs_id(Position)
        },
    });


    ecs_new(world);
    ecs_add_cid(world, entity, ecs_id(Position));

    ecs_fini(world);
    return 0;
}
