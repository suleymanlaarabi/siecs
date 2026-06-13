#include "siecs.h"

ECS_COMPONENT(Position, {
    float x;
    float y;
});

ECS_COMPONENT(Velocity, {
    float x;
    float y;
});

void Move(ecs_iter_t *it) {
    Position *positions = ecs_field(it, 0);
    Velocity *velocities = ecs_field(it, 1);

    for (uint32_t i = 0; i < it->count; i++) {
        positions[i].x += velocities[i].x;
        positions[i].y += velocities[i].y;
    }
}

int main(void) {
    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);

    ecs_system(world, {
        .query.read = { ecs_id(Position), ecs_id(Velocity) }
    });

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, Position, {0, 0});
    ecs_set(world, entity, Velocity, {1, 1});

    while (ecs_progress(world)) {}

    ecs_fini(world);
}
