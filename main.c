#include "./ecs/world.h"
#include <stdint.h>

typedef struct {
    float x, y;
} Position, Velocity;

ECS_COMPONENT_DEFINE(Position);
ECS_COMPONENT_DEFINE(Velocity);
ECS_BIT_DEFINE(IsActive);

int main() {
    ecs_world_t *world = ecs_init();


    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);
    ECS_COMPONENT_REGISTER(world, IsActive);

    ecs_fini(world);
    return 0;
}
