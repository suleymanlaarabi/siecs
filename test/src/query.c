#include <siecs_test.h>

ECS_COMPONENT_DECLARE(QueryPosition, { int value; });
ECS_COMPONENT_DECLARE(QueryVelocity, { int value; });
ECS_COMPONENT_DECLARE(QueryMass, { int value; });
ECS_COMPONENT_DECLARE(QueryDisabled, { int value; });

ECS_COMPONENT_DEFINE(QueryPosition, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(QueryVelocity);
ECS_COMPONENT_DEFINE(QueryMass);
ECS_COMPONENT_DEFINE(QueryDisabled);

static void query_test_world(void) {
    ecs_init();
    
    ECS_COMPONENT_REGISTER(QueryPosition);
    ECS_COMPONENT_REGISTER(QueryVelocity);
    ECS_COMPONENT_REGISTER(QueryMass);
    ECS_COMPONENT_REGISTER(QueryDisabled);

}

static ecs_entity_t query_test_entity(int p, int v, int m) {
    ecs_entity_t entity = ecs_new();
    ecs_set(entity, QueryPosition, { p });
    ecs_set(entity, QueryVelocity, { v });
    ecs_set(entity, QueryMass, { m });
    return entity;
}

void query_terms_field_order(void) {
    query_test_world();
    ecs_entity_t entity = query_test_entity(10, 20, 30);

    ecs_query_id_t query = ecs_query(
        {
            .terms = {
                ecs_inout(QueryPosition),
                ecs_filter(QueryMass),
                ecs_in(QueryVelocity),
            },
        }
    );

    ecs_iter_t it = ecs_query_iter(query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    QueryVelocity *velocity = ecs_field(&it, 1);

    test_int(1, it.count);
    test_int(10, position[0].value);
    test_int(20, velocity[0].value);
    position[0].value = 11;

    test_false(ecs_iter_next(&it));

    test_int(11, ecs_get(entity, QueryPosition)->value);

    ecs_query_fini(query);
    ecs_fini();
}

void query_out_term_matches_and_returns_field(void) {
    query_test_world();
    ecs_entity_t entity = query_test_entity(1, 2, 3);

    ecs_query_id_t query = ecs_query({ .terms = { ecs_out(QueryPosition) } });
    ecs_iter_t it = ecs_query_iter(query);

    test_true(ecs_iter_next(&it));
    QueryPosition *position = ecs_field(&it, 0);
    position[0].value = 42;

    test_int(42, ecs_get(entity, QueryPosition)->value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_not_excludes_tables(void) {
    query_test_world();
    ecs_entity_t enabled = query_test_entity(1, 2, 3);
    ecs_entity_t disabled = query_test_entity(10, 20, 30);
    ecs_add(disabled, QueryDisabled);

    ecs_query_id_t query = ecs_query(
        {
            .terms = {
                ecs_in(QueryPosition),
                ecs_not(QueryDisabled),
            },
        }
    );

    ecs_iter_t it = ecs_query_iter(query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_int(ecs_get(enabled, QueryPosition)->value, position[0].value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_excludes_disabled_by_default(void) {
    query_test_world();
    ecs_entity_t enabled = query_test_entity(1, 2, 3);
    ecs_entity_t disabled = query_test_entity(10, 20, 30);
    ecs_add(disabled, Disabled);

    ecs_query_id_t query = ecs_query({ .terms = { ecs_in(QueryPosition) } });

    ecs_iter_t it = ecs_query_iter(query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_int(ecs_get(enabled, QueryPosition)->value, position[0].value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_can_include_disabled_explicitly(void) {
    query_test_world();
    ecs_entity_t enabled = query_test_entity(1, 2, 3);
    ecs_entity_t disabled = query_test_entity(10, 20, 30);
    ecs_add(disabled, Disabled);

    ecs_query_id_t query =
        ecs_query({ .terms = { ecs_in(QueryPosition), ecs_filter(Disabled) } });

    ecs_iter_t it = ecs_query_iter(query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_int(ecs_get(disabled, QueryPosition)->value, position[0].value);
    test_int(1, ecs_has(disabled, Disabled));
    test_int(0, ecs_has(enabled, Disabled));
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_excludes_abstract_by_default(void) {
    query_test_world();
    ecs_entity_t entity = query_test_entity(1, 2, 3);
    ecs_entity_t abstract = query_test_entity(10, 20, 30);
    ecs_add(abstract, Abstract);

    ecs_query_id_t query = ecs_query({ .terms = { ecs_in(QueryPosition) } });

    ecs_iter_t it = ecs_query_iter(query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_assert(it.entities[0] == entity);
    test_int(ecs_get(entity, QueryPosition)->value, position[0].value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_can_include_abstract_explicitly(void) {
    query_test_world();
    ecs_entity_t entity = query_test_entity(1, 2, 3);
    ecs_entity_t abstract = query_test_entity(10, 20, 30);
    ecs_add(abstract, Abstract);

    ecs_query_id_t query =
        ecs_query({ .terms = { ecs_in(QueryPosition), ecs_filter(Abstract) } });

    ecs_iter_t it = ecs_query_iter(query);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_assert(it.entities[0] == abstract);
    test_int(ecs_get(abstract, QueryPosition)->value, position[0].value);
    test_int(1, ecs_has(abstract, Abstract));
    test_int(0, ecs_has(entity, Abstract));
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_optional_field_present(void) {
    query_test_world();
    query_test_entity(10, 20, 30);

    ecs_query_id_t query =
        ecs_query({ .terms = { ecs_in_optional(QueryPosition), ecs_in(QueryVelocity) } });
    ecs_iter_t it = ecs_query_iter(query);

    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    QueryVelocity *velocity = ecs_field(&it, 1);
    test_not_null(position);
    test_not_null(velocity);
    test_int(1, it.count);
    test_int(10, position[0].value);
    test_int(20, velocity[0].value);

    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_optional_field_missing_keeps_field_order(void) {
    query_test_world();

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, QueryVelocity, { 20 });

    ecs_query_id_t query = ecs_query(
        {
            .terms = {
                ecs_in_optional(QueryPosition),
                ecs_inout(QueryVelocity),
            },
        }
    );
    ecs_iter_t it = ecs_query_iter(query);

    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    QueryVelocity *velocity = ecs_field(&it, 1);

    test_null(position);
    test_not_null(velocity);
    test_int(1, it.count);
    test_int(20, velocity[0].value);
    velocity[0].value = 21;

    test_false(ecs_iter_next(&it));
    test_int(21, ecs_get(entity, QueryVelocity)->value);

    ecs_query_fini(query);
    ecs_fini();
}

void query_inout_optional_mutates_when_present(void) {
    query_test_world();
    ecs_entity_t entity = query_test_entity(10, 20, 30);

    ecs_query_id_t query =
        ecs_query({ .terms = { ecs_inout_optional(QueryPosition), ecs_in(QueryVelocity) } });
    ecs_iter_t it = ecs_query_iter(query);

    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    QueryVelocity *velocity = ecs_field(&it, 1);

    test_not_null(position);
    test_not_null(velocity);
    position[0].value += velocity[0].value;

    test_false(ecs_iter_next(&it));
    test_int(30, ecs_get(entity, QueryPosition)->value);

    ecs_query_fini(query);
    ecs_fini();
}

void query_inherited_field_is_shared(void) {
    query_test_world();

    ecs_entity_t base = ecs_new();
    ecs_set(base, QueryPosition, { 42 });
    ecs_add(base, Abstract);

    ecs_entity_t entity = ecs_new();
    ecs_is_a(entity, base);

    ecs_query_id_t query = ecs_query({ .terms = { ecs_in(QueryPosition) } });
    ecs_iter_t it = ecs_query_iter(query);

    bool found_shared = false;
    while (ecs_iter_next(&it)) {
        QueryPosition *position = ecs_field(&it, 0);
        if (ecs_field_kind(&it, 0) == EcsFieldShared) {
            test_int(1, it.count);
            test_assert(it.entities[0] == entity);
            test_assert(position == ecs_get(base, QueryPosition));
            test_int(42, position->value);
            found_shared = true;
        }
    }

    test_true(found_shared);

    ecs_query_fini(query);
    ecs_fini();
}

void query_override_field_is_owned(void) {
    query_test_world();

    ecs_entity_t base = ecs_new();
    ecs_set(base, QueryPosition, { 42 });
    ecs_add(base, Abstract);

    ecs_entity_t entity = ecs_new();
    ecs_is_a(entity, base);
    ecs_set(entity, QueryPosition, { 100 });

    ecs_query_id_t query = ecs_query({ .terms = { ecs_inout(QueryPosition) } });
    ecs_iter_t it = ecs_query_iter(query);

    bool found_owned_override = false;
    while (ecs_iter_next(&it)) {
        QueryPosition *position = ecs_field(&it, 0);
        if (it.entities[0] == entity) {
            test_int(1, it.count);
            test_int(EcsFieldOwned, ecs_field_kind(&it, 0));
            test_assert(position == ecs_get(entity, QueryPosition));
            test_int(100, position->value);
            position->value = 101;
            found_owned_override = true;
        }
    }

    test_true(found_owned_override);
    test_int(101, ecs_get(entity, QueryPosition)->value);
    test_int(42, ecs_get(base, QueryPosition)->value);

    ecs_query_fini(query);
    ecs_fini();
}

void query_inout_does_not_match_shared_inherited_field(void) {
    query_test_world();

    ecs_entity_t base = ecs_new();
    ecs_set(base, QueryPosition, { 42 });
    ecs_add(base, Abstract);

    ecs_entity_t entity = ecs_new();
    ecs_is_a(entity, base);

    ecs_query_id_t query = ecs_query({ .terms = { ecs_inout(QueryPosition) } });
    ecs_iter_t it = ecs_query_iter(query);

    bool found_entity = false;
    while (ecs_iter_next(&it)) {
        for (uint32_t row = 0; row < it.count; row++) {
            found_entity |= it.entities[row] == entity;
        }
    }
    test_false(found_entity);

    ecs_query_fini(query);
    ecs_fini();
}

void query_inout_optional_ignores_shared_inherited_field(void) {
    query_test_world();

    ecs_entity_t base = ecs_new();
    ecs_set(base, QueryPosition, { 42 });
    ecs_add(base, Abstract);

    ecs_entity_t entity = ecs_new();
    ecs_is_a(entity, base);

    ecs_query_id_t query = ecs_query({ .terms = { ecs_inout_optional(QueryPosition) } });
    ecs_iter_t it = ecs_query_iter(query);

    bool found_entity = false;
    while (ecs_iter_next(&it)) {
        if (it.entities[0] == entity) {
            test_int(1, it.count);
            test_int(EcsFieldNone, ecs_field_kind(&it, 0));
            test_assert(ecs_field(&it, 0) == NULL);
            found_entity = true;
        }
    }

    test_true(found_entity);

    ecs_query_fini(query);
    ecs_fini();
}

void query_compact_field_kinds_preserve_none_owned_shared(void) {
    query_test_world();

    ecs_entity_t base = ecs_new();
    ecs_set(base, QueryPosition, { 42 });
    ecs_add(base, Abstract);

    ecs_entity_t entity = ecs_new();
    ecs_is_a(entity, base);
    ecs_set(entity, QueryVelocity, { 7 });

    ecs_query_id_t query = ecs_query(
        {
            .terms = {
                ecs_in_optional(QueryPosition),
                ecs_inout(QueryVelocity),
                ecs_inout_optional(QueryMass),
            },
        }
    );
    ecs_iter_t it = ecs_query_iter(query);

    test_true(ecs_iter_next(&it));
    test_int(1, it.count);
    test_assert(it.entities[0] == entity);
    test_int(EcsFieldShared, ecs_field_kind(&it, 0));
    test_int(EcsFieldOwned, ecs_field_kind(&it, 1));
    test_int(EcsFieldNone, ecs_field_kind(&it, 2));
    test_assert(ecs_field(&it, 0) == ecs_get(base, QueryPosition));
    test_assert(ecs_field(&it, 1) == ecs_get(entity, QueryVelocity));
    test_null(ecs_field(&it, 2));
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_is_a_matches_direct_base(void) {
    query_test_world();

    ecs_entity_t character = ecs_new();
    ecs_add(character, Abstract);

    ecs_entity_t player = ecs_new();
    ecs_is_a(player, character);

    ecs_query_id_t query = ecs_query({ .is_a = character });
    ecs_iter_t it = ecs_query_iter(query);

    test_true(ecs_iter_next(&it));
    test_int(1, it.count);
    test_assert(it.entities[0] == player);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_is_a_excludes_other_bases(void) {
    query_test_world();

    ecs_entity_t character = ecs_new();
    ecs_add(character, Abstract);
    ecs_entity_t vehicle = ecs_new();
    ecs_add(vehicle, Abstract);

    ecs_entity_t player = ecs_new();
    ecs_is_a(player, character);
    ecs_entity_t car = ecs_new();
    ecs_is_a(car, vehicle);

    ecs_query_id_t query = ecs_query({ .is_a = character });
    ecs_iter_t it = ecs_query_iter(query);

    test_true(ecs_iter_next(&it));
    test_int(1, it.count);
    test_assert(it.entities[0] == player);
    test_assert(it.entities[0] != car);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_is_a_matches_transitive_base(void) {
    query_test_world();

    ecs_entity_t character = ecs_new();
    ecs_add(character, Abstract);

    ecs_entity_t player = ecs_new();
    ecs_is_a(player, character);
    ecs_add(player, Abstract);

    ecs_entity_t knight = ecs_new();
    ecs_is_a(knight, player);

    ecs_query_id_t query = ecs_query({ .is_a = character });
    ecs_iter_t it = ecs_query_iter(query);

    test_true(ecs_iter_next(&it));
    test_int(1, it.count);
    test_assert(it.entities[0] == knight);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_is_a_with_component_terms(void) {
    query_test_world();

    ecs_entity_t character = ecs_new();
    ecs_set(character, QueryPosition, { 42 });
    ecs_add(character, Abstract);

    ecs_entity_t player = ecs_new();
    ecs_is_a(player, character);
    ecs_set(player, QueryVelocity, { 7 });

    ecs_query_id_t query = ecs_query(
        {
            .is_a = character,
            .terms = {
                ecs_in(QueryPosition),
                ecs_in(QueryVelocity),
            },
        }
    );
    ecs_iter_t it = ecs_query_iter(query);

    test_true(ecs_iter_next(&it));
    QueryPosition *position = ecs_field(&it, 0);
    QueryVelocity *velocity = ecs_field(&it, 1);
    test_int(1, it.count);
    test_assert(it.entities[0] == player);
    test_int(EcsFieldShared, ecs_field_kind(&it, 0));
    test_int(EcsFieldOwned, ecs_field_kind(&it, 1));
    test_assert(position == ecs_get(character, QueryPosition));
    test_assert(velocity == ecs_get(player, QueryVelocity));
    test_int(42, position->value);
    test_int(7, velocity->value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(query);
    ecs_fini();
}

void query_ids_stay_valid_after_temporary_query_fini(void) {
    query_test_world();
    ecs_entity_t entity = query_test_entity(10, 20, 30);

    ecs_query_id_t temporary = ecs_query({ .terms = { ecs_in(QueryVelocity) } });
    ecs_query_id_t persistent = ecs_query({ .terms = { ecs_in(QueryPosition) } });

    ecs_query_fini(temporary);

    ecs_query_id_t reused = ecs_query({ .terms = { ecs_in(QueryMass) } });

    ecs_iter_t it = ecs_query_iter(persistent);
    test_true(ecs_iter_next(&it));

    QueryPosition *position = ecs_field(&it, 0);
    test_int(1, it.count);
    test_int(ecs_get(entity, QueryPosition)->value, position[0].value);
    test_false(ecs_iter_next(&it));

    ecs_query_fini(reused);
    ecs_query_fini(persistent);
    ecs_fini();
}

void query_fields_refresh_after_table_growth(void) {
    query_test_world();

    ecs_entity_t first = ecs_new();
    ecs_set(first, QueryPosition, { 1 });

    ecs_query_id_t query = ecs_query({ .terms = { ecs_inout(QueryPosition) } });

    ecs_entity_t last = 0;
    for (int32_t i = 2; i <= 64; i++) {
        last = ecs_new();
        ecs_set(last, QueryPosition, { i });
    }

    ecs_iter_t it = ecs_query_iter(query);
    test_true(ecs_iter_next(&it));
    QueryPosition *positions = ecs_field(&it, 0);
    test_int(64, it.count);
    for (uint32_t i = 0; i < it.count; i++) {
        positions[i].value += 100;
    }
    test_false(ecs_iter_next(&it));

    test_int(101, ecs_get(first, QueryPosition)->value);
    test_int(164, ecs_get(last, QueryPosition)->value);

    ecs_query_fini(query);
    ecs_fini();
}
