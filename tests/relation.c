#include "ecs/world.h"
#include "ecs/world_internal.h"
#include <criterion/criterion.h>
#include <signal.h>
#include <stdint.h>

ECS_RELATION_DECLARE(ChildOf2);
ECS_RELATION_DEFINE(ChildOf2);
ECS_RELATION_DECLARE(Likes);
ECS_RELATION_DEFINE(Likes);

static ecs_world_t *setup_relation(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, ChildOf2);
    return world;
}

static ecs_component_t child_of_source(void) { return ecs_source(ChildOf2); }
static ecs_component_t likes_source(void) { return ecs_source(Likes); }

static ecs_world_t *setup_two_relations(void) {
    ecs_world_t *world = setup_relation();
    ECS_COMPONENT_REGISTER(world, Likes);
    return world;
}

static RelationSource *source_get_by(ecs_world_t *world, ecs_entity_t parent, ecs_component_t src) {
    return ecs_get_cid(world, parent, src);
}

static RelationSource *source_get(ecs_world_t *world, ecs_entity_t parent) {
    return source_get_by(world, parent, child_of_source());
}

static bool source_contains(RelationSource *source, ecs_entity_t entity) {
    ecs_entity_t *entities = source->entities.data;
    for (uint32_t i = 0; i < source->entities.size; i++) {
        if (entities[i] == entity) {
            return true;
        }
    }
    return false;
}

Test(relation, set_creates_source_on_target) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child = ecs_new(world);

    ecs_set(world, child, ChildOf2, { .target = parent });

    cr_assert(ecs_has(world, child, ChildOf2), "Child must keep relation component");
    cr_assert(
        ecs_has_cid(world, parent, child_of_source()),
        "Parent must receive hidden relation source component"
    );

    RelationSource *source = source_get(world, parent);
    cr_assert_eq(source->entities.size, 1, "Parent source should contain one child");
    cr_assert(source_contains(source, child), "Parent source should contain child entity");

    ecs_fini(world);
}

Test(relation, multiple_children_share_source) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child_a = ecs_new(world);
    ecs_entity_t child_b = ecs_new(world);

    ecs_set(world, child_a, ChildOf2, { .target = parent });
    ecs_set(world, child_b, ChildOf2, { .target = parent });

    RelationSource *source = source_get(world, parent);
    cr_assert_eq(source->entities.size, 2, "Parent source should contain two children");
    cr_assert(source_contains(source, child_a), "Parent source should contain first child");
    cr_assert(source_contains(source, child_b), "Parent source should contain second child");

    ecs_fini(world);
}

Test(relation, setting_same_target_does_not_duplicate_child) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child = ecs_new(world);

    ecs_set(world, child, ChildOf2, { .target = parent });
    ecs_set(world, child, ChildOf2, { .target = parent });

    RelationSource *source = source_get(world, parent);
    cr_assert_eq(source->entities.size, 1, "Same relation target must not duplicate child");
    cr_assert(source_contains(source, child), "Parent source should still contain child");

    ecs_fini(world);
}

Test(relation, changing_target_moves_child_between_sources) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent_a = ecs_new(world);
    ecs_entity_t parent_b = ecs_new(world);
    ecs_entity_t child = ecs_new(world);

    ecs_set(world, child, ChildOf2, { .target = parent_a });
    ecs_set(world, child, ChildOf2, { .target = parent_b });

    cr_assert(
        !ecs_has_cid(world, parent_a, child_of_source()),
        "Old parent source should be removed when it becomes empty"
    );

    RelationSource *source_b = source_get(world, parent_b);
    cr_assert_eq(source_b->entities.size, 1, "New parent source should contain one child");
    cr_assert(source_contains(source_b, child), "New parent source should contain child");

    ecs_fini(world);
}

Test(relation, removing_relation_removes_child_from_source) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child = ecs_new(world);

    ecs_set(world, child, ChildOf2, { .target = parent });
    ecs_remove(world, child, ChildOf2);

    cr_assert(!ecs_has(world, child, ChildOf2), "Child relation component should be removed");
    cr_assert(
        !ecs_has_cid(world, parent, child_of_source()),
        "Parent hidden source should be removed when last child is removed"
    );

    ecs_fini(world);
}

Test(relation, removing_one_child_keeps_source_for_other_children) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child_a = ecs_new(world);
    ecs_entity_t child_b = ecs_new(world);

    ecs_set(world, child_a, ChildOf2, { .target = parent });
    ecs_set(world, child_b, ChildOf2, { .target = parent });
    ecs_remove(world, child_a, ChildOf2);

    cr_assert(
        ecs_has_cid(world, parent, child_of_source()),
        "Parent source should stay while another child remains"
    );

    RelationSource *source = source_get(world, parent);
    cr_assert_eq(source->entities.size, 1, "Parent source should contain one remaining child");
    cr_assert(!source_contains(source, child_a), "Removed child should not remain in source");
    cr_assert(source_contains(source, child_b), "Other child should remain in source");

    ecs_fini(world);
}

Test(relation, killing_child_removes_child_from_source) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child = ecs_new(world);

    ecs_set(world, child, ChildOf2, { .target = parent });
    ecs_kill(world, child);

    cr_assert(
        !ecs_has_cid(world, parent, child_of_source()),
        "Killing child should remove hidden source when it becomes empty"
    );

    ecs_fini(world);
}

Test(relation, killing_parent_removes_relation_from_child) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child = ecs_new(world);

    ecs_set(world, child, ChildOf2, { .target = parent });
    ecs_kill(world, parent);

    cr_assert(!ecs_is_alive(world, parent), "Killed parent should be dead");
    cr_assert(!ecs_has(world, child, ChildOf2), "Killing parent should remove relation from child");

    ecs_fini(world);
}

Test(relation, killing_parent_removes_relation_from_all_children) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent = ecs_new(world);
    ecs_entity_t child_a = ecs_new(world);
    ecs_entity_t child_b = ecs_new(world);

    ecs_set(world, child_a, ChildOf2, { .target = parent });
    ecs_set(world, child_b, ChildOf2, { .target = parent });
    ecs_kill(world, parent);

    cr_assert(!ecs_has(world, child_a, ChildOf2), "Killing parent should remove relation A");
    cr_assert(!ecs_has(world, child_b, ChildOf2), "Killing parent should remove relation B");

    ecs_fini(world);
}

Test(relation, setting_dead_target_aborts, .signal = SIGABRT) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t target = ecs_new(world);
    ecs_entity_t child = ecs_new(world);

    ecs_kill(world, target);
    ecs_set(world, child, ChildOf2, { .target = target });
}

Test(relation, setting_invalid_target_aborts, .signal = SIGABRT) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t child = ecs_new(world);

    ecs_set(world, child, ChildOf2, { .target = 0 });
}

Test(relation, multiple_relation_types_keep_separate_sources) {
    ecs_world_t *world = setup_two_relations();
    ecs_entity_t parent_a = ecs_new(world);
    ecs_entity_t parent_b = ecs_new(world);
    ecs_entity_t child = ecs_new(world);

    ecs_set(world, child, ChildOf2, { .target = parent_a });
    ecs_set(world, child, Likes, { .target = parent_b });

    cr_assert(
        ecs_has_cid(world, parent_a, child_of_source()),
        "ChildOf2 target should have ChildOf2 source"
    );
    cr_assert(
        !ecs_has_cid(world, parent_a, likes_source()),
        "ChildOf2 target should not get Likes source"
    );
    cr_assert(
        ecs_has_cid(world, parent_b, likes_source()),
        "Likes target should have Likes source"
    );
    cr_assert(
        !ecs_has_cid(world, parent_b, child_of_source()),
        "Likes target should not get ChildOf2 source"
    );

    RelationSource *child_of = source_get_by(world, parent_a, child_of_source());
    RelationSource *likes = source_get_by(world, parent_b, likes_source());

    cr_assert_eq(child_of->entities.size, 1, "ChildOf2 source should contain one child");
    cr_assert_eq(likes->entities.size, 1, "Likes source should contain one child");
    cr_assert(source_contains(child_of, child), "ChildOf2 source should contain child");
    cr_assert(source_contains(likes, child), "Likes source should contain child");

    ecs_fini(world);
}

Test(relation, query_children_by_relation_component) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent_a = ecs_new(world);
    ecs_entity_t parent_b = ecs_new(world);
    ecs_entity_t child_a = ecs_new(world);
    ecs_entity_t child_b = ecs_new(world);
    ecs_entity_t child_c = ecs_new(world);
    ecs_entity_t unrelated = ecs_new(world);
    (void)unrelated;

    ecs_set(world, child_a, ChildOf2, { .target = parent_a });
    ecs_set(world, child_b, ChildOf2, { .target = parent_a });
    ecs_set(world, child_c, ChildOf2, { .target = parent_b });

    ecs_query_id_t query = ecs_query(world, { .read = { ecs_id(ChildOf2) } });

    uint32_t count = 0;
    uint32_t parent_a_count = 0;
    uint32_t parent_b_count = 0;
    ecs_iter_t it = ecs_query_iter(world, query);
    while (ecs_iter_next(&it)) {
        ChildOf2 *relations = ecs_field(&it, 0);
        for (uint32_t i = 0; i < it.count; i++) {
            count++;
            if (relations[i].target == parent_a) {
                parent_a_count++;
            }
            if (relations[i].target == parent_b) {
                parent_b_count++;
            }
        }
    }

    cr_assert_eq(count, 3, "ChildOf2 query should iterate only children with relation");
    cr_assert_eq(parent_a_count, 2, "ChildOf2 query should see two children for parent A");
    cr_assert_eq(parent_b_count, 1, "ChildOf2 query should see one child for parent B");

    ecs_fini(world);
}

Test(relation, query_sources_by_ecs_source_component) {
    ecs_world_t *world = setup_relation();
    ecs_entity_t parent_a = ecs_new(world);
    ecs_entity_t parent_b = ecs_new(world);
    ecs_entity_t child_a = ecs_new(world);
    ecs_entity_t child_b = ecs_new(world);
    ecs_entity_t child_c = ecs_new(world);
    ecs_entity_t unrelated = ecs_new(world);
    (void)unrelated;

    ecs_set(world, child_a, ChildOf2, { .target = parent_a });
    ecs_set(world, child_b, ChildOf2, { .target = parent_a });
    ecs_set(world, child_c, ChildOf2, { .target = parent_b });

    ecs_query_id_t query = ecs_query(world, { .read = { ecs_source(ChildOf2) } });

    uint32_t parent_count = 0;
    uint32_t total_children = 0;
    bool saw_parent_a = false;
    bool saw_parent_b = false;
    ecs_iter_t it = ecs_query_iter(world, query);
    while (ecs_iter_next(&it)) {
        RelationSource *sources = ecs_field(&it, 0);
        ecs_table_t *table = ecs_iter_table(&it);
        for (uint32_t i = 0; i < it.count; i++) {
            parent_count++;
            total_children += sources[i].entities.size;
            if (table->entities[i] == parent_a) {
                saw_parent_a = true;
                cr_assert_eq(sources[i].entities.size, 2, "Parent A should have two children");
                cr_assert(source_contains(&sources[i], child_a), "Parent A should contain child A");
                cr_assert(source_contains(&sources[i], child_b), "Parent A should contain child B");
            }
            if (table->entities[i] == parent_b) {
                saw_parent_b = true;
                cr_assert_eq(sources[i].entities.size, 1, "Parent B should have one child");
                cr_assert(source_contains(&sources[i], child_c), "Parent B should contain child C");
            }
        }
    }

    cr_assert_eq(parent_count, 2, "Source query should iterate only parent source entities");
    cr_assert_eq(total_children, 3, "Source query should expose all inverse children");
    cr_assert(saw_parent_a, "Source query should include parent A");
    cr_assert(saw_parent_b, "Source query should include parent B");

    ecs_fini(world);
}

Test(relation, query_children_and_sources_for_separate_relation_types) {
    ecs_world_t *world = setup_two_relations();
    ecs_entity_t child_of_parent = ecs_new(world);
    ecs_entity_t likes_parent = ecs_new(world);
    ecs_entity_t child = ecs_new(world);

    ecs_set(world, child, ChildOf2, { .target = child_of_parent });
    ecs_set(world, child, Likes, { .target = likes_parent });

    ecs_query_id_t children_query =
        ecs_query(world, { .read = { ecs_id(ChildOf2), ecs_id(Likes) } });
    ecs_query_id_t child_of_sources_query = ecs_query(world, { .read = { ecs_source(ChildOf2) } });
    ecs_query_id_t likes_sources_query = ecs_query(world, { .read = { ecs_source(Likes) } });

    uint32_t child_rows = 0;
    ecs_iter_t it = ecs_query_iter(world, children_query);
    while (ecs_iter_next(&it)) {
        ChildOf2 *child_of = ecs_field(&it, 0);
        Likes *likes = ecs_field(&it, 1);
        for (uint32_t i = 0; i < it.count; i++) {
            child_rows++;
            cr_assert_eq(child_of[i].target, child_of_parent, "ChildOf2 field should be correct");
            cr_assert_eq(likes[i].target, likes_parent, "Likes field should be correct");
        }
    }

    uint32_t child_of_source_rows = 0;
    it = ecs_query_iter(world, child_of_sources_query);
    while (ecs_iter_next(&it)) {
        child_of_source_rows += it.count;
    }

    uint32_t likes_source_rows = 0;
    it = ecs_query_iter(world, likes_sources_query);
    while (ecs_iter_next(&it)) {
        likes_source_rows += it.count;
    }

    cr_assert_eq(child_rows, 1, "Combined relation query should find child once");
    cr_assert_eq(child_of_source_rows, 1, "ChildOf2 source query should find one parent");
    cr_assert_eq(likes_source_rows, 1, "Likes source query should find one parent");

    ecs_fini(world);
}
