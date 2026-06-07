#include "./ecs/world.h"
#include "ecs/datastructure/vec.h"
#include "ecs/world_internal.h"

#include <stdint.h>

typedef struct {
    float x, y;
} Position, Velocity;

ECS_COMPONENT_DEFINE(Position);
ECS_COMPONENT_DEFINE(Velocity);

void PosVelSystem(ecs_iter_t *it) {}

int main() {
    ecs_world_t *world = ecs_init();
    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child_a = ecs_new(world);
    ecs_entity_t child_b = ecs_new(world);

    ecs_observer(world, {
        .on = OnAdd,
        .query = {
            .required = {
                ecs_id(Position),
                ecs_id(Velocity),
                ecs_id(ChildOf),
            },
        },
    });

    ecs_query_id_t query = ecs_query(
        world,
        {
            .read = { ecs_id(Position), ecs_id(Velocity) },
            .required = { ecs_source(ChildOf) },
            .excluded = { ecs_id(ChildOf) },
        }
    );

    ecs_system_id_t pos_vel_sys = ecs_system(world, {
        .query = {
            .read = { ecs_id(Position), ecs_id(Velocity) },
        },
    });

    ecs_system(
        world,
        {
            .callback = PosVelSystem,
            .after = { pos_vel_sys },
        }
    );

    ecs_set(world, child_a, ChildOf, { parent });
    ecs_set(world, child_b, ChildOf, { parent });

    ecs_kill(world, parent);

    ecs_fini(world);
}
