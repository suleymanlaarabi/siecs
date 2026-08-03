#include "relation.h"
#include "siecs.h"
#include "storage/query_index.h"
#include "type.h"
#include "world_internal.h"
#include <siecs_test.h>

ECS_COMPONENT_DECLARE(RelValue, { int value; });
ECS_COMPONENT_DEFINE(RelValue);

ECS_RELATION_DECLARE(DenseRel);
ECS_RELATION_DEFINE(DenseRel, {
    .storage = EcsRelationDense,
    .on_delete_target = EcsRemoveRelation,
});

ECS_RELATION_DECLARE(DenseDelete);
ECS_RELATION_DEFINE(DenseDelete, {
    .storage = EcsRelationDense,
    .on_delete_target = EcsDeleteSources,
});

ECS_RELATION_DECLARE(DepthRel);
ECS_RELATION_DEFINE(DepthRel, {
    .storage = EcsRelationByDepth,
    .on_delete_target = EcsRemoveRelation,
    .acyclic = true,
});

ECS_RELATION_DECLARE(GroupOf);
ECS_RELATION_DEFINE(GroupOf, {
    .storage = EcsRelationByTarget,
    .on_delete_target = EcsRemoveRelation,
});

ECS_RELATION_DECLARE(OwnedBy);
ECS_RELATION_DEFINE(OwnedBy, {
    .storage = EcsRelationByTarget,
    .on_delete_target = EcsDeleteSources,
});

ECS_RELATION_DECLARE(LocatedIn);
ECS_RELATION_DEFINE(LocatedIn, {
    .storage = EcsRelationByTarget,
    .on_delete_target = EcsRemoveRelation,
    .acyclic = true,
});

static void register_value(void) { ECS_COMPONENT_REGISTER(RelValue); }

void childof_kill_parent(void) {
    ecs_init();
    ecs_entity_t parent = ecs_new();
    ecs_entity_t child = ecs_new();
    ecs_relate(child, ChildOf, parent);
    for (int i = 0; i < 20; i++) {
        ecs_relate(ecs_new(), ChildOf, parent);
    }
    ecs_kill(parent);
    test_false(ecs_is_alive(child));
    test_false(ecs_is_alive(parent));
    ecs_fini();
}

void childof_dense_retarget_without_migration(void) {
    ecs_init();
    ECS_RELATION_REGISTER(DenseRel);
    ecs_entity_t a = ecs_new();
    ecs_entity_t b = ecs_new();
    ecs_entity_t source = ecs_new();

    ecs_relate(source, DenseRel, a);
    uint16_t table = ecs_get_record(source)->table_id;
    test_true(ecs_has_relation(source, DenseRel));
    test_uint(a, ecs_target(source, DenseRel));

    ecs_relate(source, DenseRel, b);
    test_int(table, ecs_get_record(source)->table_id);
    test_uint(b, ecs_target(source, DenseRel));

    ecs_unrelate(source, DenseRel);
    test_false(ecs_has_relation(source, DenseRel));
    test_uint(0, ecs_target(source, DenseRel));
    ecs_fini();
}

void childof_dense_delete_policies(void) {
    ecs_init();
    ECS_RELATION_REGISTER(DenseRel);
    ECS_RELATION_REGISTER(DenseDelete);
    ecs_entity_t keep_target = ecs_new();
    ecs_entity_t keep = ecs_new();
    ecs_entity_t delete_target = ecs_new();
    ecs_entity_t deleted = ecs_new();
    ecs_relate(keep, DenseRel, keep_target);
    ecs_relate(deleted, DenseDelete, delete_target);

    ecs_kill(keep_target);
    ecs_kill(delete_target);
    test_true(ecs_is_alive(keep));
    test_false(ecs_has_relation(keep, DenseRel));
    test_false(ecs_is_alive(deleted));
    ecs_fini();
}

void childof_bydepth_depth_and_cascade(void) {
    ecs_init();
    register_value();
    ecs_entity_t root = ecs_new();
    ecs_entity_t child = ecs_new();
    ecs_entity_t grandchild = ecs_new();
    ecs_set(root, RelValue, { 0 });
    ecs_set(child, RelValue, { 1 });
    ecs_set(grandchild, RelValue, { 2 });
    ecs_relate(child, ChildOf, root);
    ecs_relate(grandchild, ChildOf, child);

    ecs_query_id_t q = ecs_query({
        .terms = { ecs_in(RelValue) },
        .order = ecs_cascade(ChildOf),
    });
    int expected = 0;
    ecs_iter_t it = ecs_query_iter(q);
    while (ecs_iter_next(&it)) {
        RelValue *values = ecs_field(&it, 0);
        for (uint32_t row = 0; row < it.count; row++) {
            test_int(expected++, values[row].value);
        }
    }
    test_int(3, expected);
    ecs_query_fini(q);

    q = ecs_query({ .relations = { ecs_depth(ChildOf, 2) } });
    test_int(1, ecs_query_count(q));
    it = ecs_query_iter(q);
    test_true(ecs_iter_next(&it));
    test_uint(grandchild, it.entities[0]);
    test_uint(child, ecs_targets(&it, ChildOf)[0]);
    ecs_query_fini(q);
    ecs_fini();
}

void childof_bydepth_reparent_updates_subtree(void) {
    ecs_init();
    ecs_entity_t root = ecs_new();
    ecs_entity_t other = ecs_new();
    ecs_entity_t child = ecs_new();
    ecs_entity_t grandchild = ecs_new();
    ecs_relate(child, ChildOf, root);
    ecs_relate(grandchild, ChildOf, child);
    ecs_relate(other, ChildOf, root);

    ecs_relate(child, ChildOf, other);
    ecs_query_id_t depth2 = ecs_query({ .relations = { ecs_depth(ChildOf, 2) } });
    ecs_query_id_t depth3 = ecs_query({ .relations = { ecs_depth(ChildOf, 3) } });
    test_int(1, ecs_query_count(depth2));
    test_int(1, ecs_query_count(depth3));
    ecs_query_fini(depth2);
    ecs_query_fini(depth3);

    ecs_unrelate(child, ChildOf);
    ecs_query_id_t depth1 = ecs_query({ .relations = { ecs_depth(ChildOf, 1) } });
    test_int(2, ecs_query_count(depth1));
    ecs_query_fini(depth1);
    test_false(ecs_has_relation(child, ChildOf));
    test_uint(child, ecs_target(grandchild, ChildOf));
    ecs_fini();
}

void childof_cascade_cache_accepts_new_depth(void) {
    ecs_init();
    register_value();
    ecs_entity_t root = ecs_new();
    ecs_set(root, RelValue, { 0 });
    ecs_query_id_t q = ecs_query({
        .terms = { ecs_in(RelValue) },
        .order = ecs_cascade(ChildOf),
    });

    ecs_entity_t child = ecs_new();
    ecs_entity_t grandchild = ecs_new();
    ecs_set(child, RelValue, { 1 });
    ecs_set(grandchild, RelValue, { 2 });
    ecs_relate(child, ChildOf, root);
    ecs_relate(grandchild, ChildOf, child);

    int expected = 0;
    ecs_iter_t it = ecs_query_iter(q);
    while (ecs_iter_next(&it)) {
        RelValue *values = ecs_field(&it, 0);
        for (uint32_t row = 0; row < it.count; row++) {
            test_int(expected++, values[row].value);
        }
    }
    test_int(3, expected);
    ecs_query_fini(q);
    ecs_fini();
}

void childof_bytarget_exact_query_and_retarget(void) {
    ecs_init();
    ECS_RELATION_REGISTER(GroupOf);
    register_value();
    ecs_entity_t a = ecs_new();
    ecs_entity_t b = ecs_new();
    ecs_entity_t x = ecs_new();
    ecs_entity_t y = ecs_new();
    ecs_set(x, RelValue, { 10 });
    ecs_set(y, RelValue, { 20 });
    ecs_relate(x, GroupOf, a);
    ecs_relate(y, GroupOf, a);

    ecs_query_id_t q = ecs_query({
        .terms = { ecs_in(RelValue) },
        .relations = { ecs_to(GroupOf, a) },
    });
    test_int(2, ecs_query_count(q));
    ecs_query_fini(q);

    ecs_relate(y, GroupOf, b);
    q = ecs_query({ .relations = { ecs_to(GroupOf, a) } });
    test_int(1, ecs_query_count(q));
    ecs_iter_t it = ecs_query_iter(q);
    test_true(ecs_iter_next(&it));
    test_uint(a, ecs_target_at(&it, GroupOf, 0));
    ecs_query_fini(q);

    q = ecs_query({ .relations = { ecs_to(GroupOf, b) } });
    test_int(1, ecs_query_count(q));
    ecs_query_fini(q);
    ecs_fini();
}

void childof_bytarget_exact_query_spans_tables(void) {
    ecs_init();
    ECS_RELATION_REGISTER(GroupOf);
    ecs_entity_t target = ecs_new();
    for (uint16_t i = 0; i < 5; i++) {
        ecs_entity_t source = ecs_new();
        ecs_add_cid(source, ecs_component({}));
        ecs_relate(source, GroupOf, target);
    }
    ecs_query_id_t q = ecs_query({ .relations = { ecs_to(GroupOf, target) } });
    test_int(5, ecs_query_count(q));
    ecs_query_fini(q);
    ecs_fini();
}

void childof_bytarget_delete_policies(void) {
    ecs_init();
    ECS_RELATION_REGISTER(GroupOf);
    ECS_RELATION_REGISTER(OwnedBy);
    ecs_entity_t keep_target = ecs_new();
    ecs_entity_t keep = ecs_new();
    ecs_entity_t delete_target = ecs_new();
    ecs_entity_t deleted = ecs_new();
    ecs_relate(keep, GroupOf, keep_target);
    ecs_relate(deleted, OwnedBy, delete_target);
    ecs_kill(keep_target);
    ecs_kill(delete_target);
    test_true(ecs_is_alive(keep));
    test_false(ecs_has_relation(keep, GroupOf));
    test_false(ecs_is_alive(deleted));
    ecs_fini();
}

void childof_bytarget_keeps_target_generation(void) {
    ecs_init();
    ECS_RELATION_REGISTER(GroupOf);
    ecs_entity_t old_target = ecs_new();
    ecs_entity_t source = ecs_new();
    ecs_relate(source, GroupOf, old_target);
    ecs_kill(old_target);

    ecs_entity_t new_target = ecs_new();
    test_int(ecs_first(old_target), ecs_first(new_target));
    test_true(old_target != new_target);
    ecs_relate(source, GroupOf, new_target);

    ecs_query_id_t old_query = ecs_query({ .relations = { ecs_to(GroupOf, old_target) } });
    ecs_query_id_t new_query = ecs_query({ .relations = { ecs_to(GroupOf, new_target) } });
    test_int(0, ecs_query_count(old_query));
    test_int(1, ecs_query_count(new_query));
    ecs_query_fini(old_query);
    ecs_query_fini(new_query);
    ecs_fini();
}

void childof_multiple_bytarget_relations_share_one_type(void) {
    ecs_init();
    ECS_RELATION_REGISTER(GroupOf);
    ECS_RELATION_REGISTER(LocatedIn);
    ecs_entity_t group = ecs_new();
    ecs_entity_t location = ecs_new();
    ecs_entity_t source = ecs_new();
    ecs_relate(source, GroupOf, group);
    ecs_relate(source, LocatedIn, location);

    const ecs_table_t *table = ecs_get_table(ecs_get_record(source)->table_id);
    test_int(2, table->type.relation_count);
    test_uint(group, ecs_target(source, GroupOf));
    test_uint(location, ecs_target(source, LocatedIn));
    ecs_query_id_t query = ecs_query({
        .relations = { ecs_to(GroupOf, group), ecs_to(LocatedIn, location) },
    });
    test_int(1, ecs_query_count(query));
    ecs_query_fini(query);
    ecs_fini();
}

void childof_relation_query_presence_optional_and_not(void) {
    ecs_init();
    ECS_RELATION_REGISTER(DenseRel);
    ecs_entity_t target = ecs_new();
    ecs_entity_t related = ecs_new();
    ecs_entity_t plain = ecs_new();
    ecs_relate(related, DenseRel, target);

    ecs_query_id_t q = ecs_query({ .relations = { ecs_rel(DenseRel) } });
    test_int(1, ecs_query_count(q));
    ecs_query_fini(q);
    q = ecs_query({ .relations = { ecs_not_rel(DenseRel) } });
    test_true(ecs_query_count(q) >= 2);
    ecs_query_fini(q);
    q = ecs_query({ .relations = { ecs_rel_opt(DenseRel) } });
    test_true(ecs_query_count(q) >= 3);
    ecs_query_fini(q);
    test_false(ecs_has_relation(plain, DenseRel));
    ecs_fini();
}

void childof_up_finds_nearest_ancestor(void) {
    ecs_init();
    ECS_RELATION_REGISTER(LocatedIn);
    register_value();
    ecs_entity_t root = ecs_new();
    ecs_entity_t middle = ecs_new();
    ecs_entity_t leaf = ecs_new();
    ecs_set(root, RelValue, { 10 });
    ecs_set(middle, RelValue, { 20 });
    ecs_relate(middle, LocatedIn, root);
    ecs_relate(leaf, LocatedIn, middle);

    ecs_query_id_t q = ecs_query({
        .terms = { ecs_up(RelValue, LocatedIn) },
        .relations = { ecs_to(LocatedIn, middle) },
    });
    ecs_iter_t it = ecs_query_iter(q);
    test_true(ecs_iter_next(&it));
    RelValue *value = ecs_field(&it, 0);
    test_int(20, value->value);
    test_true(ecs_field_is_shared(&it, 0));
    ecs_query_fini(q);
    ecs_fini();
}

void childof_up_optional_returns_null(void) {
    ecs_init();
    ECS_RELATION_REGISTER(LocatedIn);
    register_value();
    ecs_entity_t target = ecs_new();
    ecs_entity_t source = ecs_new();
    ecs_relate(source, LocatedIn, target);
    ecs_query_id_t q = ecs_query({
        .terms = { ecs_up_optional(RelValue, LocatedIn) },
        .relations = { ecs_rel(LocatedIn) },
    });
    ecs_iter_t it = ecs_query_iter(q);
    test_true(ecs_iter_next(&it));
    test_null(ecs_field(&it, 0));
    ecs_query_fini(q);
    ecs_fini();
}

void childof_deferred_relation_keeps_last_target(void) {
    ecs_init();
    ECS_RELATION_REGISTER(GroupOf);
    ecs_entity_t a = ecs_new();
    ecs_entity_t b = ecs_new();
    ecs_entity_t source = ecs_new();
    ecs_defer_begin();
    ecs_relate(source, GroupOf, a);
    ecs_relate(source, GroupOf, b);
    ecs_defer_end();
    test_uint(b, ecs_target(source, GroupOf));

    ecs_defer_begin();
    ecs_unrelate(source, GroupOf);
    ecs_defer_end();
    test_false(ecs_has_relation(source, GroupOf));
    ecs_fini();
}

void childof_type_layout_stays_compact(void) {
    test_int(24, sizeof(ecs_type_t));
    test_int(8, sizeof(ecs_relation_t));
    test_int(32, sizeof(ecs_query_t));
    test_int(80, sizeof(ecs_query_cache_t));
    test_int(16, sizeof(ecs_relation_record_t));
}

void childof_query_slot_reuses_component_and_relation_terms(void) {
    ecs_init();
    register_value();
    ECS_RELATION_REGISTER(GroupOf);
    ecs_entity_t group = ecs_new();
    ecs_entity_t member = ecs_new();
    ecs_set(member, RelValue, { 1 });
    ecs_relate(member, GroupOf, group);

    ecs_query_id_t components = ecs_query({ .terms = { ecs_in(RelValue) } });
    test_uint(1, ecs_query_count(components));
    ecs_query_fini(components);

    ecs_query_id_t relation = ecs_query({
        .terms = { ecs_in(RelValue) },
        .relations = { ecs_to(GroupOf, group) },
    });
    test_uint(components, relation);
    test_uint(1, ecs_query_count(relation));
    ecs_query_fini(relation);

    ecs_query_id_t components_again = ecs_query({ .terms = { ecs_filter(RelValue) } });
    test_uint(components, components_again);
    test_uint(1, ecs_query_count(components_again));
    ecs_query_fini(components_again);
    ecs_fini();
}

static uint32_t relation_system_rows;

static void relation_system_callback(ecs_iter_t *it) { relation_system_rows += it->count; }

static void relation_observer_callback(ecs_observer_event_t *event) {
    uint32_t *calls = (uint32_t *)event->user_data;
    (*calls)++;
}

void childof_relation_only_system_and_observer(void) {
    ecs_init();
    ECS_RELATION_REGISTER(GroupOf);
    ecs_entity_t group = ecs_new();
    ecs_entity_t member = ecs_new();
    ecs_entity_t plain = ecs_new();
    ecs_relate(member, GroupOf, group);

    relation_system_rows = 0;
    ecs_system_id_t system = ecs_system({
        .query.relations = { ecs_to(GroupOf, group) },
        .callback = relation_system_callback,
    });
    ecs_run_system(system);
    test_int(1, relation_system_rows);

    uint32_t observer_calls = 0;
    ecs_event_t event = ecs_event();
    ecs_observer({
        .on = event,
        .query.relations = { ecs_to(GroupOf, group) },
        .callback = relation_observer_callback,
        .user_data = (uintptr_t)&observer_calls,
    });
    ecs_observer_trigger(member, event, NULL);
    ecs_observer_trigger(plain, event, NULL);
    test_int(1, observer_calls);
    ecs_fini();
}
