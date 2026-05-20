#include "ecs/world.h"
#include <criterion/criterion.h>
#include <stdbool.h>

ECS_BIT_DEFINE(BitA);
ECS_COMPONENT_DECLARE(Data, { int value; })
ECS_COMPONENT_DEFINE(Data);

Test(bitset_migration, preserve_bit_on_add) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, BitA);
    ECS_COMPONENT_REGISTER(world, Data);

    ecs_entity_t e = ecs_new(world);
    ecs_add(world, e, BitA);
    ecs_set_bit(world, e, ecs_id(BitA), true);

    cr_assert(ecs_get_bit(world, e, ecs_id(BitA)) == true);

    // This triggers migrate_entity
    ecs_add(world, e, Data);

    // If migrate_entity is bugged, this will fail
    cr_assert(
        ecs_get_bit(world, e, ecs_id(BitA)) == true,
        "BitA should be preserved after adding Data"
    );

    ecs_fini(world);
}

Test(bitset_migration, preserve_bit_on_remove) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, BitA);
    ECS_COMPONENT_REGISTER(world, Data);

    ecs_entity_t e = ecs_new(world);
    ecs_add(world, e, BitA);
    ecs_add(world, e, Data);
    ecs_set_bit(world, e, ecs_id(BitA), true);

    cr_assert(ecs_get_bit(world, e, ecs_id(BitA)) == true);

    // This triggers migrate_entity
    ecs_remove(world, e, Data);

    // If migrate_entity is bugged, this will fail
    cr_assert(
        ecs_get_bit(world, e, ecs_id(BitA)) == true,
        "BitA should be preserved after removing Data"
    );

    ecs_fini(world);
}
