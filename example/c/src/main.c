#include "siecs.h"
#include "sijson.h"
#include "world_internal.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

ECS_COMPONENT(Position, {
    float x;
    float y;
});

ECS_COMPONENT(Velocity, {
    float x;
    float y;
});

ECS_COMPONENT(Girl, {});
ECS_COMPONENT(Boy, {});
ECS_RELATION(Maried, 0);

int main(void) {
    ecs_world_t *world = ecs_with_features({ .rest = true, .target_fps = 240 });

    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);
    ECS_COMPONENT_REGISTER(world, Girl);
    ECS_COMPONENT_REGISTER(world, Boy);

    ecs_entity_t human = ecs_new(world);
    ecs_set(world, human, Position, { 0, 0 });
    ecs_add(world, human, Abstract);

    ecs_entity_t player = ecs_new(world);
    ecs_is_a(world, player, human);

    ecs_entity_t suleyman = ecs_new(world);
    ecs_is_a(world, suleyman, human);

    ecs_entity_t mathilde = ecs_new(world);
    ecs_is_a(world, mathilde, human);

    ecs_set(world, suleyman, Maried, { mathilde });

    ecs_query_id_t qid = ecs_query(
        world,
        {
            .is_a = human,
            .terms = { ecs_filter(Maried), ecs_filter(Boy) },
        }
    );

    ecs_iter_t it = ecs_query_iter(world, qid);

    while (ecs_iter_next(&it)) {
        Maried *maried = ecs_field(&it, 0);

        for (uint32_t i = 0; i < it.count; i++) {
            if (maried->target == mathilde) {
                // on a trouver une personne marier a mathilde
            }
        }
    }

    ecs_fini(world);
}
