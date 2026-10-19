#include "siecs.h"
#include <siecs_test.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ECS_COMPONENT_DECLARE(NamingValue, { int value; });
static ecs_entity_t naming_removed[4];
static int naming_values[4];
static int naming_remove_count;
static bool naming_mutate_on_remove;

static void naming_value_removed(ecs_entity_t entity, ecs_component_t component, void *data) {
    test_int(ecs_id(NamingValue), component);
    test_true(naming_remove_count < 4);
    naming_removed[naming_remove_count] = entity;
    naming_values[naming_remove_count++] = ((NamingValue *)data)->value;
    if (naming_mutate_on_remove) {
        /* Allocates entity records while the collision is being resolved. */
        for (int i = 0; i < 400; i++)
            ecs_new();
        ecs_entity_t callback = ecs_new_named(0, "Callback::Created");
        ecs_set(callback, NamingValue, { 77 });
    }
}
ECS_COMPONENT_DEFINE(NamingValue, .on_remove = naming_value_removed);

static void naming_reset(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(NamingValue);
    naming_remove_count = 0;
    naming_mutate_on_remove = false;
}

static void naming_finish(void) {
    /* These tests count runtime deletions, not world finalization. */
    naming_remove_count = 0;
    naming_mutate_on_remove = false;
    ecs_fini();
}

void naming_paths_and_scopes(void) {
    naming_reset();
    ecs_entity_t a = ecs_new_named(0, "SceneA::Player");
    ecs_entity_t b = ecs_new_named(0, "SceneB::Player");
    ecs_entity_t root = ecs_lookup("SceneA");
    test_true(a != b);
    test_uint(a, ecs_lookup("::SceneA::Player"));
    test_uint(a, ecs_lookup_from(root, "Player"));
    test_uint(b, ecs_lookup_from(root, "::SceneB::Player"));
    test_uint(0, ecs_lookup("Player"));
    test_uint(0, ecs_lookup_from(a, "Player"));
    test_uint(0, ecs_lookup("sceneA::Player"));
    char *path = ecs_entity_path(a);
    test_str("::SceneA::Player", path);
    free(path);
    test_str("Player", ecs_entity_name(a));
    test_uint(a, ecs_new_named(0, "SceneA::Player"));
    test_uint(a, ecs_new_named(root, "Player"));
    naming_finish();
}

void naming_rename_updates_descendant_paths(void) {
    naming_reset();
    ecs_entity_t child = ecs_new_named(0, "Scene::Player::Weapon");
    ecs_entity_t scene = ecs_lookup("Scene");
    test_true(ecs_set_name(scene, "Level"));
    test_uint(0, ecs_lookup("Scene::Player::Weapon"));
    test_uint(child, ecs_lookup("Level::Player::Weapon"));
    char *path = ecs_entity_path(child);
    test_str("::Level::Player::Weapon", path);
    free(path);
    naming_finish();
}

void naming_set_component_copies_and_removes(void) {
    naming_reset();
    ecs_entity_t entity = ecs_new();
    char value[] = "Player";
    ecs_set(entity, Name, { value });
    value[0] = 'X';
    test_uint(entity, ecs_lookup("Player"));
    test_str("Player", ecs_entity_name(entity));
    Name moved = { strdup("Moved") };
    ecs_move_cid(entity, ecs_id(Name), &moved);
    test_null(moved.value);
    test_uint(entity, ecs_lookup("Moved"));
    test_uint(0, ecs_lookup("Player"));
    ecs_remove(entity, Name);
    test_uint(0, ecs_lookup("Moved"));
    test_false(ecs_has(entity, Name));
    test_true(ecs_set_name(entity, "Again"));
    test_true(ecs_set_name(entity, ""));
    test_uint(0, ecs_lookup("Again"));
    test_false(ecs_has(entity, Name));
    ecs_add(entity, Name);
    test_true(ecs_has(entity, Name));
    test_true(ecs_set_name(entity, NULL));
    test_false(ecs_has(entity, Name));
    naming_finish();
}

void naming_collision_deletes_previous_subtree(void) {
    naming_reset();
    ecs_entity_t previous = ecs_new_named(0, "Scene::Player");
    ecs_entity_t previous_child = ecs_new_named(previous, "OldChild");
    ecs_entity_t survivor = ecs_new_named(0, "Scene::Replacement");
    ecs_entity_t survivor_child = ecs_new_named(survivor, "Child");
    ecs_entity_t other = ecs_new_named(0, "Other::Player");
    ecs_set(previous, NamingValue, { 10 });
    ecs_set(previous_child, NamingValue, { 20 });
    ecs_set(survivor, NamingValue, { 30 });
    test_true(ecs_set_name(survivor, "Player"));
    test_false(ecs_is_alive(previous));
    test_false(ecs_is_alive(previous_child));
    test_true(ecs_is_alive(survivor));
    test_true(ecs_is_alive(other));
    test_uint(survivor, ecs_lookup("Scene::Player"));
    test_uint(survivor_child, ecs_lookup("Scene::Player::Child"));
    test_int(30, ecs_get(survivor, NamingValue)->value);
    test_int(2, naming_remove_count);
    test_true(
        (naming_removed[0] == previous && naming_values[0] == 10 &&
         naming_removed[1] == previous_child && naming_values[1] == 20) ||
        (naming_removed[1] == previous && naming_values[1] == 10 &&
         naming_removed[0] == previous_child && naming_values[0] == 20)
    );
    test_true(ecs_set_name(survivor, "Player"));
    test_int(2, naming_remove_count);
    naming_finish();
}

void naming_reparent_and_root_collision(void) {
    naming_reset();
    ecs_entity_t previous = ecs_new_named(0, "A::Player");
    ecs_entity_t victim_child = ecs_new_named(previous, "OldChild");
    ecs_entity_t moving = ecs_new_named(0, "B::Player");
    ecs_entity_t moving_child = ecs_new_named(moving, "Child");
    ecs_relate(moving, ChildOf, ecs_lookup("A"));
    test_false(ecs_is_alive(previous));
    test_false(ecs_is_alive(victim_child));
    test_uint(moving_child, ecs_lookup("A::Player::Child"));
    test_uint(0, ecs_lookup("B::Player"));
    ecs_entity_t root_victim = ecs_new_named(0, "Player");
    ecs_unrelate(moving, ChildOf);
    test_false(ecs_is_alive(root_victim));
    test_uint(moving, ecs_lookup("Player"));
    test_uint(moving_child, ecs_lookup("Player::Child"));
    naming_finish();
}

void naming_protects_ancestors_and_cycles(void) {
    naming_reset();
    ecs_entity_t ancestor = ecs_new_named(0, "Same");
    ecs_entity_t entity = ecs_new_named(ancestor, "Same");
    ecs_unrelate(entity, ChildOf); /* would delete its own ancestor */
    test_uint(ancestor, ecs_target(entity, ChildOf));
    test_uint(ancestor, ecs_lookup("Same"));
    test_uint(entity, ecs_lookup("Same::Same"));
    ecs_relate(ancestor, ChildOf, entity); /* cycle, rejected also in release */
    test_uint(0, ecs_target(ancestor, ChildOf));
    test_true(ecs_is_alive(ancestor));
    test_true(ecs_is_alive(entity));
    naming_finish();
}

void naming_invalid_input_is_non_destructive(void) {
    naming_reset();
    ecs_entity_t entity = ecs_new_named(0, "Player");
    test_false(ecs_set_name(entity, "Invalid::Name"));
    test_false(ecs_set_name(entity, "#reserved"));
    test_false(ecs_set_name(entity, "Trailing:"));
    test_false(ecs_set_name(0, "Name"));
    test_uint(0, ecs_new_named(0, "New::::Child"));
    test_uint(0, ecs_new_named(0, "New::"));
    test_uint(0, ecs_new_named(0, "New::Trailing:"));
    test_uint(0, ecs_lookup(""));
    test_uint(0, ecs_lookup(NULL));
    test_uint(0, ecs_lookup("New"));
    test_uint(entity, ecs_lookup("Player"));
    test_null(ecs_entity_path(0));
    naming_finish();
}

void naming_anonymous_segments_and_generations(void) {
    naming_reset();
    ecs_entity_t parent = ecs_new();
    ecs_entity_t child = ecs_new_named(parent, "Player");
    char *path = ecs_entity_path(child);
    test_uint(child, ecs_lookup(path));
    ecs_entity_t anonymous = ecs_new_named(child, NULL);
    char *anonymous_path = ecs_entity_path(anonymous);
    test_uint(anonymous, ecs_lookup(anonymous_path));
    ecs_kill(parent);
    test_uint(0, ecs_lookup(path));
    test_uint(0, ecs_lookup(anonymous_path));
    ecs_entity_t reused = ecs_new_named(0, "Reused");
    test_uint(reused, ecs_lookup("Reused"));
    test_null(ecs_entity_path(child));
    free(path);
    free(anonymous_path);
    naming_finish();
}

void naming_deferred_final_name_and_parent(void) {
    naming_reset();
    ecs_entity_t wrong_victim = ecs_new_named(0, "A::Final");
    ecs_entity_t real_victim = ecs_new_named(0, "B::Final");
    ecs_entity_t source = ecs_new_named(0, "A::Initial");
    ecs_defer_begin();
    ecs_set(source, Name, { "Intermediate" });
    ecs_set(source, Name, { "Final" });
    ecs_relate(source, ChildOf, ecs_lookup("B"));
    test_uint(source, ecs_lookup("A::Initial"));
    test_uint(real_victim, ecs_lookup("B::Final"));
    ecs_defer_end();
    test_true(ecs_is_alive(wrong_victim));
    test_false(ecs_is_alive(real_victim));
    test_uint(source, ecs_lookup("B::Final"));
    test_uint(0, ecs_lookup("A::Initial"));
    naming_finish();
}

void naming_deferred_creation_reuses_pending_paths(void) {
    naming_reset();
    ecs_defer_begin();
    ecs_entity_t first = ecs_new_named(0, "Scene::Player::Weapon");
    test_uint(first, ecs_new_named(0, "Scene::Player::Weapon"));
    test_uint(0, ecs_lookup("Scene::Player::Weapon"));
    ecs_entity_t sibling = ecs_new_named(0, "Other::Player::Weapon");
    test_true(sibling != first);
    ecs_defer_end();
    test_uint(first, ecs_lookup("Scene::Player::Weapon"));
    test_uint(sibling, ecs_lookup("Other::Player::Weapon"));
    naming_finish();
}

void naming_deferred_remove_kill_and_recreate(void) {
    naming_reset();
    ecs_entity_t first = ecs_new_named(0, "Player");
    ecs_defer_begin();
    ecs_remove(first, Name);
    ecs_entity_t next = ecs_new_named(0, "Player");
    test_true(first != next);
    ecs_defer_end();
    test_true(ecs_is_alive(first));
    test_false(ecs_has(first, Name));
    test_uint(next, ecs_lookup("Player"));
    ecs_defer_begin();
    ecs_kill(next);
    ecs_entity_t third = ecs_new_named(0, "Player");
    test_true(third != next);
    ecs_set(next, Name, { "IgnoredAfterKill" });
    ecs_defer_end();
    test_false(ecs_is_alive(next));
    test_uint(third, ecs_lookup("Player"));
    test_uint(0, ecs_lookup("IgnoredAfterKill"));
    naming_finish();
}

void naming_deferred_siblings_are_all_owned_by_parent(void) {
    naming_reset();
    ecs_entity_t children[4];
    ecs_defer_begin();
    for (int i = 0; i < 4; i++) {
        char path[40];
        snprintf(path, sizeof path, "Scene::Child%d", i);
        children[i] = ecs_new_named(0, path);
    }
    ecs_defer_end();
    ecs_entity_t parent = ecs_lookup("Scene");
    for (int i = 0; i < 4; i++)
        test_uint(parent, ecs_target(children[i], ChildOf));
    ecs_kill(parent);
    for (int i = 0; i < 4; i++)
        test_false(ecs_is_alive(children[i]));
    naming_finish();
}

static int naming_set_count;
static ecs_entity_t naming_set_entity;
static ecs_entity_t naming_set_parent;

static void naming_name_set(ecs_observer_event_t *event) {
    test_uint(naming_set_entity, event->entity);
    test_int(EcsOnSet, event->event);
    test_int(ecs_id(Name), event->component);
    test_str("Player", ((const Name *)event->trigger_data)->value);
    test_uint(naming_set_parent, ecs_target(event->entity, ChildOf));
    test_uint(event->entity, ecs_lookup_from(naming_set_parent, "Player"));
    naming_set_count++;
}

void naming_set_event_observes_final_identity(void) {
    naming_reset();
    naming_set_count = 0;
    naming_set_entity = ecs_new_named(0, "Before");
    naming_set_parent = ecs_new_named(0, "Scene");
    ecs_observer({ .on = EcsOnSet, .entity = naming_set_entity, .callback = naming_name_set });
    ecs_defer_begin();
    ecs_set(naming_set_entity, Name, { "Player" });
    ecs_relate(naming_set_entity, ChildOf, naming_set_parent);
    test_int(0, naming_set_count);
    ecs_defer_end();
    test_int(1, naming_set_count);
    test_true(ecs_set_name(naming_set_entity, "Player"));
    test_false(ecs_set_name(naming_set_entity, "Invalid::Local"));
    ecs_new_named(0, "Other::Player");
    test_int(1, naming_set_count);
    naming_finish();
}

void naming_collision_callbacks_can_allocate(void) {
    naming_reset();
    ecs_entity_t previous = ecs_new_named(0, "Player");
    ecs_entity_t replacement = ecs_new_named(0, "Replacement");
    ecs_set(previous, NamingValue, { 42 });
    naming_mutate_on_remove = true;
    test_true(ecs_set_name(replacement, "Player"));
    naming_mutate_on_remove = false;
    test_int(1, naming_remove_count);
    test_uint(previous, naming_removed[0]);
    test_int(42, naming_values[0]);
    test_uint(replacement, ecs_lookup("Player"));
    ecs_entity_t callback = ecs_lookup("Callback::Created");
    test_true(callback != 0);
    test_int(77, ecs_get(callback, NamingValue)->value);
    naming_finish();
}

void naming_prefab_instances_have_local_child_names(void) {
    naming_reset();
    ecs_entity_t prefab = ecs_new_named(0, "Prefab");
    ecs_entity_t original_child = ecs_new_named(prefab, "Weapon");
    ecs_entity_t original_grandchild = ecs_new_named(original_child, "Muzzle");
    ecs_entity_t first = ecs_new_named(0, "First");
    ecs_entity_t second = ecs_new_named(0, "Second");
    ecs_is_a(first, prefab);
    ecs_defer_begin();
    ecs_is_a(second, prefab);
    ecs_defer_end();
    ecs_entity_t first_child = ecs_lookup("First::Weapon");
    ecs_entity_t second_child = ecs_lookup("Second::Weapon");
    test_true(first_child != 0);
    test_true(second_child != 0);
    test_true(first_child != second_child && first_child != original_child);
    test_true(ecs_lookup("First::Weapon::Muzzle") != original_grandchild);
    test_true(ecs_lookup("First::Weapon::Muzzle") != 0);
    test_true(ecs_lookup("Second::Weapon::Muzzle") != 0);
    ecs_entity_t anonymous_instance = ecs_new();
    ecs_is_a(anonymous_instance, prefab);
    test_false(ecs_has(anonymous_instance, Name));
    test_null(ecs_try_get(anonymous_instance, Name));
    test_uint(prefab, ecs_lookup("Prefab"));
    ecs_query_id_t query = ecs_query({ .components = { ecs_in(Name) } });
    test_int(8, ecs_query_count(query));
    ecs_query_fini(query);
    naming_finish();
}

void naming_scene_roundtrip_preserves_duplicate_local_names(void) {
    naming_reset();
    ecs_new_named(0, "SceneA::Player::Weapon");
    ecs_new_named(0, "SceneB::Player::Weapon");
    void *data = NULL;
    size_t size = 0;
    test_true(ecs_save_memory(&data, &size));
    naming_finish();
    naming_reset();
    test_true(ecs_load_memory(data, size));
    ecs_scene_free(data);
    ecs_entity_t a = ecs_lookup("SceneA::Player::Weapon");
    ecs_entity_t b = ecs_lookup("SceneB::Player::Weapon");
    test_true(a != 0 && b != 0 && a != b);
    test_uint(0, ecs_lookup("Player"));
    naming_finish();
}

void naming_hash_growth_tombstones_and_restart(void) {
    naming_reset();
    const size_t lengths[] = { 1, 2, 3, 4, 8, 9, 16, 17, 32, 65, 256 };
    for (size_t i = 0; i < sizeof lengths / sizeof lengths[0]; i++) {
        char key[257];
        memset(key, 'L', lengths[i]);
        key[lengths[i]] = '\0';
        ecs_entity_t entity = ecs_new_named(0, key);
        test_uint(entity, ecs_lookup(key));
        test_true(entity != 0);
        ecs_kill(entity);
        test_uint(0, ecs_lookup(key));
    }

    char path[64];
    for (int i = 0; i < 2000; i++) {
        snprintf(path, sizeof path, "Scene%d::Player", i);
        test_true(ecs_new_named(0, path) != 0);
    }
    for (int i = 0; i < 2000; i++) {
        snprintf(path, sizeof path, "Scene%d", i);
        ecs_entity_t parent = ecs_lookup(path);
        test_true(parent != 0);
        if (i % 2)
            ecs_kill(parent);
        else {
            ecs_entity_t child = ecs_lookup_from(parent, "Player");
            test_true(ecs_set_name(child, "Renamed"));
            test_uint(child, ecs_lookup_from(parent, "Renamed"));
        }
    }
    for (int i = 0; i < 2000; i++) {
        snprintf(path, sizeof path, "Scene%d::Renamed", i);
        test_true((ecs_lookup(path) != 0) == (i % 2 == 0));
    }
    naming_finish();
    naming_reset();
    test_uint(0, ecs_lookup("Scene0"));
    test_true(ecs_new_named(0, "Scene0::Player") != 0);
    naming_finish();
}
