#include <siecs_test.h>

ECS_COMPONENT_DECLARE(QueryPosition, { int value; });
ECS_COMPONENT_DECLARE(QueryVelocity, { int value; });
ECS_COMPONENT_DECLARE(QueryMass, { int value; });
ECS_COMPONENT_DECLARE(QueryDisabled, { int value; });

ECS_COMPONENT_DEFINE(QueryPosition);
ECS_COMPONENT_DEFINE(QueryVelocity);
ECS_COMPONENT_DEFINE(QueryMass);
ECS_COMPONENT_DEFINE(QueryDisabled);

static ecs_world_t *query_test_world(void) {
    ecs_world_t *world = ecs_init();
    test_not_null(world);

    ECS_COMPONENT_REGISTER(world, QueryPosition);
    ECS_COMPONENT_REGISTER(world, QueryVelocity);
    ECS_COMPONENT_REGISTER(world, QueryMass);
    ECS_COMPONENT_REGISTER(world, QueryDisabled);

    return world;
}

static ecs_entity_t query_test_entity(ecs_world_t *world, int p, int v, int m) {
    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, QueryPosition, { p });
    ecs_set(world, entity, QueryVelocity, { v });
    ecs_set(world, entity, QueryMass, { m });
    return entity;
}

void query_terms_field_order(void) {
    ecs_world_t *world = query_test_world();
    ecs_entity_t entity = query_test_entity(world, 10, 20, 30);

    ecs_query_id_t query = ecs_query(
        world,
        {
            .terms = {
                ecs_inout(QueryPosition),
                ecs_filter(QueryMass),
                ecs_in(QueryVelocity),
            },
        }
    );

    ecs_iter_t it = ecs_query_iter(world, query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    QueryVelocity *velocity = ecs_field(&it, 1);

    test_int(1, it.count);
    test_int(10, position[0].value);
    test_int(20, velocity[0].value);
    position[0].value = 11;

    test_false(ecs_iter_next(&it));

    test_int(11, ecs_get(world, entity, QueryPosition)->value);

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_out_term_matches_and_returns_field(void) {
    ecs_world_t *world = query_test_world();
    ecs_entity_t entity = query_test_entity(world, 1, 2, 3);

    ecs_query_id_t query = ecs_query(world, { .terms = { ecs_out(QueryPosition) } });
    ecs_iter_t it = ecs_query_iter(world, query);

    test_true(ecs_iter_next(&it));
    QueryPosition *position = ecs_field(&it, 0);
    position[0].value = 42;

    test_int(42, ecs_get(world, entity, QueryPosition)->value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_not_excludes_tables(void) {
    ecs_world_t *world = query_test_world();
    ecs_entity_t enabled = query_test_entity(world, 1, 2, 3);
    ecs_entity_t disabled = query_test_entity(world, 10, 20, 30);
    ecs_add(world, disabled, QueryDisabled);

    ecs_query_id_t query = ecs_query(
        world,
        {
            .terms = {
                ecs_in(QueryPosition),
                ecs_not(QueryDisabled),
            },
        }
    );

    ecs_iter_t it = ecs_query_iter(world, query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_int(ecs_get(world, enabled, QueryPosition)->value, position[0].value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_excludes_disabled_by_default(void) {
    ecs_world_t *world = query_test_world();
    ecs_entity_t enabled = query_test_entity(world, 1, 2, 3);
    ecs_entity_t disabled = query_test_entity(world, 10, 20, 30);
    ecs_add(world, disabled, Disabled);

    ecs_query_id_t query = ecs_query(world, { .terms = { ecs_in(QueryPosition) } });

    ecs_iter_t it = ecs_query_iter(world, query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_int(ecs_get(world, enabled, QueryPosition)->value, position[0].value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_can_include_disabled_explicitly(void) {
    ecs_world_t *world = query_test_world();
    ecs_entity_t enabled = query_test_entity(world, 1, 2, 3);
    ecs_entity_t disabled = query_test_entity(world, 10, 20, 30);
    ecs_add(world, disabled, Disabled);

    ecs_query_id_t query =
        ecs_query(world, { .terms = { ecs_in(QueryPosition), ecs_filter(Disabled) } });

    ecs_iter_t it = ecs_query_iter(world, query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_int(ecs_get(world, disabled, QueryPosition)->value, position[0].value);
    test_int(1, ecs_has(world, disabled, Disabled));
    test_int(0, ecs_has(world, enabled, Disabled));
    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_optional_field_present(void) {
    ecs_world_t *world = query_test_world();
    query_test_entity(world, 10, 20, 30);

    ecs_query_id_t query =
        ecs_query(world, { .terms = { ecs_in_optional(QueryPosition), ecs_in(QueryVelocity) } });
    ecs_iter_t it = ecs_query_iter(world, query);

    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    QueryVelocity *velocity = ecs_field(&it, 1);
    test_not_null(position);
    test_not_null(velocity);
    test_int(1, it.count);
    test_int(10, position[0].value);
    test_int(20, velocity[0].value);

    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_optional_field_missing_keeps_field_order(void) {
    ecs_world_t *world = query_test_world();

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, QueryVelocity, { 20 });

    ecs_query_id_t query = ecs_query(
        world,
        {
            .terms = {
                ecs_in_optional(QueryPosition),
                ecs_inout(QueryVelocity),
            },
        }
    );
    ecs_iter_t it = ecs_query_iter(world, query);

    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    QueryVelocity *velocity = ecs_field(&it, 1);

    test_null(position);
    test_not_null(velocity);
    test_int(1, it.count);
    test_int(20, velocity[0].value);
    velocity[0].value = 21;

    test_false(ecs_iter_next(&it));
    test_int(21, ecs_get(world, entity, QueryVelocity)->value);

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_inout_optional_mutates_when_present(void) {
    ecs_world_t *world = query_test_world();
    ecs_entity_t entity = query_test_entity(world, 10, 20, 30);

    ecs_query_id_t query =
        ecs_query(world, { .terms = { ecs_inout_optional(QueryPosition), ecs_in(QueryVelocity) } });
    ecs_iter_t it = ecs_query_iter(world, query);

    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    QueryVelocity *velocity = ecs_field(&it, 1);

    test_not_null(position);
    test_not_null(velocity);
    position[0].value += velocity[0].value;

    test_false(ecs_iter_next(&it));
    test_int(30, ecs_get(world, entity, QueryPosition)->value);

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_ids_stay_valid_after_temporary_query_fini(void) {
    ecs_world_t *world = query_test_world();
    ecs_entity_t entity = query_test_entity(world, 10, 20, 30);

    ecs_query_id_t temporary = ecs_query(world, { .terms = { ecs_in(QueryVelocity) } });
    ecs_query_id_t persistent = ecs_query(world, { .terms = { ecs_in(QueryPosition) } });

    ecs_query_fini(world, temporary);

    ecs_query_id_t reused = ecs_query(world, { .terms = { ecs_in(QueryMass) } });

    ecs_iter_t it = ecs_query_iter(world, persistent);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_int(ecs_get(world, entity, QueryPosition)->value, position[0].value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, reused);
    ecs_query_fini(world, persistent);
    ecs_fini(world);
}
