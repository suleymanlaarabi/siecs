#include <siecs.h>

ECS_COMPONENT(Position, {
    float x;
    float y;
});

ECS_COMPONENT(Velocity, {
    float x;
    float y;
});

static void move(ecs_iter_t *it) {
    Position *positions = ecs_field(it, 0);
    const Velocity *velocities = ecs_field(it, 1);

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
        .query.terms = { ecs_inout(Position), ecs_in(Velocity) },
        .callback = move,
        .phase = EcsOnUpdate,
    });

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, Position, { .x = 1.0f, .y = 2.0f });
    ecs_set(world, entity, Velocity, { .x = 3.0f, .y = 4.0f });

    ecs_progress(world);

    const Position *position = ecs_get(world, entity, Position);
    const int ok = position->x == 4.0f && position->y == 6.0f;

    ecs_fini(world);

    return ok ? 0 : 1;
}
