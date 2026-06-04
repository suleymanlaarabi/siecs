#include "ecs/world.h"
#include <criterion/criterion.h>
#include <stdint.h>

ECS_COMPONENT_DECLARE(Player, { int hp; })
ECS_COMPONENT_DECLARE(Enemy, { int hp; })

ECS_COMPONENT_DEFINE(Player);
ECS_COMPONENT_DEFINE(Enemy);

typedef struct {
    int amount;
} Damage;

static int g_add_count;
static int g_remove_count;
static int g_custom_count;
static ecs_entity_t g_last_entity;
static bool g_had_component_on_remove;

static void reset_state(void) {
    g_add_count = 0;
    g_remove_count = 0;
    g_custom_count = 0;
    g_last_entity = 0;
    g_had_component_on_remove = false;
}

static void on_add(ecs_world_t *world, ecs_entity_t entity) {
    (void)world;
    g_add_count++;
    g_last_entity = entity;
}

static void on_remove(ecs_world_t *world, ecs_entity_t entity) {
    g_remove_count++;
    // OnRemove fires before migration, so the component is still present.
    g_had_component_on_remove = ecs_has(world, entity, Player);
}

static void on_custom(ecs_world_t *world, ecs_entity_t entity) {
    (void)world;
    g_custom_count++;
    g_last_entity = entity;
}

static ecs_world_t *setup(void) {
    reset_state();
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Player);
    ECS_COMPONENT_REGISTER(world, Enemy);
    return world;
}

Test(observer, on_add_fires_once) {
    ecs_world_t *world = setup();

    ecs_observer(
        world,
        {
            .on = OnAdd,
            .query = { .required = { ecs_id(Player) } },
            .callback = on_add,
        }
    );

    ecs_entity_t e = ecs_new(world);
    ecs_add(world, e, Player);

    cr_assert_eq(g_add_count, 1, "OnAdd should fire exactly once");
    cr_assert_eq(g_last_entity, e, "OnAdd should report the added entity");

    ecs_fini(world);
}

Test(observer, on_add_ignores_non_matching_table) {
    ecs_world_t *world = setup();

    ecs_observer(
        world,
        {
            .on = OnAdd,
            .query = { .required = { ecs_id(Player) } },
            .callback = on_add,
        }
    );

    ecs_entity_t e = ecs_new(world);
    ecs_add(world, e, Enemy);

    cr_assert_eq(g_add_count, 0, "OnAdd must not fire for a non-matching table");

    ecs_fini(world);
}

Test(observer, duplicate_add_does_not_fire) {
    ecs_world_t *world = setup();

    ecs_observer(
        world,
        {
            .on = OnAdd,
            .query = { .required = { ecs_id(Player) } },
            .callback = on_add,
        }
    );

    ecs_entity_t e = ecs_new(world);
    ecs_add(world, e, Player);
    ecs_add(world, e, Player); // duplicate: no migration, no event

    cr_assert_eq(g_add_count, 1, "Duplicate add must not re-fire OnAdd");

    ecs_fini(world);
}

Test(observer, on_remove_fires_before_removal) {
    ecs_world_t *world = setup();

    ecs_observer(
        world,
        {
            .on = OnRemove,
            .query = { .required = { ecs_id(Player) } },
            .callback = on_remove,
        }
    );

    ecs_entity_t e = ecs_new(world);
    ecs_add(world, e, Player);
    ecs_remove(world, e, Player);

    cr_assert_eq(g_remove_count, 1, "OnRemove should fire exactly once");
    cr_assert(g_had_component_on_remove, "Component must still be present when OnRemove fires");

    ecs_fini(world);
}

Test(observer, custom_event_only_matching_table) {
    ecs_world_t *world = setup();

    ecs_event_t damage = ecs_event(world);

    ecs_observer(
        world,
        {
            .on = damage,
            .query = { .required = { ecs_id(Player) } },
            .callback = on_custom,
        }
    );

    ecs_entity_t player = ecs_new(world);
    ecs_add(world, player, Player);

    ecs_entity_t enemy = ecs_new(world);
    ecs_add(world, enemy, Enemy);

    ecs_observer_trigger(world, player, damage);
    ecs_observer_trigger(world, enemy, damage);

    cr_assert_eq(g_custom_count, 1, "Custom event must fire only for the matching table");
    cr_assert_eq(g_last_entity, player, "Custom event must report the matching entity");

    ecs_fini(world);
}

Test(observer, custom_event_data_reaches_callback) {
    ecs_world_t *world = setup();

    ecs_event_t damage = ecs_event(world);

    ecs_observer(
        world,
        {
            .on = damage,
            .query = { .required = { ecs_id(Player) } },
            .callback = on_custom,
        }
    );

    ecs_entity_t player = ecs_new(world);
    ecs_add(world, player, Player);

    ecs_observer_trigger(world, player, damage);

    cr_assert_eq(g_custom_count, 1, "Custom event should fire once");
    ecs_fini(world);
}

Test(observer, registered_after_table_matches_existing) {
    ecs_world_t *world = setup();

    // Create the {Player} table before the observer exists.
    ecs_entity_t pre = ecs_new(world);
    ecs_add(world, pre, Player);

    ecs_observer(
        world,
        {
            .on = OnAdd,
            .query = { .required = { ecs_id(Player) } },
            .callback = on_add,
        }
    );

    // A later add into the already-existing table must fire.
    ecs_entity_t e = ecs_new(world);
    ecs_add(world, e, Player);

    cr_assert_eq(
        g_add_count,
        1,
        "Observer registered after table creation must match the existing table"
    );
    cr_assert_eq(g_last_entity, e, "OnAdd should report the newly added entity");

    ecs_fini(world);
}

Test(observer, registered_before_table_cached_on_new_table) {
    ecs_world_t *world = setup();

    ecs_observer(
        world,
        {
            .on = OnAdd,
            .query = { .required = { ecs_id(Player) } },
            .callback = on_add,
        }
    );

    // {Player} table does not exist yet; it is created by this add.
    ecs_entity_t e = ecs_new(world);
    ecs_add(world, e, Player);

    cr_assert_eq(g_add_count, 1, "Observer must be cached onto a table created after registration");

    ecs_fini(world);
}
