#include "siecs.h"
#include <string.h>

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

ECS_MODULE_DECLARE(physics, {});

ECS_MODULE_DEFINE(physics);

void physics_import(ecs_world_t *world, const physics_props_t *props) {
    (void)props;

    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);

    ecs_system(
        world,
        {
            .query.terms = { ecs_inout(Position), ecs_in(Velocity) },
            .callback = Move,
            .phase = EcsOnUpdate,
        }
    );
}

int main(void) {
    ecs_world_t *world = ecs_with_features({ .rest = true });

    ECS_MODULE_IMPORT(world, physics, {});

    ecs_entity_t player = ecs_new(world);
    ecs_set(world, player, Position, { 0, 0 });
    ecs_set(world, player, Velocity, { 1, 1 });
    ecs_set(world, player, Name, { .value = strdup("Player") });

    ecs_entity_t child = ecs_new(world);
    ecs_set(world, child, ChildOf, { player });
    ecs_set(world, child, Name, { .value = strdup("Child") });

    ecs_entity_t enemy = ecs_new(world);
    ecs_set(world, enemy, Name, { .value = strdup("Enemy") });

    while (ecs_progress(world)) {
    }

    ecs_fini(world);
}
