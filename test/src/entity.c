#include "world_internal.h"
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

void entity_is_a_moves_entity_to_type_with_base(void) {
    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_REGISTER(world, Transform);
    ECS_COMPONENT_REGISTER(world, Renderable);

    ecs_entity_t base = ecs_new(world);
    ecs_add(world, base, Transform);
    ecs_add(world, base, Abstract);

    ecs_entity_t entity = ecs_new(world);
    ecs_add(world, entity, Renderable);

    ecs_is_a(world, entity, base);

    ecs_entity_record_t *entity_record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, entity_record->table_id);

    test_assert(table->type.base == base);
    test_true(ecs_has(world, entity, Renderable));

    ecs_fini(world);
}

void entity_is_a_keeps_local_component_data(void) {
    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_REGISTER(world, Transform);
    ECS_COMPONENT_REGISTER(world, Renderable);

    ecs_entity_t base = ecs_new(world);
    ecs_add(world, base, Transform);
    ecs_add(world, base, Abstract);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, Renderable, { 42 });
    ecs_is_a(world, entity, base);

    test_true(ecs_has(world, entity, Renderable));
    test_int(42, ecs_get(world, entity, Renderable)->value);

    ecs_fini(world);
}

void entity_is_a_same_target_is_noop(void) {
    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_REGISTER(world, Transform);
    ECS_COMPONENT_REGISTER(world, Renderable);

    ecs_entity_t base = ecs_new(world);
    ecs_add(world, base, Transform);
    ecs_add(world, base, Abstract);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, Renderable, { 7 });
    ecs_add(world, entity, Abstract);

    ecs_is_a(world, entity, base);
    ecs_entity_record_t before = *ecs_get_record(world, entity);

    ecs_is_a(world, entity, base);
    ecs_entity_record_t after = *ecs_get_record(world, entity);

    test_int(before.table_id, after.table_id);
    test_int(before.table_row, after.table_row);
    test_int(7, ecs_get(world, entity, Renderable)->value);

    ecs_fini(world);
}

void entity_is_a_different_target_creates_different_table(void) {
    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_REGISTER(world, Transform);
    ECS_COMPONENT_REGISTER(world, Renderable);

    ecs_entity_t base_a = ecs_new(world);
    ecs_add(world, base_a, Transform);
    ecs_add(world, base_a, Abstract);

    ecs_entity_t base_b = ecs_new(world);
    ecs_add(world, base_b, Transform);
    ecs_add(world, base_b, Abstract);

    ecs_entity_t entity_a = ecs_new(world);
    ecs_add(world, entity_a, Renderable);
    ecs_is_a(world, entity_a, base_a);

    ecs_entity_t entity_b = ecs_new(world);
    ecs_add(world, entity_b, Renderable);
    ecs_is_a(world, entity_b, base_b);

    ecs_entity_record_t *record_a = ecs_get_record(world, entity_a);
    ecs_entity_record_t *record_b = ecs_get_record(world, entity_b);

    test_assert(record_a->table_id != record_b->table_id);
    test_assert(ecs_get_table(world, record_a->table_id)->type.base == base_a);
    test_assert(ecs_get_table(world, record_b->table_id)->type.base == base_b);

    ecs_fini(world);
}
