#include "./ecs/world.h"

typedef struct {
    float x, y;
} Position, Velocity;

ECS_COMPONENT_DEFINE(Position);
ECS_COMPONENT_DEFINE(Velocity);

void on_position_added(ecs_world_t *world, ecs_entity_t) { ecs_new(world); }

int main() {
    ecs_world_t *world = ecs_init();

    ecs_new(world);

    // ECS_COMPONENT_REGISTER(world, Position);
    // ECS_COMPONENT_REGISTER(world, Velocity);

    // ecs_observer_trigger(world, ecs_new(world), OnAdd);

    // ecs_query(world, { .required = { ecs_id(Position), ecs_id(Velocity) } });

    ecs_fini(world);
}
