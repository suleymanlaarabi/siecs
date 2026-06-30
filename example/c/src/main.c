#include "siecs.h"
#include "sijson.h"
#include <assert.h>
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

int main(void) {
    ecs_world_t *world = ecs_with_features({ .rest = true, .target_fps = 240 });

    ECS_COMPONENT_REGISTER(world, Position);

    ecs_entity_t animal = ecs_new(world);
    ecs_set(world, animal, Position, { 0, 0 });
    ecs_add(world, animal, Abstract);

    ecs_entity_t human = ecs_new(world);
    ecs_is_a(world, human, animal);
    ecs_add(world, human, Abstract);

    ecs_entity_t player = ecs_new(world);
    ecs_is_a(world, player, human);

    ecs_query_id_t qid = ecs_query(world, { .is_a = human });

    ecs_iter_t it = ecs_query_iter(world, qid);

    int count = 0;

    while (ecs_iter_next(&it)) {
        count += it.count;
    }

    printf("%d\n", it.count);

    assert(count == 1);

    while (ecs_progress(world)) {
    }

    ecs_fini(world);
}
