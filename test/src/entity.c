#include "siecs.h"
#include "world_internal.h"
#include <siecs_test.h>
#include <stdlib.h>
#include <string.h>

ECS_COMPONENT_DECLARE(Transform, { int value; });
ECS_COMPONENT_DECLARE(Renderable, { int value; });

ECS_COMPONENT_DEFINE(Transform);
ECS_COMPONENT_DEFINE(Renderable);

void entity_create(void) {
    ecs_init();
    
    ecs_entity_t entity = ecs_new();
    test_assert(entity != 0);
    test_true(ecs_is_alive(entity));

    ecs_fini();
}

void entity_reuses_id_with_new_generation_after_kill(void) {
    ecs_init();

    ecs_entity_t dead = ecs_new();
    uint32_t dead_index = ecs_first(dead);
    uint32_t dead_generation = ecs_second(dead);
    ecs_kill(dead);

    test_false(ecs_is_alive(dead));

    ecs_entity_t recycled = ecs_new();
    test_int((int)dead_index, (int)ecs_first(recycled));
    test_int((int)(dead_generation + 1), (int)ecs_second(recycled));
    test_true(ecs_is_alive(recycled));
    test_false(ecs_is_alive(dead));

    ecs_fini();
}

void entity_new_no_reuse_uses_new_index(void) {
    ecs_init();

    ecs_entity_t first = ecs_new();
    ecs_entity_t second = ecs_new();
    uint32_t last_index = ecs_first(second);
    ecs_kill(first);

    ecs_entity_t next = ecs_new_no_reuse();
    test_true(ecs_first(next) > last_index);
    test_int(0, (int)ecs_second(next));
    test_true(ecs_is_alive(next));

    ecs_fini();
}

void entity_new_no_reuse_ignores_multiple_free_indices(void) {
    ecs_init();

    ecs_entity_t first = ecs_new();
    ecs_entity_t second = ecs_new();
    ecs_entity_t third = ecs_new();
    uint32_t last_index = ecs_first(third);
    ecs_kill(first);
    ecs_kill(second);

    ecs_entity_t next = ecs_new_no_reuse();
    test_true(ecs_first(next) > last_index);
    test_true(ecs_is_alive(next));

    ecs_fini();
}

void entity_create_has_default_name(void) {
    ecs_init();

    ecs_entity_t entity = ecs_new();
    test_str("(1, 0)", ecs_entity_name(entity));

    ecs_fini();
}

void entity_explicit_name_overrides_default(void) {
    ecs_init();

    ecs_entity_t entity = ecs_new();
    char *input = strdup("Player");
    ecs_set(entity, Name, { input });
    free(input);

    test_str("Player", ecs_entity_name(entity));

    ecs_fini();
}

void entity_with(void) {
    ecs_init();
    
    ECS_COMPONENT_REGISTER(Transform);
    ECS_COMPONENT_REGISTER(Renderable);

    ecs_with(Renderable, Transform);

    ecs_entity_t entity = ecs_new();
    ecs_add(entity, Renderable);

    test_true(ecs_has(entity, Renderable));
    test_true(ecs_has(entity, Transform));

    ecs_fini();
}

void entity_is_a_moves_entity_to_type_with_base(void) {
    ecs_init();

    ECS_COMPONENT_REGISTER(Transform);
    ECS_COMPONENT_REGISTER(Renderable);

    ecs_entity_t base = ecs_new();
    ecs_add(base, Transform);

    ecs_entity_t entity = ecs_new();
    ecs_add(entity, Renderable);

    ecs_is_a(entity, base);

    ecs_entity_record_t *entity_record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(entity_record->table_id);

    test_assert(table->type.base == base);
    test_true(ecs_has(entity, Renderable));

    ecs_fini();
}

void entity_is_a_marks_base_abstract(void) {
    ecs_init();

    ecs_entity_t base = ecs_new();
    ecs_entity_t entity = ecs_new();
    ecs_is_a(entity, base);

    test_true(ecs_has_cid_owned(base, ecs_id(Abstract)));
    test_true(ecs_is(entity, base));

    ecs_fini();
}

void entity_deferred_is_a_marks_base_abstract(void) {
    ecs_init();

    ecs_entity_t base = ecs_new();
    ecs_entity_t entity = ecs_new();
    ecs_defer_begin();
    ecs_is_a(entity, base);
    ecs_defer_end();

    test_true(ecs_has_cid_owned(base, ecs_id(Abstract)));
    test_true(ecs_is(entity, base));

    ecs_fini();
}

void entity_is_a_keeps_local_component_data(void) {
    ecs_init();

    ECS_COMPONENT_REGISTER(Transform);
    ECS_COMPONENT_REGISTER(Renderable);

    ecs_entity_t base = ecs_new();
    ecs_add(base, Transform);
    ecs_add(base, Abstract);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, Renderable, { 42 });
    ecs_is_a(entity, base);

    test_true(ecs_has(entity, Renderable));
    test_int(42, ecs_get(entity, Renderable)->value);

    ecs_fini();
}

void entity_is_a_same_target_is_noop(void) {
    ecs_init();

    ECS_COMPONENT_REGISTER(Transform);
    ECS_COMPONENT_REGISTER(Renderable);

    ecs_entity_t base = ecs_new();
    ecs_add(base, Transform);
    ecs_add(base, Abstract);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, Renderable, { 7 });
    ecs_add(entity, Abstract);

    ecs_is_a(entity, base);
    ecs_entity_record_t before = *ecs_get_record(entity);

    ecs_is_a(entity, base);
    ecs_entity_record_t after = *ecs_get_record(entity);

    test_int(before.table_id, after.table_id);
    test_int(before.table_row, after.table_row);
    test_int(7, ecs_get(entity, Renderable)->value);

    ecs_fini();
}

void entity_is_a_different_target_creates_different_table(void) {
    ecs_init();

    ECS_COMPONENT_REGISTER(Transform);
    ECS_COMPONENT_REGISTER(Renderable);

    ecs_entity_t base_a = ecs_new();
    ecs_add(base_a, Transform);
    ecs_add(base_a, Abstract);

    ecs_entity_t base_b = ecs_new();
    ecs_add(base_b, Transform);
    ecs_add(base_b, Abstract);

    ecs_entity_t entity_a = ecs_new();
    ecs_add(entity_a, Renderable);
    ecs_is_a(entity_a, base_a);

    ecs_entity_t entity_b = ecs_new();
    ecs_add(entity_b, Renderable);
    ecs_is_a(entity_b, base_b);

    ecs_entity_record_t *record_a = ecs_get_record(entity_a);
    ecs_entity_record_t *record_b = ecs_get_record(entity_b);

    test_assert(record_a->table_id != record_b->table_id);
    test_assert(ecs_get_table(record_a->table_id)->type.base == base_a);
    test_assert(ecs_get_table(record_b->table_id)->type.base == base_b);

    ecs_fini();
}

void entity_is_with_multiple_depth(void) {
    ecs_init();

    ecs_entity_t base = ecs_new();
    ecs_add(base, Abstract);

    ecs_entity_t base2 = ecs_new();
    ecs_is_a(base2, base);
    ecs_add(base2, Abstract);

    ecs_entity_t entity = ecs_new();
    ecs_is_a(entity, base2);

    test_assert(ecs_is(entity, base) == true);

    ecs_fini();
}
