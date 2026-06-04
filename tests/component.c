#include "../ecs/id.h"
#include "../ecs/world.h"
#include <criterion/criterion.h>
#include <criterion/internal/test.h>
#include <stdint.h>

typedef struct {
    float x, y;
} Position;

typedef struct {
    float dx, dy;
} Velocity;

typedef struct {
    int level;
} Player;

Test(component, add_remove) {
    ecs_world_t *world = ecs_init();

    ecs_component_t pos_id = ecs_component(world, { .name = "Position", .size = sizeof(Position) });
    ecs_component_t vel_id = ecs_component(world, { .name = "Velocity", .size = sizeof(Velocity) });

    ecs_entity_t e = ecs_new(world);

    // Add Position
    ecs_add_cid(world, e, pos_id);
    Position *p = ecs_get_cid(world, e, pos_id);
    p->x = 10.0f;
    p->y = 20.0f;

    // Add Velocity
    ecs_add_cid(world, e, vel_id);
    Velocity *v = ecs_get_cid(world, e, vel_id);
    v->dx = 1.0f;
    v->dy = 2.0f;

    // Verify Position data persisted after migration
    p = ecs_get_cid(world, e, pos_id);
    cr_assert_float_eq(p->x, 10.0f, 0.0001f);
    cr_assert_float_eq(p->y, 20.0f, 0.0001f);

    // Remove Position
    ecs_remove_cid(world, e, pos_id);

    // Verify Velocity data persisted
    v = ecs_get_cid(world, e, vel_id);
    cr_assert_float_eq(v->dx, 1.0f, 0.0001f);
    cr_assert_float_eq(v->dy, 2.0f, 0.0001f);

    ecs_fini(world);
}

Test(component, set) {
    ecs_world_t *world = ecs_init();

    ecs_component_t pos_id = ecs_component(world, { .name = "Position", .size = sizeof(Position) });

    ecs_entity_t e = ecs_new(world);

    Position new_pos = { 5.0f, 10.0f };
    ecs_set_cid(world, e, pos_id, &new_pos);

    Position *p = ecs_get_cid(world, e, pos_id);
    cr_assert_float_eq(p->x, 5.0f, 0.0001f);
    cr_assert_float_eq(p->y, 10.0f, 0.0001f);

    ecs_fini(world);
}

Test(component, multiple_entities_migration) {
    ecs_world_t *world = ecs_init();

    ecs_component_t pos_id = ecs_component(world, { .name = "Position", .size = sizeof(Position) });

    ecs_entity_t e1 = ecs_new(world);
    ecs_entity_t e2 = ecs_new(world);

    ecs_add_cid(world, e1, pos_id);
    ecs_add_cid(world, e2, pos_id);

    Position *p1 = ecs_get_cid(world, e1, pos_id);
    Position *p2 = ecs_get_cid(world, e2, pos_id);

    p1->x = 1.0f;
    p2->x = 2.0f;

    // Kill e1, should trigger swap-and-pop in table
    ecs_kill(world, e1);

    // Verify e2 is still alive and has correct data
    cr_assert(ecs_is_alive(world, e2));
    Position *p2_new = ecs_get_cid(world, e2, pos_id);
    cr_assert_float_eq(p2_new->x, 2.0f, 0.0001f);

    ecs_fini(world);
}

Test(component, has) {
    ecs_world_t *world = ecs_init();

    ecs_component_t pos_id = ecs_component(world, { .name = "Position", .size = sizeof(Position) });
    ecs_component_t vel_id = ecs_component(world, { .name = "Velocity", .size = sizeof(Velocity) });

    ecs_entity_t e = ecs_new(world);

    cr_assert(!ecs_has_cid(world, e, pos_id));
    cr_assert(!ecs_has_cid(world, e, vel_id));

    ecs_add_cid(world, e, pos_id);
    cr_assert(ecs_has_cid(world, e, pos_id));
    cr_assert(!ecs_has_cid(world, e, vel_id));

    ecs_add_cid(world, e, vel_id);
    cr_assert(ecs_has_cid(world, e, pos_id));
    cr_assert(ecs_has_cid(world, e, vel_id));

    ecs_remove_cid(world, e, pos_id);
    cr_assert(!ecs_has_cid(world, e, pos_id));
    cr_assert(ecs_has_cid(world, e, vel_id));

    // Add multiple components and check
    ecs_entity_t e2 = ecs_new(world);
    ecs_add_cid(world, e2, pos_id);
    ecs_add_cid(world, e2, vel_id);
    cr_assert(ecs_has_cid(world, e2, pos_id));
    cr_assert(ecs_has_cid(world, e2, vel_id));

    ecs_fini(world);
}

Test(component, add_existing_does_nothing) {
    ecs_world_t *world = ecs_init();

    ecs_component_t pos_id = ecs_component(world, { .name = "Position", .size = sizeof(Position) });
    ecs_entity_t e = ecs_new(world);

    ecs_add_cid(world, e, pos_id);
    Position *p1 = ecs_get_cid(world, e, pos_id);
    p1->x = 42.0f;

    // Add again
    ecs_add_cid(world, e, pos_id);

    Position *p2 = ecs_get_cid(world, e, pos_id);
    cr_assert_eq(p1, p2, "Pointer should remain the same (no migration should occur)");
    cr_assert_float_eq(p2->x, 42.0f, 0.0001f, "Data should not be reset or corrupted");

    ecs_fini(world);
}

Test(component, stress_migration) {
    ecs_world_t *world = ecs_init();

    ecs_component_t c1 = ecs_component(world, { .name = "C1", .size = 4 });
    ecs_component_t c2 = ecs_component(world, { .name = "C2", .size = 4 });
    ecs_component_t c3 = ecs_component(world, { .name = "C3", .size = 4 });

    const int count = 100;
    ecs_entity_t entities[count];

    for (int i = 0; i < count; i++) {
        entities[i] = ecs_new(world);
        ecs_add_cid(world, entities[i], c1);
        *(int *)ecs_get_cid(world, entities[i], c1) = i;
    }

    // Randomly add/remove components
    for (int i = 0; i < count; i++) {
        if (i % 2 == 0)
            ecs_add_cid(world, entities[i], c2);
        if (i % 3 == 0)
            ecs_add_cid(world, entities[i], c3);
    }

    // Verify data integrity
    for (int i = 0; i < count; i++) {
        cr_assert_eq(*(int *)ecs_get_cid(world, entities[i], c1), i);
    }

    ecs_fini(world);
}
