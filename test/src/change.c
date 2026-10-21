#include "world_internal.h"
#include <siecs_test.h>
#include <stdatomic.h>

ECS_COMPONENT_DECLARE(ChangePosition, { int value; });
ECS_COMPONENT_DECLARE(ChangeVelocity, { int value; });
ECS_COMPONENT_DECLARE(ChangeColor, { int value; });
ECS_TAG_DECLARE(ChangeTag);

ECS_COMPONENT_DEFINE(ChangePosition);
ECS_COMPONENT_DEFINE(ChangeVelocity);
ECS_COMPONENT_DEFINE(ChangeColor, .inheritance = EcsInheritShared);
ECS_TAG_DEFINE(ChangeTag);

static uint32_t change_rows, change_batches, change_structure_batches;
static uint32_t change_first_changed, change_second_changed;
static atomic_uint change_parallel_rows, change_parallel_calls;

static void change_world(uint16_t workers) {
    ecs_with_features({ .worker_threads = workers });
    ECS_COMPONENT_REGISTER(ChangePosition);
    ECS_COMPONENT_REGISTER(ChangeVelocity);
    ECS_COMPONENT_REGISTER(ChangeColor);
    ECS_COMPONENT_REGISTER(ChangeTag);
}

static void change_reset(void) {
    change_rows = change_batches = change_structure_batches = 0;
    change_first_changed = change_second_changed = 0;
}

static void change_count(ecs_iter_t *it) {
    change_rows += it->count;
    change_batches++;
    if (ecs_iter_structure_changed(it))
        change_structure_batches++;
}

static void change_count_fields(ecs_iter_t *it) {
    change_count(it);
    change_first_changed += ecs_iter_changed(it, 0);
    change_second_changed += ecs_iter_changed(it, 1);
}

static void change_increment(ecs_iter_t *it) {
    ChangePosition *p = ecs_field(it, 0);
    for (uint32_t i = 0; i < it->count; i++)
        p[i].value++;
    change_count(it);
}

static void change_copy_to_velocity(ecs_iter_t *it) {
    const ChangePosition *p = ecs_field(it, 0);
    ChangeVelocity *v = ecs_field(it, 1);
    for (uint32_t i = 0; i < it->count; i++)
        v[i].value = p[i].value;
}

static void change_set_position(ecs_iter_t *it) {
    const ChangePosition *p = ecs_field(it, 0);
    for (uint32_t i = 0; i < it->count; i++)
        ecs_set(it->entities[i], ChangePosition, { p[i].value + 1 });
    change_count(it);
}

static void change_parallel_count(ecs_iter_t *it) {
    atomic_fetch_add(&change_parallel_rows, it->count);
    atomic_fetch_add(&change_parallel_calls, 1);
}

static ecs_entity_t change_entity(int value) {
    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ChangePosition, { value });
    return entity;
}

void change_filter_skips_unchanged_tables(void) {
    change_world(0);
    ecs_entity_t plain = change_entity(0);
    change_entity(1);
    ecs_entity_t tagged = change_entity(2);
    ecs_add(tagged, ChangeTag);
    ecs_system(
        { .query.components = { ecs_in_changed(ChangePosition) }, .callback = change_count }
    );

    change_reset();
    ecs_progress();
    test_uint(3, change_rows);
    test_uint(2, change_batches);

    change_reset();
    ecs_progress();
    test_uint(0, change_batches);

    ecs_set(plain, ChangePosition, { 10 });
    change_reset();
    ecs_progress();
    test_uint(2, change_rows);
    test_uint(1, change_batches);
    test_uint(0, change_structure_batches);

    change_reset();
    ecs_progress();
    test_uint(0, change_batches);
    ecs_fini();
}

void change_filter_ignores_own_writes(void) {
    change_world(0);
    ecs_entity_t entity = change_entity(0);
    ecs_system(
        { .query.components = { ecs_inout_changed(ChangePosition) }, .callback = change_increment }
    );
    change_reset();
    ecs_progress();
    ecs_progress();
    test_uint(1, change_batches);
    test_int(1, ecs_get(entity, ChangePosition)->value);
    ecs_fini();
}

/* Commands are stamped when applied, after the writer's run started: the
 * writer sees them on its next run, unlike writes through its fields. */
void change_filter_sees_own_commands(void) {
    const uint16_t workers[] = { 0, 2 };
    for (uint32_t w = 0; w < 2; w++) {
        change_world(workers[w]);
        ecs_entity_t entity = change_entity(0);
        ecs_system(
            { .query.components = { ecs_in_changed(ChangePosition) },
              .callback = change_set_position }
        );
        change_reset();
        ecs_progress();
        ecs_progress();
        test_uint(2, change_batches);
        test_int(2, ecs_get(entity, ChangePosition)->value);
        ecs_fini();
    }
}

void change_skipped_table_does_not_stamp_writes(void) {
    change_world(0);
    ecs_entity_t moving = change_entity(0);
    ecs_set(moving, ChangeVelocity, { 0 });
    ecs_entity_t still = change_entity(5);
    ecs_set(still, ChangeVelocity, { 0 });
    ecs_add(still, ChangeTag);
    ecs_system(
        { .query.components = { ecs_in_changed(ChangePosition), ecs_out(ChangeVelocity) },
          .callback = change_copy_to_velocity }
    );
    ecs_system(
        { .query.components = { ecs_in_changed(ChangeVelocity) }, .callback = change_count }
    );

    change_reset();
    ecs_progress();
    test_uint(2, change_batches);
    test_int(5, ecs_get(still, ChangeVelocity)->value);

    ecs_set(moving, ChangePosition, { 7 });
    change_reset();
    ecs_progress();
    test_uint(1, change_batches);
    test_uint(1, change_rows);
    test_int(7, ecs_get(moving, ChangeVelocity)->value);

    change_reset();
    ecs_progress();
    test_uint(0, change_batches);
    ecs_fini();
}

void change_filter_sees_deferred_writes(void) {
    change_world(0);
    ecs_entity_t entity = change_entity(0);
    ecs_system(
        { .query.components = { ecs_in_changed(ChangePosition) }, .callback = change_count }
    );
    ecs_progress();

    ecs_defer_begin();
    ecs_set(entity, ChangePosition, { 3 });
    ecs_defer_end();
    change_reset();
    ecs_progress();
    test_uint(1, change_batches);

    ecs_modified(entity, ChangePosition);
    change_reset();
    ecs_progress();
    test_uint(1, change_batches);
    ecs_fini();
}

void change_filter_sees_structure_changes(void) {
    change_world(0);
    change_entity(0);
    ecs_entity_t second = change_entity(1);
    ecs_system(
        { .query.components = { ecs_in_changed(ChangePosition) }, .callback = change_count }
    );
    ecs_progress();

    change_entity(2);
    change_reset();
    ecs_progress();
    test_uint(1, change_batches);
    test_uint(1, change_structure_batches);
    test_uint(3, change_rows);

    ecs_kill(second);
    change_reset();
    ecs_progress();
    test_uint(1, change_structure_batches);
    test_uint(2, change_rows);

    change_reset();
    ecs_progress();
    test_uint(0, change_batches);
    ecs_fini();
}

void change_filter_sees_shared_base_writes(void) {
    change_world(0);
    ecs_entity_t base = ecs_new();
    ecs_set(base, ChangeColor, { 1 });
    ecs_add(base, Abstract);
    ecs_entity_t instance = change_entity(0);
    ecs_is_a(instance, base);
    ecs_system(
        { .query.components = { ecs_in(ChangePosition), ecs_in_changed(ChangeColor) },
          .callback = change_count }
    );
    change_reset();
    ecs_progress();
    test_uint(1, change_rows);

    change_reset();
    ecs_progress();
    test_uint(0, change_batches);

    ecs_set(base, ChangeColor, { 2 });
    change_reset();
    ecs_progress();
    test_uint(1, change_rows);
    test_uint(0, change_structure_batches);
    ecs_fini();
}

void change_iter_reports_changed_fields(void) {
    change_world(0);
    ecs_entity_t entity = change_entity(0);
    ecs_set(entity, ChangeVelocity, { 0 });
    ecs_system(
        { .query.components = { ecs_in_changed(ChangePosition), ecs_in_changed(ChangeVelocity) },
          .callback = change_count_fields }
    );
    ecs_progress();

    ecs_set(entity, ChangeVelocity, { 1 });
    change_reset();
    ecs_progress();
    test_uint(1, change_batches);
    test_uint(0, change_first_changed);
    test_uint(1, change_second_changed);
    ecs_fini();
}

void change_adhoc_query_records_completed_runs(void) {
    change_world(0);
    ecs_entity_t entity = change_entity(0);
    ecs_entity_t tagged = change_entity(1);
    ecs_add(tagged, ChangeTag);
    ecs_query_id_t query = ecs_query({ .components = { ecs_in_changed(ChangePosition) } });
    uint32_t batches = 0;
    for (ecs_iter_t it = ecs_query_iter(query); ecs_iter_next(&it);)
        batches++;
    test_uint(2, batches);

    batches = 0;
    for (ecs_iter_t it = ecs_query_iter(query); ecs_iter_next(&it);)
        batches++;
    test_uint(0, batches);

    ecs_set(entity, ChangePosition, { 2 });
    ecs_set(tagged, ChangePosition, { 3 });
    ecs_iter_t it = ecs_query_iter(query);
    test_assert(ecs_iter_next(&it));
    batches = 0;
    for (it = ecs_query_iter(query); ecs_iter_next(&it);)
        batches++;
    test_uint(2, batches);

    test_uint(2, ecs_query_count(query));
    ecs_query_fini(query);
    ecs_fini();
}

void change_parallel_skips_unchanged_tables(void) {
    for (uint16_t workers = 0; workers <= 2; workers += 2) {
        change_world(workers);
        ecs_entity_t entities[5];
        for (int i = 0; i < 5; i++)
            entities[i] = change_entity(i);
        ecs_system(
            { .query.components = { ecs_in_changed(ChangePosition) },
              .callback = change_parallel_count,
              .parallel = true,
              .chunk_size = 2 }
        );
        atomic_store(&change_parallel_rows, 0);
        atomic_store(&change_parallel_calls, 0);
        ecs_progress();
        test_uint(5, atomic_load(&change_parallel_rows));
        test_uint(3, atomic_load(&change_parallel_calls));

        atomic_store(&change_parallel_rows, 0);
        atomic_store(&change_parallel_calls, 0);
        ecs_progress();
        test_uint(0, atomic_load(&change_parallel_calls));

        ecs_set(entities[4], ChangePosition, { 9 });
        ecs_progress();
        test_uint(5, atomic_load(&change_parallel_rows));
        test_uint(3, atomic_load(&change_parallel_calls));
        ecs_fini();
    }
}

void change_disabled_system_keeps_changes(void) {
    change_world(0);
    ecs_entity_t entity = change_entity(0);
    ecs_system_id_t sys = ecs_system(
        { .query.components = { ecs_in_changed(ChangePosition) }, .callback = change_count }
    );
    ecs_progress();

    ecs_system_disable(sys);
    ecs_set(entity, ChangePosition, { 1 });
    change_reset();
    ecs_progress();
    test_uint(0, change_batches);

    ecs_system_enable(sys);
    ecs_progress();
    test_uint(1, change_batches);
    ecs_fini();
}
