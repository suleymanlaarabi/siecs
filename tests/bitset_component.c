#include "ecs/world.h"
#include <criterion/criterion.h>
#include <stdbool.h>

ECS_BIT_DEFINE(IsActive);
ECS_BIT_DEFINE(IsVisible);
ECS_COMPONENT_DECLARE(BitIterPos, { float x, y; })
ECS_COMPONENT_DECLARE(BitIterVel, { float x, y; })
ECS_COMPONENT_DEFINE(BitIterPos);
ECS_COMPONENT_DEFINE(BitIterVel);

Test(bitset, basic_set_get) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, IsActive);
    ECS_COMPONENT_REGISTER(world, IsVisible);

    ecs_entity_t e1 = ecs_new(world);
    ecs_add(world, e1, IsActive);
    ecs_add(world, e1, IsVisible);

    ecs_set_bit(world, e1, ecs_id(IsActive), true);
    ecs_set_bit(world, e1, ecs_id(IsVisible), false);

    cr_assert(ecs_get_bit(world, e1, ecs_id(IsActive)) == true);
    cr_assert(ecs_get_bit(world, e1, ecs_id(IsVisible)) == false);

    ecs_set_bit(world, e1, ecs_id(IsActive), false);
    cr_assert(ecs_get_bit(world, e1, ecs_id(IsActive)) == false);

    ecs_fini(world);
}

Test(bitset, multiple_entities) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, IsActive);

    const int count = 100;
    ecs_entity_t entities[count];

    for (int i = 0; i < count; i++) {
        entities[i] = ecs_new(world);
        ecs_add(world, entities[i], IsActive);
        ecs_set_bit(world, entities[i], ecs_id(IsActive), (i % 2 == 0));
    }

    for (int i = 0; i < count; i++) {
        bool val = ecs_get_bit(world, entities[i], ecs_id(IsActive));
        cr_assert_eq(
            val,
            (i % 2 == 0),
            "Entity %d failed: expected %d, got %d",
            i,
            (i % 2 == 0),
            val
        );
    }

    ecs_fini(world);
}

Test(bitset, removal_preserves_bits) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, IsActive);

    ecs_entity_t e1 = ecs_new(world);
    ecs_entity_t e2 = ecs_new(world);
    ecs_entity_t e3 = ecs_new(world);

    ecs_add(world, e1, IsActive);
    ecs_add(world, e2, IsActive);
    ecs_add(world, e3, IsActive);

    ecs_set_bit(world, e1, ecs_id(IsActive), true);
    ecs_set_bit(world, e2, ecs_id(IsActive), false);
    ecs_set_bit(world, e3, ecs_id(IsActive), true);

    // Killing e2 will trigger "swap and pop" in the table.
    // e3 should move to e2's position.
    ecs_kill(world, e2);

    cr_assert(ecs_get_bit(world, e1, ecs_id(IsActive)) == true);
    cr_assert(
        ecs_get_bit(world, e3, ecs_id(IsActive)) == true,
        "e3 should still be true after move"
    );

    ecs_fini(world);
}

Test(bitset, iter_set_bits_updates_only_active_entities) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, IsActive);
    ECS_COMPONENT_REGISTER(world, BitIterPos);
    ECS_COMPONENT_REGISTER(world, BitIterVel);

    const uint32_t count = 12;
    ecs_entity_t entities[count];

    for (uint32_t i = 0; i < count; i++) {
        entities[i] = ecs_new(world);
        ecs_add(world, entities[i], BitIterPos);
        ecs_add(world, entities[i], BitIterVel);
        ecs_add(world, entities[i], IsActive);

        BitIterPos *pos = ecs_get(world, entities[i], BitIterPos);
        BitIterVel *vel = ecs_get(world, entities[i], BitIterVel);
        pos->x = (float)i;
        pos->y = (float)(i * 10);
        vel->x = 1.0f;
        vel->y = 2.0f;

        ecs_set_bit(world, entities[i], ecs_id(IsActive), (i % 3) == 0);
    }

    ecs_query_id_t q = ecs_query(
        world,
        {
            .required = { ecs_id(BitIterPos), ecs_id(BitIterVel), ecs_id(IsActive) },
        }
    );

    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_iter_next(&it)) {
        BitIterPos *pos = ecs_field(&it, ecs_id(BitIterPos));
        BitIterVel *vel = ecs_field(&it, ecs_id(BitIterVel));

        ecs_bitfield_t active = ecs_bitfield(&it, ecs_id(IsActive));

        ECS_BITS_FOREACH_SET(active, i, {
            pos[i].x += vel[i].x;
            pos[i].y += vel[i].y;
        });
    }

    for (uint32_t i = 0; i < count; i++) {
        BitIterPos *pos = ecs_get(world, entities[i], BitIterPos);
        float dx = ((i % 3) == 0) ? 1.0f : 0.0f;
        float dy = ((i % 3) == 0) ? 2.0f : 0.0f;
        cr_assert_float_eq(pos->x, (float)i + dx, 1e-6f);
        cr_assert_float_eq(pos->y, (float)(i * 10) + dy, 1e-6f);
    }

    ecs_fini(world);
}

Test(bitset, iter_set_bits_crosses_word_boundaries) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, IsActive);
    ECS_COMPONENT_REGISTER(world, BitIterPos);

    const uint32_t count = 130;
    ecs_entity_t entities[count];

    for (uint32_t i = 0; i < count; i++) {
        entities[i] = ecs_new(world);
        ecs_add(world, entities[i], BitIterPos);
        ecs_add(world, entities[i], IsActive);

        BitIterPos *pos = ecs_get(world, entities[i], BitIterPos);
        pos->x = 0.0f;
        pos->y = 0.0f;

        bool active = i == 0 || i == 63 || i == 64 || i == 65 || i == 129;
        ecs_set_bit(world, entities[i], ecs_id(IsActive), active);
    }

    ecs_query_id_t q = ecs_query(
        world,
        {
            .required = { ecs_id(BitIterPos), ecs_id(IsActive) },
        }
    );

    uint32_t visited = 0;
    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_iter_next(&it)) {
        BitIterPos *pos = ecs_field(&it, ecs_id(BitIterPos));
        ecs_bitfield_t active = ecs_bitfield(&it, ecs_id(IsActive));

        ECS_BITS_FOREACH_SET(active, i, {
            pos[i].x = 1.0f;
            visited++;
        });
    }

    cr_assert_eq(visited, 5u);

    for (uint32_t i = 0; i < count; i++) {
        BitIterPos *pos = ecs_get(world, entities[i], BitIterPos);
        bool active = i == 0 || i == 63 || i == 64 || i == 65 || i == 129;
        cr_assert_float_eq(pos->x, active ? 1.0f : 0.0f, 1e-6f);
    }

    ecs_fini(world);
}

Test(bitset, iter_ignores_reused_cleared_rows) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, IsActive);
    ECS_COMPONENT_REGISTER(world, BitIterPos);

    ecs_entity_t stale = ecs_new(world);
    ecs_add(world, stale, BitIterPos);
    ecs_add(world, stale, IsActive);
    ecs_set_bit(world, stale, ecs_id(IsActive), true);
    ecs_kill(world, stale);

    ecs_entity_t reused = ecs_new(world);
    ecs_add(world, reused, BitIterPos);
    ecs_add(world, reused, IsActive);

    ecs_query_id_t q = ecs_query(
        world,
        {
            .required = { ecs_id(BitIterPos), ecs_id(IsActive) },
        }
    );

    uint32_t visited = 0;
    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_iter_next(&it)) {
        ecs_bitfield_t active = ecs_bitfield(&it, ecs_id(IsActive));

        ECS_BITS_FOREACH_SET(active, i, {
            (void)i;
            visited++;
        });
    }

    cr_assert_eq(visited, 0u);

    ecs_fini(world);
}
