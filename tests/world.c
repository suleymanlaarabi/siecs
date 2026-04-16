#include "../ecs/world.h"
#include "../ecs/id.h"
#include <criterion/criterion.h>
#include <criterion/internal/assert.h>
#include <criterion/internal/test.h>

Test(entity, basique) {
    ecs_world_t *world = ecs_init();
    ecs_entity_t entity = ecs_new(world);

    cr_assert_eq(entity, ecs_entity(1, 0));

    cr_assert_eq(ecs_is_alive(world, entity), true);
    ecs_kill(world, entity);
    cr_assert_eq(ecs_is_alive(world, entity), false);
    entity = ecs_new(world);

    cr_assert_eq(entity, ecs_entity(1, 1));
    entity = ecs_new(world);
    cr_assert_eq(entity, ecs_entity(2, 0));
}
