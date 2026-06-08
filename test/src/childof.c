#include "siecs.h"
#include <siecs_test.h>

void childof_kill_parent(void) {
    ecs_world_t *world = ecs_init();

    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child = ecs_new(world);

    ecs_set(world, child, ChildOf, { parent });
    for (int i = 0; i < 20; i++) {
        ecs_set(world, ecs_new(world), ChildOf, { parent });
    }
    test_true(ecs_is_alive(world, child));
    test_true(ecs_is_alive(world, parent));

    ecs_kill(world, parent);

    test_false(ecs_is_alive(world, child));
    test_false(ecs_is_alive(world, parent));

    ecs_fini(world);
}
