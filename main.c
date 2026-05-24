#include "./ecs/world.h"
#include <stdint.h>

typedef struct {
    float x, y;
} Position, Velocity;

ECS_COMPONENT_DEFINE(Position);
ECS_COMPONENT_DEFINE(Velocity);
ECS_BIT_DEFINE(IsActive);

ECS_COMPONENT_DECLARE(Vec3, { float x; })

int main() {
    ecs_world_t *world = ecs_init();

    ecs_entity_t entity = ecs_new(world);

    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);

    ecs_add(world, entity, Position);

    ecs_query_id_t qid = ecs_query(
        world,
        {
            .required = { ecs_id(Position) },
        }
    );

    // ecs_observer(world, {
    //     .on = OnAdd,
    //     .query = {
    //         .required = {
    //             ecs_id(Player)
    //         }
    //     },
    //     .callback = on_add_player
    // });

    ecs_iter_t it = ecs_query_iter(world, qid);

    while (ecs_iter_next(&it)) {
        Position *positions = ecs_field(&it, ecs_id(Position));
        Velocity *velocities = ecs_field(&it, ecs_id(Velocity));
        ecs_bitfield_t is_active = ecs_bitfield(&it, ecs_id(IsActive));

        ECS_BITS_FOREACH_SET(is_active, i, {
            positions[i].x += velocities[i].x;
            positions[i].y += velocities[i].y;
        });
    }

    ecs_new(world);
    ecs_add_cid(world, entity, ecs_id(Position));

    ecs_fini(world);
    return 0;
}
