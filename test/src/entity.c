#include <siecs_test.h>

void entity_create(void) {
    ecs_world_t *world = ecs_init();
    test_not_null(world);

    ecs_entity_t entity = ecs_new(world);
    test_assert(entity != 0);
    test_true(ecs_is_alive(world, entity));

    ecs_fini(world);
}
