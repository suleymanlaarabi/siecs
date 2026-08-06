#include "siecs.h"
#include "sijson.h"
#include "storage/table_index.h"
#include "type.h"
#include "world_internal.h"
#include <siecs_test.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(ecs_type_t) == 24);
_Static_assert(offsetof(ecs_type_t, pair_count) == 10);
_Static_assert(offsetof(ecs_type_t, hash) == 12);
_Static_assert(sizeof(ecs_table_t) == 96);
#endif

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

void component_dynamic_component_layout_and_info(void) {
    ecs_init();

    ecs_component_t mixed = ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = "DynamicMixedComponent",
        .fields = "{ u8 a; f64 b; u32 c; }",
    });
    ecs_component_t tag = ecs_tag_init("DynamicInfoTag");

    test_assert(mixed != 0);
    test_assert(tag != 0);
    test_int(mixed, ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
                        .name = "DynamicMixedComponent",
                        .fields = "{ u8 a; f64 b; u32 c; }",
                    }));

    const ecs_component_info_t *mixed_info = ecs_component_info(mixed);
    const ecs_component_info_t *tag_info = ecs_component_info(tag);
    test_not_null((void *)mixed_info);
    test_str("DynamicMixedComponent", mixed_info->name);
    test_uint(mixed_info->size, 24);
    test_uint(tag_info->size, 0);
    test_uint(
        mixed_info->type,
        sireflect_type_by_name(sijson_default_registry(), "DynamicMixedComponent")
    );

    const sireflect_fields_t *fields =
        sireflect_type_fields(sijson_default_registry(), mixed_info->type);
    test_uint(fields->field_count, 3);
    test_uint(fields->fields[0].offset, 0);
    test_uint(fields->fields[1].offset, 8);
    test_uint(fields->fields[2].offset, 16);
    test_null(ecs_component_info(0));
    test_int(0, ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
                    .name = "BadDynamicComponent",
                    .fields = "{ Missing value; }",
                }));

    ecs_fini();
}

void component_component_info_is_stable(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(Position);

    const ecs_component_info_t *before = ecs_component_info(ecs_id(Position));
    for (uint32_t i = 0; i < 512; i++) {
        ecs_component({ .size = sizeof(uint32_t) });
    }
    const ecs_component_info_t *after = ecs_component_info(ecs_id(Position));

    test_ptr(before, after);
    test_uint(after->size, sizeof(Position));
    test_str(after->name, "Position");

    ecs_fini();
}

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
static uint32_t add_observer_calls;
static uint32_t lifecycle_ctor_calls;
static uint32_t lifecycle_dtor_calls;
static uint32_t lifecycle_copy_calls;
static uint32_t lifecycle_move_ctor_calls;

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
    add_observer_calls = 0;
}

static void lifecycle_reset(void) {
    lifecycle_ctor_calls = 0;
    lifecycle_dtor_calls = 0;
    lifecycle_copy_calls = 0;
    lifecycle_move_ctor_calls = 0;
}

static void lifecycle_ctor(void *ptr, uint32_t count) {
    int *values = ptr;
    for (uint32_t i = 0; i < count; i++) {
        values[i] = -1;
        lifecycle_ctor_calls++;
    }
}

static void lifecycle_dtor(void *ptr, uint32_t count) {
    (void)ptr;
    lifecycle_dtor_calls += count;
}

static void lifecycle_copy(void *dst, const void *src, uint32_t count) {
    int *out = dst;
    const int *in = src;
    for (uint32_t i = 0; i < count; i++) {
        out[i] = in[i];
        lifecycle_copy_calls++;
    }
}

static void lifecycle_move_ctor(void *dst, void *src, uint32_t count) {
    int *out = dst;
    int *in = src;
    for (uint32_t i = 0; i < count; i++) {
        out[i] = in[i];
        in[i] = -999;
        lifecycle_move_ctor_calls++;
    }
}

static void on_component_add_observer(ecs_observer_event_t *event) {
    (void)event;
    add_observer_calls++;
}

static void hook_component_on_add(
        ecs_entity_t entity,
    ecs_component_t component,
    void *ptr
) {
    (void)entity;

    const HookComponent *value = ptr;
    hook_add_calls++;
    hook_last_add_order = ++hook_order;
    hook_add_saw_zero = value->value == 0;
    hook_add_saw_component = component == ecs_id(HookComponent);
}

static void hook_component_on_set(
        ecs_entity_t entity,
    ecs_component_t component,
    const void *new_value,
    void *current_value
) {
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
        ecs_entity_t entity,
    ecs_component_t component,
    void *ptr
) {
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
        ecs_component_t tags[15],
    ecs_component_t data[15],
    char tag_names[15][32],
    char data_names[15][32]
) {
    for (uint32_t i = 0; i < 15; i++) {
        snprintf(tag_names[i], 32, "ManyTag%d", i);
        snprintf(data_names[i], 32, "ManyData%d", i);

        tags[i] = ecs_component({ .name = tag_names[i] });
        data[i] = ecs_component({ .name = data_names[i], .size = sizeof(int) });
    }
}

static void
set_many_data(ecs_entity_t entity, ecs_component_t data[15], int base) {
    for (uint32_t i = 0; i < 15; i++) {
        int value = base + (int)i;
        ecs_set_cid(entity, data[i], &value);
    }
}

static void add_many_tags(ecs_entity_t entity, ecs_component_t tags[15]) {
    for (uint32_t i = 0; i < 15; i++) {
        ecs_add_cid(entity, tags[i]);
    }
}

static void expect_many_data(
        ecs_entity_t entity,
    ecs_component_t data[15],
    int base,
    uint32_t skip
) {
    for (uint32_t i = 0; i < 15; i++) {
        if (i == skip) {
            continue;
        }
        test_int(base + (int)i, *(int *)ecs_get_cid(entity, data[i]));
    }
}

static void expect_many_tags(ecs_entity_t entity, ecs_component_t tags[15]) {
    for (uint32_t i = 0; i < 15; i++) {
        test_true(ecs_has_cid(entity, tags[i]));
    }
}

void component_reflection(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(Position);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, Position, { 10, 20 });

    void *data = ecs_get(entity, Position);
    char *result = sijson_to_json_ptr(Position, data);

    test_str("{\"x\":10,\"y\":20}", result);
    free(result);
    ecs_fini();
}

void component_name_returns_registered_name(void) {
    ecs_init();

    ECS_COMPONENT_REGISTER(Position);
    test_str("Position", ecs_component_name(ecs_id(Position)));

    ecs_fini();
}

void component_on_add(void) {
    reset_hook_state();

    ecs_init();
    ECS_COMPONENT_REGISTER(HookComponent);

    ecs_entity_t entity = ecs_new();
    ecs_add(entity, HookComponent);

    test_assert(hook_add_calls == 1);
    test_assert(hook_set_calls == 0);
    test_true(hook_add_saw_zero);
    test_true(hook_add_saw_component);
    test_int(0, ecs_get(entity, HookComponent)->value);

    ecs_add(entity, HookComponent);
    test_assert(hook_add_calls == 1);

    ecs_set(entity, HookComponent, { 5 });
    test_assert(hook_add_calls == 1);
    test_assert(hook_set_calls == 1);
    test_assert(hook_last_set_old == 0);
    test_assert(hook_last_set_new == 5);
    test_int(5, ecs_get(entity, HookComponent)->value);

    ecs_entity_t implicit_entity = ecs_new();
    ecs_set(implicit_entity, HookComponent, { 9 });

    test_assert(hook_add_calls == 2);
    test_assert(hook_set_calls == 2);
    test_assert(hook_remove_calls == 0);
    test_assert(hook_last_add_order < hook_last_set_order);
    test_true(hook_add_saw_zero);
    test_assert(hook_last_set_old == 0);
    test_assert(hook_last_set_new == 9);
    test_int(9, ecs_get(implicit_entity, HookComponent)->value);

    ecs_remove(implicit_entity, HookComponent);
    test_assert(hook_remove_calls == 1);
    test_true(hook_remove_saw_component);
    test_assert(hook_last_set_order < hook_last_remove_order);
    test_int(9, hook_last_remove_value);
    test_false(ecs_has(implicit_entity, HookComponent));

    ecs_fini();
}

void component_lifecycle_ops_are_used_for_storage_moves(void) {
    lifecycle_reset();

    ecs_init();
    ecs_type_ops_t ops = {
        .ctor = lifecycle_ctor,
        .dtor = lifecycle_dtor,
        .copy = lifecycle_copy,
        .move_ctor = lifecycle_move_ctor,
    };
    ecs_component_t a =
        ecs_component({ .name = "LifecycleA", .size = sizeof(int), .ops = ops });
    ecs_component_t b =
        ecs_component({ .name = "LifecycleB", .size = sizeof(int), .ops = ops });

    ecs_entity_t entity = ecs_new();
    int value = 10;
    ecs_set_cid(entity, a, &value);
    test_int(1, lifecycle_ctor_calls);
    test_int(1, lifecycle_copy_calls);
    test_int(10, *(int *)ecs_get_cid(entity, a));

    ecs_add_cid(entity, b);
    test_int(2, lifecycle_ctor_calls);
    test_int(1, lifecycle_move_ctor_calls);
    test_int(10, *(int *)ecs_get_cid(entity, a));

    ecs_remove_cid(entity, b);
    test_int(2, lifecycle_move_ctor_calls);
    test_int(1, lifecycle_dtor_calls);

    ecs_remove_cid(entity, a);
    test_int(2, lifecycle_dtor_calls);

    ecs_fini();
}

void component_deferred_set_overwrite_preserves_lifecycle(void) {
    lifecycle_reset();

    ecs_init();
    ecs_type_ops_t ops = {
        .dtor = lifecycle_dtor,
        .copy_ctor = lifecycle_copy,
    };
    ecs_component_t component =
        ecs_component({ .name = "DeferredLifecycle", .size = sizeof(int), .ops = ops });
    ecs_entity_t entity = ecs_new();
    int first = 10;
    int second = 20;

    ecs_defer_begin();
    ecs_set_cid(entity, component, &first);
    ecs_set_cid(entity, component, &second);
    test_int(2, lifecycle_copy_calls);
    test_int(1, lifecycle_dtor_calls);
    ecs_defer_end();

    test_int(20, *(int *)ecs_get_cid(entity, component));
    test_int(1, lifecycle_dtor_calls);
    ecs_fini();
    test_int(2, lifecycle_dtor_calls);
}

void component_add_with_required_uses_current_table_edge(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(RequiredA);
    ECS_COMPONENT_REGISTER(RequiredB);

    ecs_with(ecs_id(RequiredA), ecs_id(RequiredB));

    ecs_entity_t warmup = ecs_new();
    ecs_add(warmup, RequiredA);

    ecs_entity_t entity = ecs_new();
    ecs_add(entity, RequiredA);

    test_true(ecs_has(entity, RequiredA));
    test_true(ecs_has(entity, RequiredB));

    ecs_fini();
}

void component_add_with_required_uses_cached_multi_add_edge(void) {
    reset_hook_state();

    ecs_init();
    ECS_COMPONENT_REGISTER(RequiredA);
    ECS_COMPONENT_REGISTER(RequiredB);
    ECS_COMPONENT_REGISTER(HookComponent);

    ecs_with(ecs_id(RequiredA), ecs_id(RequiredB));
    ecs_with(ecs_id(RequiredB), ecs_id(HookComponent));

    ecs_entity_t first = ecs_new();
    ecs_add(first, RequiredA);

    ecs_entity_t second = ecs_new();
    ecs_add(second, RequiredA);

    test_true(ecs_has(first, RequiredA));
    test_true(ecs_has(first, RequiredB));
    test_true(ecs_has(first, HookComponent));
    test_true(ecs_has(second, RequiredA));
    test_true(ecs_has(second, RequiredB));
    test_true(ecs_has(second, HookComponent));
    test_int(0, ecs_get(first, HookComponent)->value);
    test_int(0, ecs_get(second, HookComponent)->value);
    test_assert(hook_add_calls == 2);
    test_true(hook_add_saw_component);

    ecs_fini();
}

void component_add_with_required_emits_each_on_add_once(void) {
    reset_hook_state();

    ecs_init();
    ECS_COMPONENT_REGISTER(RequiredA);
    ECS_COMPONENT_REGISTER(RequiredB);
    ECS_COMPONENT_REGISTER(HookComponent);

    ecs_with(ecs_id(RequiredA), ecs_id(RequiredB));
    ecs_with(ecs_id(RequiredB), ecs_id(HookComponent));

    ecs_observer(
        {
            .on = EcsOnAdd,
            .query = { .terms = { ecs_in(RequiredA) } },
            .callback = on_component_add_observer,
        }
    );

    ecs_add(ecs_new(), RequiredA);

    test_int(3, add_observer_calls);

    ecs_fini();
}

void component_add_with_required_accepts_sixteen_component_plan(void) {
    ecs_init();
    ecs_component_t components[16];
    char names[16][32];

    for (uint32_t i = 0; i < 16; i++) {
        snprintf(names[i], 32, "RequiredChain%d", i);
        components[i] = ecs_component({ .name = names[i] });
    }

    for (uint32_t i = 1; i < 16; i++) {
        ecs_with(components[i], components[i - 1]);
    }

    ecs_entity_t first = ecs_new();
    ecs_add_cid(first, components[15]);

    ecs_entity_t second = ecs_new();
    ecs_add_cid(second, components[15]);

    for (uint32_t i = 0; i < 16; i++) {
        test_true(ecs_has_cid(first, components[i]));
        test_true(ecs_has_cid(second, components[i]));
    }

    ecs_fini();
}

void component_add_zeroes_reused_component_slot(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(HookComponent);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, HookComponent, { 42 });
    ecs_remove(entity, HookComponent);

    reset_hook_state();

    ecs_entity_t reused = ecs_new();
    ecs_add(reused, HookComponent);

    test_assert(hook_add_calls == 1);
    test_true(hook_add_saw_zero);
    test_int(0, ecs_get(reused, HookComponent)->value);

    ecs_fini();
}

void component_double_add_and_remove_are_noops(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(HookComponent);
    reset_hook_state();

    ecs_entity_t entity = ecs_new();
    ecs_add(entity, HookComponent);
    ecs_set(entity, HookComponent, { 9 });

    ecs_add(entity, HookComponent);
    test_int(1, (int)hook_add_calls);
    test_int(9, ecs_get(entity, HookComponent)->value);

    ecs_remove(entity, HookComponent);
    ecs_remove(entity, HookComponent);
    test_int(1, (int)hook_remove_calls);
    test_false(ecs_has(entity, HookComponent));

    ecs_fini();
}

void component_many_tags_preserve_data_on_migration(void) {
    ecs_init();
    ecs_component_t tags[15];
    ecs_component_t data[15];
    char tag_names[15][32];
    char data_names[15][32];
    register_many_tag_and_data_components(tags, data, tag_names, data_names);

    ecs_entity_t entity = ecs_new();
    set_many_data(entity, data, 100);

    for (uint32_t i = 0; i < 14; i++) {
        ecs_add_cid(entity, tags[i]);
    }
    expect_many_data(entity, data, 100, UINT32_MAX);

    ecs_add_cid(entity, tags[14]);
    expect_many_data(entity, data, 100, UINT32_MAX);
    expect_many_tags(entity, tags);

    ecs_remove_cid(entity, tags[3]);
    test_false(ecs_has_cid(entity, tags[3]));
    expect_many_data(entity, data, 100, UINT32_MAX);

    ecs_remove_cid(entity, data[7]);
    test_false(ecs_has_cid(entity, data[7]));
    expect_many_data(entity, data, 100, 7);

    ecs_fini();
}

void component_many_tags_swap_remove_preserves_moved_entity_data(void) {
    ecs_init();
    ecs_component_t tags[15];
    ecs_component_t data[15];
    char tag_names[15][32];
    char data_names[15][32];
    register_many_tag_and_data_components(tags, data, tag_names, data_names);

    ecs_entity_t first = ecs_new();
    ecs_entity_t moved = ecs_new();

    set_many_data(first, data, 100);
    add_many_tags(first, tags);

    set_many_data(moved, data, 200);
    add_many_tags(moved, tags);

    ecs_remove_cid(first, tags[0]);

    expect_many_data(moved, data, 200, UINT32_MAX);
    expect_many_tags(moved, tags);

    ecs_query_id_t query = ecs_query(
        {
            .terms = {
                (ecs_query_term_t){ data[0], EcsIn },
                (ecs_query_term_t){ tags[0], EcsFilter },
            },
        }
    );
    ecs_iter_t it = ecs_query_iter(query);
    test_true(ecs_iter_next(&it));
    test_int(1, it.count);
    int *values = ecs_field(&it, 0);
    test_int(200, values[0]);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

static ecs_type_t component_type_with_position_and_base(ecs_entity_t base) {
    ecs_type_t empty = { 0 };
    ecs_type_t with_position = ecs_type_with(
        &empty,
        ecs_id(Position),
        (ecs_type_pair_t){ 0 }
    );
    ecs_type_t with_base = ecs_type_with_base(&with_position, base);
    ecs_type_fini(&with_position);
    return with_base;
}

void component_same_local_type_with_different_base_creates_different_tables(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(Position);

    ecs_entity_t base_a = ecs_new();
    ecs_entity_t base_b = ecs_new();

    uint16_t table_a =
        ecs_table_index_get_or_create(component_type_with_position_and_base(base_a));
    uint16_t table_b =
        ecs_table_index_get_or_create(component_type_with_position_and_base(base_b));

    test_assert(table_a != table_b);
    test_assert(base_a == ecs_world.table_index.tables[table_a].type.base);
    test_assert(base_b == ecs_world.table_index.tables[table_b].type.base);

    ecs_fini();
}

void component_type_add_remove_preserves_base(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(Position);

    ecs_entity_t base = ecs_new();
    ecs_type_t empty = { .base = base };
    ecs_type_t added = ecs_type_with(
        &empty,
        ecs_id(Position),
        (ecs_type_pair_t){ 0 }
    );
    ecs_type_t removed = ecs_type_without(&added, 0, 0);

    test_assert(base == added.base);
    test_assert(base == removed.base);

    ecs_type_fini(&added);
    ecs_type_fini(&removed);
    ecs_fini();
}

void component_type_pairs_are_sorted_replaced_and_removed_atomically(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(Position);

    ecs_entity_t base = ecs_new();
    ecs_type_t empty = { .base = base };
    ecs_type_t first = ecs_type_with(
        &empty,
        ecs_id(Position),
        (ecs_type_pair_t){ .key = 9, .value = UINT64_C(0x123456789abcdef0) }
    );
    ecs_type_t second = ecs_type_with(
        &first,
        0,
        (ecs_type_pair_t){ .key = 3, .value = 42 }
    );
    ecs_type_t replaced = ecs_type_with(
        &second,
        0,
        (ecs_type_pair_t){ .key = 9, .value = UINT64_C(0xfedcba9876543210) }
    );

    test_int(1, replaced.component_count);
    test_int(2, replaced.pair_count);
    test_int(3, ecs_type_pairs(&replaced)[0].key);
    test_uint(42, ecs_type_pairs(&replaced)[0].value);
    test_int(9, ecs_type_pairs(&replaced)[1].key);
    test_uint(UINT64_C(0xfedcba9876543210), ecs_type_pairs(&replaced)[1].value);
    test_assert(base == replaced.base);

    ecs_type_t removed = ecs_type_without(&replaced, 0, 9);
    test_int(0, removed.component_count);
    test_int(1, removed.pair_count);
    test_int(3, ecs_type_pairs(&removed)[0].key);
    test_uint(42, ecs_type_pairs(&removed)[0].value);
    test_assert(base == removed.base);

    ecs_type_fini(&first);
    ecs_type_fini(&second);
    ecs_type_fini(&replaced);
    ecs_type_fini(&removed);
    ecs_fini();
}

void component_table_index_indexes_generic_pairs(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(Position);

    ecs_type_t empty = { 0 };
    ecs_type_t first = ecs_type_with(
        &empty,
        0,
        (ecs_type_pair_t){ .key = 7, .value = UINT64_C(0x123456789abcdef0) }
    );
    ecs_type_t same = ecs_type_with_base(&first, 0);
    ecs_type_t second = ecs_type_with(
        &first,
        ecs_id(Position),
        (ecs_type_pair_t){ .key = 9, .value = 42 }
    );
    uint16_t table = ecs_table_index_get_or_create(first);
    uint16_t same_table = ecs_table_index_get_or_create(same);
    uint16_t second_table = ecs_table_index_get_or_create(second);
    ecs_pair_tables_t shared =
        ecs_table_index_pair_tables(7, UINT64_C(0x123456789abcdef0));
    ecs_pair_tables_t unique = ecs_table_index_pair_tables(9, 42);

    test_int(table, same_table);
    test_int(2, shared.count);
    test_int(table, shared.ids[0]);
    test_int(second_table, shared.ids[1]);
    test_int(1, unique.count);
    test_int(second_table, unique.ids[0]);
    test_int(0, ecs_table_index_pair_tables(7, 1).count);
    ecs_fini();
}

void component_tag_components_have_no_storage(void) {
    ecs_init();

    test_int(0, ecs_id(Disabled_desc).size);
    test_not_null(ecs_id(Disabled_desc).struct_desc);
    test_str("{}", ecs_id(Disabled_desc).struct_desc->fields);
    test_int(0, ecs_id(Abstract_desc).size);
    test_not_null(ecs_id(Abstract_desc).struct_desc);
    test_str("{}", ecs_id(Abstract_desc).struct_desc->fields);

    ecs_entity_t entity = ecs_new();
    ecs_add(entity, Disabled);
    ecs_add(entity, Abstract);

    ecs_table_t *table = ecs_get_table(ecs_get_record(entity)->table_id);
    test_int(0, table->add_edge.aux);
    test_null(table->data_columns);

    ecs_fini();
}

void component_try_get_handles_missing_and_inherited(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(Position);

    ecs_entity_t base = ecs_new();
    ecs_set(base, Position, { 1, 2 });
    ecs_entity_t entity = ecs_new();
    ecs_is_a(entity, base);

    ecs_entity_t missing = ecs_new();
    test_null(ecs_try_get_cid(missing, ecs_id(Position)));
    test_ptr(ecs_try_get(entity, Position), ecs_get(base, Position));

    ecs_fini();
}

void component_table_type_tracks_data_columns(void) {
    ecs_init();

    ecs_component_t tag_a = ecs_component({0});
    ecs_component_t tag_b = ecs_component({0});
    ecs_component_t data_a = ecs_component({ .size = sizeof(uint32_t) });
    ecs_component_t data_b = ecs_component({ .size = sizeof(uint64_t) });
    ecs_entity_t entity = ecs_new();

    ecs_add_cid(entity, tag_a);
    ecs_add_cid(entity, tag_b);
    ecs_table_t *table = ecs_get_table(ecs_get_record(entity)->table_id);
    test_int(0, table->add_edge.aux);
    test_null(table->data_columns);

    uint32_t value_a = 42;
    ecs_set_cid(entity, data_a, &value_a);
    table = ecs_get_table(ecs_get_record(entity)->table_id);
    test_int(1, table->add_edge.aux);
    test_assert(table->type.ids[table->data_columns[0]] == data_a);

    uint64_t value_b = 84;
    ecs_set_cid(entity, data_b, &value_b);
    table = ecs_get_table(ecs_get_record(entity)->table_id);
    test_int(2, table->add_edge.aux);
    test_assert(table->type.ids[table->data_columns[0]] == data_a);
    test_assert(table->type.ids[table->data_columns[1]] == data_b);

    ecs_remove_cid(entity, data_a);
    table = ecs_get_table(ecs_get_record(entity)->table_id);
    test_int(1, table->add_edge.aux);
    test_assert(table->type.ids[table->data_columns[0]] == data_b);
    test_int(84, *(uint64_t *)ecs_get_cid(entity, data_b));

    ecs_fini();
}

static ecs_type_t component_type_from_mask(
    const ecs_component_t *components,
    uint16_t component_count,
    uint32_t mask
) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < component_count; i++) {
        count += (uint16_t)((mask >> i) & 1u);
    }

    ecs_type_t type = {
        .ids = count == 0 ? NULL : malloc(sizeof(ecs_component_t) * count),
        .component_count = count,
    };
    uint16_t out = 0;
    for (uint16_t i = 0; i < component_count; i++) {
        if (mask & (1u << i)) {
            type.ids[out++] = components[i];
        }
    }
    return type;
}

void component_table_index_resize_preserves_type_hashes(void) {
    enum {
        ComponentCount = 12,
        TypeCount = 3200,
    };

    ecs_init();

    ecs_component_t components[ComponentCount];
    for (uint16_t i = 0; i < ComponentCount; i++) {
        components[i] = ecs_component({0});
    }

    uint16_t *table_ids = malloc(sizeof(uint16_t) * TypeCount);
    for (uint32_t mask = 0; mask < TypeCount; mask++) {
        table_ids[mask] = ecs_table_index_get_or_create(
            component_type_from_mask(components, ComponentCount, mask)
        );
    }

    test_assert(ecs_world.table_index.slot_shift > 12);
    uint16_t table_count = ecs_world.table_index.table_count;

    for (uint32_t mask = 0; mask < TypeCount; mask++) {
        uint16_t table_id = ecs_table_index_get_or_create(
            component_type_from_mask(components, ComponentCount, mask)
        );
        test_int(table_ids[mask], table_id);
    }
    test_int(table_count, ecs_world.table_index.table_count);

    free(table_ids);
    ecs_fini();
}

void component_table_resolves_recursive_base_components(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(Position);
    ECS_COMPONENT_REGISTER(Velocity);

    ecs_entity_t grandparent = ecs_new();
    ecs_set(grandparent, Position, { 10, 20 });
    ecs_add(grandparent, Abstract);

    ecs_entity_t parent = ecs_new();
    ecs_set(parent, Velocity, { 30, 40 });
    ecs_is_a(parent, grandparent);
    ecs_add(parent, Abstract);

    ecs_entity_t child = ecs_new();
    ecs_is_a(child, parent);

    const ecs_entity_record_t *child_record = ecs_get_record(child);
    const ecs_table_t *child_table = ecs_get_table(child_record->table_id);
    bool position_shared = false;
    bool velocity_shared = false;
    Position *position = ecs_table_field(child_table, ecs_id(Position), &position_shared);
    Velocity *velocity = ecs_table_field(child_table, ecs_id(Velocity), &velocity_shared);

    test_true(ecs_has(child, Position));
    test_true(ecs_has(child, Velocity));
    test_true(position_shared);
    test_true(velocity_shared);
    test_assert(position == ecs_get(grandparent, Position));
    test_assert(velocity == ecs_get(parent, Velocity));
    test_int(10, position->x);
    test_int(40, velocity->y);

    ecs_fini();
}
