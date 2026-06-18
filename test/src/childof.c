#include "siecs.h"
#include <siecs_test.h>

ECS_RELATION_DECLARE(Targets);
ECS_RELATION_DEFINE(Targets, 0);

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

void childof_relation_without_cascade_keeps_related_alive(void) {
    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_REGISTER(world, Targets);

    ecs_entity_t target = ecs_new(world);
    ecs_entity_t source = ecs_new(world);

    ecs_set(world, source, Targets, { target });
    test_true(ecs_has(world, source, Targets));
    test_true(ecs_has_cid(world, target, ecs_source(Targets)));

    ecs_kill(world, target);

    test_true(ecs_is_alive(world, source));
    test_false(ecs_has(world, source, Targets));

    ecs_fini(world);
}

void childof_relation_remove_updates_source(void) {
    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_REGISTER(world, Targets);

    ecs_entity_t target = ecs_new(world);
    ecs_entity_t source = ecs_new(world);

    ecs_set(world, source, Targets, { target });
    test_true(ecs_has_cid(world, target, ecs_source(Targets)));

    ecs_remove(world, source, Targets);

    test_false(ecs_has_cid(world, target, ecs_source(Targets)));
    test_true(ecs_is_alive(world, source));
    test_true(ecs_is_alive(world, target));

    ecs_fini(world);
}
