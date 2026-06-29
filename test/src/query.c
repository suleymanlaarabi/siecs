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

void query_excludes_abstract_by_default(void) {
    ecs_world_t *world = query_test_world();
    ecs_entity_t entity = query_test_entity(world, 1, 2, 3);
    ecs_entity_t abstract = query_test_entity(world, 10, 20, 30);
    ecs_add(world, abstract, Abstract);

    ecs_query_id_t query = ecs_query(world, { .terms = { ecs_in(QueryPosition) } });

    ecs_iter_t it = ecs_query_iter(world, query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_assert(it.entities[0] == entity);
    test_int(ecs_get(world, entity, QueryPosition)->value, position[0].value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_can_include_abstract_explicitly(void) {
    ecs_world_t *world = query_test_world();
    ecs_entity_t entity = query_test_entity(world, 1, 2, 3);
    ecs_entity_t abstract = query_test_entity(world, 10, 20, 30);
    ecs_add(world, abstract, Abstract);

    ecs_query_id_t query =
        ecs_query(world, { .terms = { ecs_in(QueryPosition), ecs_filter(Abstract) } });

    ecs_iter_t it = ecs_query_iter(world, query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_assert(it.entities[0] == abstract);
    test_int(ecs_get(world, abstract, QueryPosition)->value, position[0].value);
    test_int(1, ecs_has(world, abstract, Abstract));
    test_int(0, ecs_has(world, entity, Abstract));
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

void query_inherited_field_is_shared(void) {
    ecs_world_t *world = query_test_world();

    ecs_entity_t base = ecs_new(world);
    ecs_set(world, base, QueryPosition, { 42 });
    ecs_add(world, base, Abstract);

    ecs_entity_t entity = ecs_new(world);
    ecs_is_a(world, entity, base);

    ecs_query_id_t query = ecs_query(world, { .terms = { ecs_in(QueryPosition) } });
    ecs_iter_t it = ecs_query_iter(world, query);

    bool found_shared = false;
    while (ecs_iter_next(&it)) {
        QueryPosition *position = ecs_field(&it, 0);
        if (it.field_kinds[0] == EcsFieldShared) {
            test_int(1, it.count);
            test_assert(it.entities[0] == entity);
            test_assert(position == ecs_get(world, base, QueryPosition));
            test_int(42, position->value);
            found_shared = true;
        }
    }

    test_true(found_shared);

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_override_field_is_owned(void) {
    ecs_world_t *world = query_test_world();

    ecs_entity_t base = ecs_new(world);
    ecs_set(world, base, QueryPosition, { 42 });
    ecs_add(world, base, Abstract);

    ecs_entity_t entity = ecs_new(world);
    ecs_is_a(world, entity, base);
    ecs_set(world, entity, QueryPosition, { 100 });

    ecs_query_id_t query = ecs_query(world, { .terms = { ecs_inout(QueryPosition) } });
    ecs_iter_t it = ecs_query_iter(world, query);

    bool found_owned_override = false;
    while (ecs_iter_next(&it)) {
        QueryPosition *position = ecs_field(&it, 0);
        if (it.entities[0] == entity) {
            test_int(1, it.count);
            test_int(EcsFieldOwned, it.field_kinds[0]);
            test_assert(position == ecs_get(world, entity, QueryPosition));
            test_int(100, position->value);
            position->value = 101;
            found_owned_override = true;
        }
    }

    test_true(found_owned_override);
    test_int(101, ecs_get(world, entity, QueryPosition)->value);
    test_int(42, ecs_get(world, base, QueryPosition)->value);

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_inout_does_not_match_shared_inherited_field(void) {
    ecs_world_t *world = query_test_world();

    ecs_entity_t base = ecs_new(world);
    ecs_set(world, base, QueryPosition, { 42 });
    ecs_add(world, base, Abstract);

    ecs_entity_t entity = ecs_new(world);
    ecs_is_a(world, entity, base);

    ecs_query_id_t query = ecs_query(world, { .terms = { ecs_inout(QueryPosition) } });
    ecs_iter_t it = ecs_query_iter(world, query);

    while (ecs_iter_next(&it)) {
        test_assert(it.entities[0] != entity);
    }

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_inout_optional_ignores_shared_inherited_field(void) {
    ecs_world_t *world = query_test_world();

    ecs_entity_t base = ecs_new(world);
    ecs_set(world, base, QueryPosition, { 42 });
    ecs_add(world, base, Abstract);

    ecs_entity_t entity = ecs_new(world);
    ecs_is_a(world, entity, base);

    ecs_query_id_t query = ecs_query(world, { .terms = { ecs_inout_optional(QueryPosition) } });
    ecs_iter_t it = ecs_query_iter(world, query);

    bool found_entity = false;
    while (ecs_iter_next(&it)) {
        if (it.entities[0] == entity) {
            test_int(1, it.count);
            test_int(EcsFieldNone, it.field_kinds[0]);
            test_assert(ecs_field(&it, 0) == NULL);
            found_entity = true;
        }
    }

    test_true(found_entity);

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_is_a_matches_direct_base(void) {
    ecs_world_t *world = query_test_world();

    ecs_entity_t character = ecs_new(world);
    ecs_add(world, character, Abstract);

    ecs_entity_t player = ecs_new(world);
    ecs_is_a(world, player, character);

    ecs_query_id_t query = ecs_query(world, { .is_a = character });
    ecs_iter_t it = ecs_query_iter(world, query);

    test_true(ecs_iter_next(&it));
    test_int(1, it.count);
    test_assert(it.entities[0] == player);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_is_a_excludes_other_bases(void) {
    ecs_world_t *world = query_test_world();

    ecs_entity_t character = ecs_new(world);
    ecs_add(world, character, Abstract);
    ecs_entity_t vehicle = ecs_new(world);
    ecs_add(world, vehicle, Abstract);

    ecs_entity_t player = ecs_new(world);
    ecs_is_a(world, player, character);
    ecs_entity_t car = ecs_new(world);
    ecs_is_a(world, car, vehicle);

    ecs_query_id_t query = ecs_query(world, { .is_a = character });
    ecs_iter_t it = ecs_query_iter(world, query);

    test_true(ecs_iter_next(&it));
    test_int(1, it.count);
    test_assert(it.entities[0] == player);
    test_assert(it.entities[0] != car);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_is_a_matches_transitive_base(void) {
    ecs_world_t *world = query_test_world();

    ecs_entity_t character = ecs_new(world);
    ecs_add(world, character, Abstract);

    ecs_entity_t player = ecs_new(world);
    ecs_is_a(world, player, character);
    ecs_add(world, player, Abstract);

    ecs_entity_t knight = ecs_new(world);
    ecs_is_a(world, knight, player);

    ecs_query_id_t query = ecs_query(world, { .is_a = character });
    ecs_iter_t it = ecs_query_iter(world, query);

    test_true(ecs_iter_next(&it));
    test_int(1, it.count);
    test_assert(it.entities[0] == knight);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(world, query);
    ecs_fini(world);
}

void query_is_a_with_component_terms(void) {
    ecs_world_t *world = query_test_world();

    ecs_entity_t character = ecs_new(world);
    ecs_set(world, character, QueryPosition, { 42 });
    ecs_add(world, character, Abstract);

    ecs_entity_t player = ecs_new(world);
    ecs_is_a(world, player, character);
    ecs_set(world, player, QueryVelocity, { 7 });

    ecs_query_id_t query = ecs_query(
        world,
        {
            .is_a = character,
            .terms = {
                ecs_in(QueryPosition),
                ecs_in(QueryVelocity),
            },
        }
    );
    ecs_iter_t it = ecs_query_iter(world, query);

    test_true(ecs_iter_next(&it));
    QueryPosition *position = ecs_field(&it, 0);
    QueryVelocity *velocity = ecs_field(&it, 1);
    test_int(1, it.count);
    test_assert(it.entities[0] == player);
    test_int(EcsFieldShared, it.field_kinds[0]);
    test_int(EcsFieldOwned, it.field_kinds[1]);
    test_assert(position == ecs_get(world, character, QueryPosition));
    test_assert(velocity == ecs_get(world, player, QueryVelocity));
    test_int(42, position->value);
    test_int(7, velocity->value);
    test_false(ecs_iter_next(&it));

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
