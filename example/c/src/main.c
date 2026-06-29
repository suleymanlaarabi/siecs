#include "siecs.h"
#include "sijson.h"
#include <assert.h>
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

    ecs_entity_t animal = ecs_new(world);
    ecs_set(world, animal, Position, { 0, 0 });

    ecs_entity_t human = ecs_new(world);
    ecs_is_a(world, human, animal);

    ecs_entity_t player = ecs_new(world);
    ecs_is_a(world, player, human);

    assert(ecs_has(world, player, Position));

    ecs_fini(world);
}
