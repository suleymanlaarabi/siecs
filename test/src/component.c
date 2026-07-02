#include "siecs.h"
#include "sijson.h"
#include "storage/table_index.h"
#include "type.h"
#include "world_internal.h"
#include <siecs_test.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

ECS_COMPONENT_DECLARE(Position, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(Position);

ECS_COMPONENT_DECLARE(Velocity, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(Velocity);

ECS_COMPONENT_DECLARE(HookComponent, { int value; });
ECS_COMPONENT_DECLARE(RequiredA, { int value; });
ECS_COMPONENT_DECLARE(RequiredB, { int value; });

static uint32_t hook_add_calls;
static uint32_t hook_set_calls;
static uint32_t hook_remove_calls;
static uint32_t hook_order;
static uint32_t hook_last_add_order;
static uint32_t hook_last_set_order;
static uint32_t hook_last_remove_order;
static bool hook_add_saw_zero;
static bool hook_add_saw_component;
static bool hook_remove_saw_component;
static int hook_last_set_old;
static int hook_last_set_new;
static int hook_last_remove_value;

static void reset_hook_state(void) {
    hook_add_calls = 0;
    hook_set_calls = 0;
    hook_remove_calls = 0;
    hook_order = 0;
    hook_last_add_order = 0;
    hook_last_set_order = 0;
    hook_last_remove_order = 0;
    hook_add_saw_zero = false;
    hook_add_saw_component = false;
    hook_remove_saw_component = false;
    hook_last_set_old = 0;
    hook_last_set_new = 0;
    hook_last_remove_value = 0;
}

static void hook_component_on_add(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    void *ptr
) {
    (void)world;
    (void)entity;

    const HookComponent *value = ptr;
    hook_add_calls++;
    hook_last_add_order = ++hook_order;
    hook_add_saw_zero = value->value == 0;
    hook_add_saw_component = component == ecs_id(HookComponent);
}

static void hook_component_on_set(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *new_value,
    void *current_value
) {
    (void)world;
    (void)entity;
    (void)component;

    const HookComponent *old_value = current_value;
    const HookComponent *next_value = new_value;

    hook_set_calls++;
    hook_last_set_order = ++hook_order;
    hook_last_set_old = old_value->value;
    hook_last_set_new = next_value->value;
}

static void hook_component_on_remove(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    void *ptr
) {
    (void)world;
    (void)entity;

    const HookComponent *value = ptr;
    hook_remove_calls++;
    hook_last_remove_order = ++hook_order;
    hook_last_remove_value = value->value;
    hook_remove_saw_component = component == ecs_id(HookComponent);
}

ECS_COMPONENT_DEFINE(
    HookComponent,
    .on_add = hook_component_on_add,
    .on_set = hook_component_on_set,
    .on_remove = hook_component_on_remove
);

ECS_COMPONENT_DEFINE(RequiredA);
ECS_COMPONENT_DEFINE(RequiredB);

static void register_many_tag_and_data_components(
    ecs_world_t *world,
    ecs_component_t tags[15],
    ecs_component_t data[15],
    char tag_names[15][32],
    char data_names[15][32]
) {
    for (uint32_t i = 0; i < 15; i++) {
        snprintf(tag_names[i], 32, "ManyTag%d", i);
        snprintf(data_names[i], 32, "ManyData%d", i);

        tags[i] = ecs_component(world, { .name = tag_names[i] });
        data[i] = ecs_component(world, { .name = data_names[i], .size = sizeof(int) });
    }
}

static void
set_many_data(ecs_world_t *world, ecs_entity_t entity, ecs_component_t data[15], int base) {
    for (uint32_t i = 0; i < 15; i++) {
        int value = base + (int)i;
        ecs_set_cid(world, entity, data[i], &value);
    }
}

static void add_many_tags(ecs_world_t *world, ecs_entity_t entity, ecs_component_t tags[15]) {
    for (uint32_t i = 0; i < 15; i++) {
        ecs_add_cid(world, entity, tags[i]);
    }
}

static void expect_many_data(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t data[15],
    int base,
    uint32_t skip
) {
    for (uint32_t i = 0; i < 15; i++) {
        if (i == skip) {
            continue;
        }
        test_int(base + (int)i, *(int *)ecs_get_cid(world, entity, data[i]));
    }
}

static void expect_many_tags(ecs_world_t *world, ecs_entity_t entity, ecs_component_t tags[15]) {
    for (uint32_t i = 0; i < 15; i++) {
        test_true(ecs_has_cid(world, entity, tags[i]));
    }
}

void component_reflection(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Position);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, Position, { 10, 20 });

    void *data = ecs_get(world, entity, Position);
    char *result = sijson_to_json_ptr(Position, data);

    test_str("{\"x\":10,\"y\":20}", result);
    free(result);
    ecs_fini(world);
}

void component_on_add(void) {
    reset_hook_state();

    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, HookComponent);

    ecs_entity_t entity = ecs_new(world);
    ecs_add(world, entity, HookComponent);

    test_assert(hook_add_calls == 1);
    test_assert(hook_set_calls == 0);
    test_true(hook_add_saw_zero);
    test_true(hook_add_saw_component);
    test_int(0, ecs_get(world, entity, HookComponent)->value);

    ecs_add(world, entity, HookComponent);
    test_assert(hook_add_calls == 1);

    ecs_set(world, entity, HookComponent, { 5 });
    test_assert(hook_add_calls == 1);
    test_assert(hook_set_calls == 1);
    test_assert(hook_last_set_old == 0);
    test_assert(hook_last_set_new == 5);
    test_int(5, ecs_get(world, entity, HookComponent)->value);

    ecs_entity_t implicit_entity = ecs_new(world);
    ecs_set(world, implicit_entity, HookComponent, { 9 });

    test_assert(hook_add_calls == 2);
    test_assert(hook_set_calls == 2);
    test_assert(hook_remove_calls == 0);
    test_assert(hook_last_add_order < hook_last_set_order);
    test_true(hook_add_saw_zero);
    test_assert(hook_last_set_old == 0);
    test_assert(hook_last_set_new == 9);
    test_int(9, ecs_get(world, implicit_entity, HookComponent)->value);

    ecs_remove(world, implicit_entity, HookComponent);
    test_assert(hook_remove_calls == 1);
    test_true(hook_remove_saw_component);
    test_assert(hook_last_set_order < hook_last_remove_order);
    test_int(9, hook_last_remove_value);
    test_false(ecs_has(world, implicit_entity, HookComponent));

    ecs_fini(world);
}

void component_add_with_required_uses_current_table_edge(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, RequiredA);
    ECS_COMPONENT_REGISTER(world, RequiredB);

    ecs_with(world, ecs_id(RequiredA), ecs_id(RequiredB));

    ecs_entity_t warmup = ecs_new(world);
    ecs_add(world, warmup, RequiredA);

    ecs_entity_t entity = ecs_new(world);
    ecs_add(world, entity, RequiredA);

    test_true(ecs_has(world, entity, RequiredA));
    test_true(ecs_has(world, entity, RequiredB));

    ecs_fini(world);
}

void component_add_with_required_uses_cached_multi_add_edge(void) {
    reset_hook_state();

    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, RequiredA);
    ECS_COMPONENT_REGISTER(world, RequiredB);
    ECS_COMPONENT_REGISTER(world, HookComponent);

    ecs_with(world, ecs_id(RequiredA), ecs_id(RequiredB));
    ecs_with(world, ecs_id(RequiredB), ecs_id(HookComponent));

    ecs_entity_t first = ecs_new(world);
    ecs_add(world, first, RequiredA);

    ecs_entity_t second = ecs_new(world);
    ecs_add(world, second, RequiredA);

    test_true(ecs_has(world, first, RequiredA));
    test_true(ecs_has(world, first, RequiredB));
    test_true(ecs_has(world, first, HookComponent));
    test_true(ecs_has(world, second, RequiredA));
    test_true(ecs_has(world, second, RequiredB));
    test_true(ecs_has(world, second, HookComponent));
    test_int(0, ecs_get(world, first, HookComponent)->value);
    test_int(0, ecs_get(world, second, HookComponent)->value);
    test_assert(hook_add_calls == 2);
    test_true(hook_add_saw_component);

    ecs_fini(world);
}

void component_add_zeroes_reused_component_slot(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, HookComponent);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, HookComponent, { 42 });
    ecs_remove(world, entity, HookComponent);

    reset_hook_state();

    ecs_entity_t reused = ecs_new(world);
    ecs_add(world, reused, HookComponent);

    test_assert(hook_add_calls == 1);
    test_true(hook_add_saw_zero);
    test_int(0, ecs_get(world, reused, HookComponent)->value);

    ecs_fini(world);
}

void component_many_tags_preserve_data_on_migration(void) {
    ecs_world_t *world = ecs_init();
    ecs_component_t tags[15];
    ecs_component_t data[15];
    char tag_names[15][32];
    char data_names[15][32];
    register_many_tag_and_data_components(world, tags, data, tag_names, data_names);

    ecs_entity_t entity = ecs_new(world);
    set_many_data(world, entity, data, 100);

    for (uint32_t i = 0; i < 14; i++) {
        ecs_add_cid(world, entity, tags[i]);
    }
    expect_many_data(world, entity, data, 100, UINT32_MAX);

    ecs_add_cid(world, entity, tags[14]);
    expect_many_data(world, entity, data, 100, UINT32_MAX);
    expect_many_tags(world, entity, tags);

    ecs_remove_cid(world, entity, tags[3]);
    test_false(ecs_has_cid(world, entity, tags[3]));
    expect_many_data(world, entity, data, 100, UINT32_MAX);

    ecs_remove_cid(world, entity, data[7]);
    test_false(ecs_has_cid(world, entity, data[7]));
    expect_many_data(world, entity, data, 100, 7);

    ecs_fini(world);
}

void component_many_tags_swap_remove_preserves_moved_entity_data(void) {
    ecs_world_t *world = ecs_init();
    ecs_component_t tags[15];
    ecs_component_t data[15];
    char tag_names[15][32];
    char data_names[15][32];
    register_many_tag_and_data_components(world, tags, data, tag_names, data_names);

    ecs_entity_t first = ecs_new(world);
    ecs_entity_t moved = ecs_new(world);

    set_many_data(world, first, data, 100);
    add_many_tags(world, first, tags);

    set_many_data(world, moved, data, 200);
    add_many_tags(world, moved, tags);

    ecs_remove_cid(world, first, tags[0]);

    expect_many_data(world, moved, data, 200, UINT32_MAX);
    expect_many_tags(world, moved, tags);

    ecs_query_id_t query = ecs_query(
        world,
        {
            .terms = {
                (ecs_query_term_t){ data[0], EcsIn },
                (ecs_query_term_t){ tags[0], EcsFilter },
            },
        }
    );
    ecs_iter_t it = ecs_query_iter(world, query);
    test_true(ecs_iter_next(&it));
    test_int(1, it.count);
    int *values = ecs_field(&it, 0);
    test_int(200, values[0]);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, query);
    ecs_fini(world);
}

static ecs_type_t component_type_with_position_and_base(ecs_entity_t base) {
    ecs_type_t empty = { 0 };
    ecs_type_t with_position = ecs_type_with_add(&empty, ecs_id(Position));
    ecs_type_t with_base = ecs_type_with_base(&with_position, base);
    ecs_type_fini(&with_position);
    return with_base;
}

void component_same_local_type_with_different_base_creates_different_tables(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Position);

    ecs_entity_t base_a = ecs_new(world);
    ecs_entity_t base_b = ecs_new(world);

    uint16_t table_a =
        ecs_table_index_get_or_create(world, component_type_with_position_and_base(base_a));
    uint16_t table_b =
        ecs_table_index_get_or_create(world, component_type_with_position_and_base(base_b));

    test_assert(table_a != table_b);
    test_assert(base_a == world->table_index.tables[table_a].type.base);
    test_assert(base_b == world->table_index.tables[table_b].type.base);

    ecs_fini(world);
}

void component_type_add_remove_preserves_base(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Position);

    ecs_entity_t base = ecs_new(world);
    ecs_type_t empty = { .base = base };
    ecs_type_t added = ecs_type_with_add(&empty, ecs_id(Position));
    ecs_type_t removed = ecs_type_with_remove_at(&added, 0);

    test_assert(base == added.base);
    test_assert(base == removed.base);

    ecs_type_fini(&added);
    ecs_type_fini(&removed);
    ecs_fini(world);
}

void component_table_resolves_recursive_base_components(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Position);
    ECS_COMPONENT_REGISTER(world, Velocity);

    ecs_entity_t grandparent = ecs_new(world);
    ecs_set(world, grandparent, Position, { 10, 20 });
    ecs_add(world, grandparent, Abstract);

    ecs_entity_t parent = ecs_new(world);
    ecs_set(world, parent, Velocity, { 30, 40 });
    ecs_is_a(world, parent, grandparent);
    ecs_add(world, parent, Abstract);

    ecs_entity_t child = ecs_new(world);
    ecs_is_a(world, child, parent);

    const ecs_entity_record_t *child_record = ecs_get_record(world, child);
    const ecs_table_t *child_table = ecs_get_table(world, child_record->table_id);
    bool position_shared = false;
    bool velocity_shared = false;
    Position *position = ecs_table_field(world, child_table, ecs_id(Position), &position_shared);
    Velocity *velocity = ecs_table_field(world, child_table, ecs_id(Velocity), &velocity_shared);

    test_true(ecs_has(world, child, Position));
    test_true(ecs_has(world, child, Velocity));
    test_true(position_shared);
    test_true(velocity_shared);
    test_assert(position == ecs_get(world, grandparent, Position));
    test_assert(velocity == ecs_get(world, parent, Velocity));
    test_int(10, position->x);
    test_int(40, velocity->y);

    ecs_fini(world);
}
