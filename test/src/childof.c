#include "siecs.h"
#include <siecs_test.h>

ECS_RELATION_DECLARE(Targets);
ECS_RELATION_DEFINE(Targets, 0);

void childof_kill_parent(void) {
    ecs_init();

    ecs_entity_t parent = ecs_new();
    ecs_entity_t child = ecs_new();

    ecs_set(child, ChildOf, { parent });
    for (int i = 0; i < 20; i++) {
        ecs_set(ecs_new(), ChildOf, { parent });
    }
    test_true(ecs_is_alive(child));
    test_true(ecs_is_alive(parent));

    ecs_kill(parent);

    test_false(ecs_is_alive(child));
    test_false(ecs_is_alive(parent));

    ecs_fini();
}

void childof_relation_without_cascade_keeps_related_alive(void) {
    ecs_init();

    ECS_COMPONENT_REGISTER(Targets);

    ecs_entity_t target = ecs_new();
    ecs_entity_t source = ecs_new();

    ecs_set(source, Targets, { target });
    test_true(ecs_has(source, Targets));
    test_true(ecs_has_cid(target, ecs_source(Targets)));

    ecs_kill(target);

    test_true(ecs_is_alive(source));
    test_false(ecs_has(source, Targets));

    ecs_fini();
}

void childof_relation_remove_updates_source(void) {
    ecs_init();

    ECS_COMPONENT_REGISTER(Targets);

    ecs_entity_t target = ecs_new();
    ecs_entity_t source = ecs_new();

    ecs_set(source, Targets, { target });
    test_true(ecs_has_cid(target, ecs_source(Targets)));

    ecs_remove(source, Targets);

    test_false(ecs_has_cid(target, ecs_source(Targets)));
    test_true(ecs_is_alive(source));
    test_true(ecs_is_alive(target));

    ecs_fini();
}
