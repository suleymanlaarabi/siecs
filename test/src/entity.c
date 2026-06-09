#include <siecs_test.h>

ECS_COMPONENT_DECLARE(Transform, { int value; });
ECS_COMPONENT_DECLARE(Renderable, { int value; });

ECS_COMPONENT_DEFINE(Transform);
ECS_COMPONENT_DEFINE(Renderable);

void entity_create(void) {
    ecs_world_t *world = ecs_init();
    test_not_null(world);

    ecs_entity_t entity = ecs_new(world);
    test_assert(entity != 0);
    test_true(ecs_is_alive(world, entity));

    ecs_fini(world);
}

void entity_with(void) {
    ecs_world_t *world = ecs_init();
    test_not_null(world);

    ECS_COMPONENT_REGISTER(world, Transform);
    ECS_COMPONENT_REGISTER(world, Renderable);

    ecs_with(world, ecs_id(Renderable), ecs_id(Transform));

    ecs_entity_t entity = ecs_new(world);
    ecs_add(world, entity, Renderable);

    test_true(ecs_has(world, entity, Renderable));
    test_true(ecs_has(world, entity, Transform));

    ecs_fini(world);
}
