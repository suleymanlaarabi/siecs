#include <siecs_test.h>

ECS_COMPONENT_DECLARE(MultiWorldPosition, { int value; });
ECS_COMPONENT_DEFINE(MultiWorldPosition);

ECS_RELATION_DECLARE(MultiWorldTarget);
ECS_RELATION_DEFINE(MultiWorldTarget, 0);

ECS_RESOURCE_DECLARE(MultiWorldResourceA, { int value; });
ECS_RESOURCE_DEFINE(MultiWorldResourceA);

ECS_RESOURCE_DECLARE(MultiWorldResourceB, { int value; });
ECS_RESOURCE_DEFINE(MultiWorldResourceB);

static void multi_world_register_all(ecs_world_t *world) {
    ECS_COMPONENT_REGISTER(world, MultiWorldPosition);
    ECS_COMPONENT_REGISTER(world, MultiWorldTarget);
}

void multi_world_same_component_registered_in_two_worlds(void) {
    ecs_world_t *a = ecs_init();
    ecs_world_t *b = ecs_init();

    ECS_COMPONENT_REGISTER(a, MultiWorldPosition);
    ecs_component_t first = ecs_id(MultiWorldPosition);
    ECS_COMPONENT_REGISTER(b, MultiWorldPosition);

    test_int(first, ecs_id(MultiWorldPosition));

    ecs_entity_t entity_a = ecs_new(a);
    ecs_entity_t entity_b = ecs_new(b);

    ecs_set(a, entity_a, MultiWorldPosition, { 1 });
    ecs_set(b, entity_b, MultiWorldPosition, { 2 });

    test_int(1, ecs_get(a, entity_a, MultiWorldPosition)->value);
    test_int(2, ecs_get(b, entity_b, MultiWorldPosition)->value);

    ecs_fini(a);
    ecs_fini(b);
}

void multi_world_worlds_keep_independent_component_storage(void) {
    ecs_world_t *a = ecs_init();
    ecs_world_t *b = ecs_init();
    multi_world_register_all(a);
    multi_world_register_all(b);

    ecs_entity_t entity_a = ecs_new(a);
    ecs_entity_t entity_b = ecs_new(b);

    ecs_set(a, entity_a, MultiWorldPosition, { 10 });
    ecs_set(b, entity_b, MultiWorldPosition, { 20 });

    ecs_get(a, entity_a, MultiWorldPosition)->value = 11;
    ecs_get(b, entity_b, MultiWorldPosition)->value = 21;

    test_int(11, ecs_get(a, entity_a, MultiWorldPosition)->value);
    test_int(21, ecs_get(b, entity_b, MultiWorldPosition)->value);

    ecs_fini(a);
    ecs_fini(b);
}

void multi_world_queries_only_see_their_world_tables(void) {
    ecs_world_t *a = ecs_init();
    ecs_world_t *b = ecs_init();
    multi_world_register_all(a);
    multi_world_register_all(b);

    ecs_entity_t entity_a = ecs_new(a);
    ecs_entity_t entity_b = ecs_new(b);
    ecs_set(a, entity_a, MultiWorldPosition, { 100 });
    ecs_set(b, entity_b, MultiWorldPosition, { 200 });

    ecs_query_id_t query_a = ecs_query(a, { .terms = { ecs_in(MultiWorldPosition) } });
    ecs_query_id_t query_b = ecs_query(b, { .terms = { ecs_in(MultiWorldPosition) } });

    ecs_iter_t it_a = ecs_query_iter(a, query_a);
    test_true(ecs_iter_next(&it_a));
    MultiWorldPosition *pos_a = ecs_field(&it_a, 0);
    test_int(1, it_a.count);
    test_int(100, pos_a[0].value);
    test_false(ecs_iter_next(&it_a));

    ecs_iter_t it_b = ecs_query_iter(b, query_b);
    test_true(ecs_iter_next(&it_b));
    MultiWorldPosition *pos_b = ecs_field(&it_b, 0);
    test_int(1, it_b.count);
    test_int(200, pos_b[0].value);
    test_false(ecs_iter_next(&it_b));

    ecs_query_fini(a, query_a);
    ecs_query_fini(b, query_b);
    ecs_fini(a);
    ecs_fini(b);
}

void multi_world_fini_one_world_keeps_other_world_valid(void) {
    ecs_world_t *a = ecs_init();
    ecs_world_t *b = ecs_init();
    multi_world_register_all(a);
    multi_world_register_all(b);

    ecs_entity_t entity_a = ecs_new(a);
    ecs_entity_t entity_b = ecs_new(b);
    ecs_set(a, entity_a, MultiWorldPosition, { 1 });
    ecs_set(b, entity_b, MultiWorldPosition, { 2 });

    ecs_fini(a);

    ecs_set(b, entity_b, MultiWorldPosition, { 3 });
    test_int(3, ecs_get(b, entity_b, MultiWorldPosition)->value);

    ecs_query_id_t query_b = ecs_query(b, { .terms = { ecs_in(MultiWorldPosition) } });
    ecs_iter_t it = ecs_query_iter(b, query_b);
    test_true(ecs_iter_next(&it));
    test_int(1, it.count);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(b, query_b);
    ecs_fini(b);
}

void multi_world_relations_remain_world_local(void) {
    ecs_world_t *a = ecs_init();
    ecs_world_t *b = ecs_init();
    multi_world_register_all(a);
    multi_world_register_all(b);

    ecs_entity_t target_a = ecs_new(a);
    ecs_entity_t source_a = ecs_new(a);
    ecs_entity_t target_b = ecs_new(b);
    ecs_entity_t source_b = ecs_new(b);

    ecs_set(a, source_a, MultiWorldTarget, { target_a });
    ecs_set(b, source_b, MultiWorldTarget, { target_b });

    test_true(ecs_has_cid(a, target_a, ecs_source(MultiWorldTarget)));
    test_true(ecs_has_cid(b, target_b, ecs_source(MultiWorldTarget)));

    ecs_kill(a, target_a);

    test_true(ecs_is_alive(a, source_a));
    test_false(ecs_has(a, source_a, MultiWorldTarget));
    test_true(ecs_is_alive(b, target_b));
    test_true(ecs_is_alive(b, source_b));
    test_true(ecs_has(b, source_b, MultiWorldTarget));
    test_true(ecs_has_cid(b, target_b, ecs_source(MultiWorldTarget)));

    ecs_fini(a);
    ecs_fini(b);
}

void multi_world_resource_ids_are_not_overwritten_by_other_world(void) {
    ecs_world_t *a = ecs_init();
    ecs_world_t *b = ecs_init();

    ECS_RESOURCE_REGISTER(a, MultiWorldResourceA);
    ecs_resource_t resource_a_in_a = ecs_id(MultiWorldResourceA);
    ECS_RESOURCE_REGISTER(a, MultiWorldResourceB);
    ecs_resource_t resource_b_in_a = ecs_id(MultiWorldResourceB);

    ECS_RESOURCE_REGISTER(b, MultiWorldResourceB);
    ECS_RESOURCE_REGISTER(b, MultiWorldResourceA);

    test_int(resource_a_in_a, ecs_id(MultiWorldResourceA));
    test_int(resource_b_in_a, ecs_id(MultiWorldResourceB));

    ecs_fini(a);
    ecs_fini(b);
}

void multi_world_typed_resource_macros_use_their_world_records(void) {
    ecs_world_t *a = ecs_init();
    ecs_world_t *b = ecs_init();

    ECS_RESOURCE_REGISTER(a, MultiWorldResourceA);
    ecs_resource_t resource_a_in_a = ecs_id(MultiWorldResourceA);
    ECS_RESOURCE_REGISTER(a, MultiWorldResourceB);
    ecs_resource_t resource_b_in_a = ecs_id(MultiWorldResourceB);

    ECS_RESOURCE_REGISTER(b, MultiWorldResourceB);
    ECS_RESOURCE_REGISTER(b, MultiWorldResourceA);

    ecs_set_resource(a, MultiWorldResourceA, { 11 });

    test_true(ecs_has_resource_rid(a, resource_a_in_a));
    test_false(ecs_has_resource_rid(a, resource_b_in_a));
    test_int(11, ((MultiWorldResourceA *)ecs_resource_rid(a, resource_a_in_a))->value);

    ecs_fini(a);
    ecs_fini(b);
}
