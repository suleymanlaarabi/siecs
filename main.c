#include "./ecs/world.h"
#include <assert.h>
#include <stdint.h>

typedef struct {
    float x, y;
} Position, Velocity;

ECS_COMPONENT_DEFINE(Position);
ECS_COMPONENT_DEFINE(Velocity);
ECS_BIT_DEFINE(IsActive);

void OnAddPosition(ecs_world_t *world, ecs_entity_t entity) {
    ecs_get(world, entity, Position)->x = 10;
    ecs_get(world, entity, Position)->y = 10;
}

void OnSetPosition(ecs_world_t *world, ecs_entity_t entity) {
    ecs_get(world, entity, Position)->x = 20;
    ecs_get(world, entity, Position)->y = 20;
}

int main() {
    ecs_world_t *world = ecs_init();

    // ECS_COMPONENT_REGISTER(world, Position);

    // ecs_observer(
    //     world,
    //     {
    //         .on = OnAdd,
    //         .query = {},
    //         .callback = OnAddPosition,
    //     }
    // );

    // ecs_entity_t entity = ecs_new(world);

    // ecs_add(world, entity, Position);

    // ecs_set(
    //     world,
    //     entity,
    //     Position,
    //     {
    //         .x = 15,
    //         .y = 15,
    //     }
    // );

    ecs_fini(world);
    return 0;
}
