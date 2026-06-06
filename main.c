#include "./ecs/world.h"

#include <stdint.h>

typedef struct {
    float x, y;
} Position, Velocity;

ECS_COMPONENT_DEFINE(Position);
ECS_COMPONENT_DEFINE(Velocity);

int main() {
    ecs_world_t *world = ecs_init();
    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child_a = ecs_new(world);
    ecs_entity_t child_b = ecs_new(world);

    ecs_set(world, child_a, ChildOf, { parent });
    ecs_set(world, child_b, ChildOf, { parent });

    ecs_kill(world, parent);

    ecs_fini(world);
}
