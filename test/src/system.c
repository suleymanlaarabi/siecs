#include "world_internal.h"
#include <siecs_test.h>
#include <stdatomic.h>
#include <stdlib.h>

ECS_COMPONENT_DECLARE(SystemPosition, { int value; });
ECS_COMPONENT_DECLARE(SystemTag, { int value; });
ECS_COMPONENT_DECLARE(SystemBatchA, { int value; });
ECS_COMPONENT_DECLARE(SystemBatchB, { int value; });
ECS_COMPONENT_DECLARE(SystemBatchC, { int value; });

ECS_COMPONENT_DEFINE(SystemPosition);
ECS_COMPONENT_DEFINE(SystemTag);
ECS_COMPONENT_DEFINE(SystemBatchA);
ECS_COMPONENT_DEFINE(SystemBatchB);
ECS_COMPONENT_DEFINE(SystemBatchC);

static uint32_t system_calls;
static float system_delta_time;
static uint32_t system_seen;
static uint32_t system_order_count;
static int system_order[8];
static ecs_entity_t system_entity;
static uint32_t system_user_data_dtor_calls;
static atomic_uint parallel_entered;
static atomic_uint parallel_context_entered;
static atomic_bool parallel_context_invalid;
static atomic_uintptr_t parallel_stacks[2];
static atomic_uint manual_system_calls;
static atomic_uint after_stage;
static atomic_uint stress_system_calls;
static atomic_uint resource_stage;
static atomic_uint same_table_active;
static atomic_uint same_table_overlap;
#if defined(_MSC_VER)
#define SYSTEM_THREAD_LOCAL __declspec(thread)
#else
#define SYSTEM_THREAD_LOCAL _Thread_local
#endif
static SYSTEM_THREAD_LOCAL uint8_t system_thread_marker;
static atomic_uintptr_t main_thread_only_marker;

static void assert_parallel_worker_stacks(void) {
    const uintptr_t first = atomic_load(&parallel_stacks[0]);
    const uintptr_t second = atomic_load(&parallel_stacks[1]);
    test_assert(first != second);
}

static ecs_entity_t create_system_entity(int value);
static void add_tag_to_global_entity(ecs_iter_t *it);
static void count_tag_system(ecs_iter_t *it);

static void reset_system_test_state(void) {
    system_calls = 0;
    system_delta_time = 0.0f;
    system_seen = 0;
    system_order_count = 0;
}

static void count_system(ecs_iter_t *it) {
    SystemPosition *p = ecs_field(it, 0);

    for (uint32_t i = 0; i < it->count; i++) {
        p[i].value++;
        system_seen++;
    }

    system_calls++;
}

static void interval_system(ecs_iter_t *it) {
    system_calls++;
    system_delta_time = it->delta_time;
}

static void parallel_system_a(ecs_iter_t *it) {
    (void)it;
    uintptr_t marker = (uintptr_t)&it;
    atomic_store_explicit(&parallel_stacks[0], marker, memory_order_relaxed);
    atomic_fetch_add_explicit(&parallel_entered, 1, memory_order_release);
    while (atomic_load_explicit(&parallel_entered, memory_order_acquire) < 2) {
    }
}

static void parallel_system_b(ecs_iter_t *it) {
    (void)it;
    uintptr_t marker = (uintptr_t)&it;
    atomic_store_explicit(&parallel_stacks[1], marker, memory_order_relaxed);
    atomic_fetch_add_explicit(&parallel_entered, 1, memory_order_release);
    while (atomic_load_explicit(&parallel_entered, memory_order_acquire) < 2) {
    }
}

static void parallel_context_system(ecs_iter_t *it) {
    (void)it;

    if (!ecs_execution_context_current()->scheduler_parallel)
        atomic_store(&parallel_context_invalid, true);

    atomic_fetch_add_explicit(&parallel_context_entered, 1, memory_order_release);

    while (atomic_load_explicit(&parallel_context_entered, memory_order_acquire) < 2) {
    }
}

static void manual_system(ecs_iter_t *it) {
    (void)it;
    atomic_fetch_add_explicit(&manual_system_calls, 1, memory_order_relaxed);
}

static void after_first_system(ecs_iter_t *it) {
    (void)it;
    atomic_store_explicit(&after_stage, 1, memory_order_release);
}

static void after_second_system(ecs_iter_t *it) {
    (void)it;
    test_uint(1, atomic_load_explicit(&after_stage, memory_order_acquire));
    atomic_store_explicit(&after_stage, 2, memory_order_release);
}

static void stress_system(ecs_iter_t *it) {
    (void)it;
    atomic_fetch_add_explicit(&stress_system_calls, 1, memory_order_relaxed);
}

static void resource_writer_system(ecs_iter_t *it) {
    (void)it;
    ecs_set_resource(DeltaTime, { .value = 1.0f });
    atomic_store_explicit(&resource_stage, 1, memory_order_release);
}

static void resource_reader_system(ecs_iter_t *it) {
    (void)it;
    const uint32_t stage = atomic_load_explicit(&resource_stage, memory_order_acquire);
    test_uint(stage, 1);
    test_assert(ecs_get_resource_read(DeltaTime)->value == 1.0f);
    atomic_store_explicit(&resource_stage, 2, memory_order_release);
}

static void same_table_writer_system(ecs_iter_t *it) {
    (void)it;
    uint32_t active = atomic_fetch_add_explicit(&same_table_active, 1, memory_order_acq_rel);
    if (active != 0) {
        atomic_store_explicit(&same_table_overlap, 1, memory_order_release);
    }
    atomic_fetch_sub_explicit(&same_table_active, 1, memory_order_release);
}

static void main_thread_only_system(ecs_iter_t *it) {
    (void)it;
    atomic_store_explicit(
        &main_thread_only_marker,
        (uintptr_t)&system_thread_marker,
        memory_order_release
    );
}

static void order_pre_update(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 1;
}

static void order_pre_start(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 1;
}

static void order_start(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 2;
}

static void order_post_start(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 3;
}

static void order_update(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 4;
}

static void order_render(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 5;
}

static void order_first(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 10;
}

static void order_second(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 20;
}

static void no_query_system(ecs_iter_t *it) {
    test_not_null(it);
    test_uint(1, it->count);
    system_calls++;
}

static void user_data_system(ecs_iter_t *it) {
    int *value = (int *)it->user_data;
    test_int(42, *value);
    *value = 77;
    system_calls++;
}

static void user_data_dtor(uintptr_t user_data) {
    int *value = (int *)user_data;
    test_int(77, *value);
    system_user_data_dtor_calls++;
    free(value);
}

static void consume_next_batch_system(ecs_iter_t *it) {
    system_calls++;
    system_seen += it->count;

    if (ecs_iter_next(it)) {
        system_calls++;
        system_seen += it->count;
    }
}

static void remove_position_system(ecs_iter_t *it) {
    for (uint32_t i = 0; i < it->count; i++) {
        ecs_remove(it->entities[i], SystemPosition);
        system_seen++;
    }
    system_calls++;
}

static void add_tag_to_global_entity(ecs_iter_t *it) {
    ecs_add(system_entity, SystemTag);
    system_calls++;
}

static void count_tag_system(ecs_iter_t *it) {
    system_seen += it->count;
    system_calls++;
}

static void quit_system(ecs_iter_t *it) {
    ecs_quit();
    system_calls++;
}

static ecs_entity_t create_system_entity(int value) {
    ecs_entity_t entity = ecs_new();
    ecs_set(entity, SystemPosition, { value });
    return entity;
}

void system_run(void) {
    reset_system_test_state();

    ecs_init();

    ECS_COMPONENT_REGISTER(SystemPosition);
    ecs_entity_t entity = create_system_entity(41);

    ecs_system(
        {
            .name = "Count",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_inout(SystemPosition) } },
            .callback = count_system,
        }
    );

    ecs_progress();

    SystemPosition *p = ecs_get(entity, SystemPosition);
    test_assert(system_calls == 1);
    test_assert(system_seen == 1);
    test_assert(p->value == 42);

    ecs_fini();
}

void system_parallel_independent_callbacks(void) {
    atomic_store(&parallel_entered, 0);
    atomic_store(&parallel_stacks[0], 0);
    atomic_store(&parallel_stacks[1], 0);

    ecs_with_features({ .worker_threads = 1 });
    ECS_COMPONENT_REGISTER(SystemBatchA);
    ECS_COMPONENT_REGISTER(SystemBatchB);
    ecs_entity_t first = ecs_new();
    ecs_entity_t second = ecs_new();
    ecs_set(first, SystemBatchA, { 1 });
    ecs_set(second, SystemBatchB, { 2 });

    ecs_system(
        {
            .name = "ParallelA",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemBatchA) } },
            .callback = parallel_system_a,
        }
    );
    ecs_system(
        {
            .name = "ParallelB",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemBatchB) } },
            .callback = parallel_system_b,
        }
    );

    ecs_progress();

    test_uint(2, atomic_load(&parallel_entered));
    assert_parallel_worker_stacks();
    ecs_fini();
}

void system_parallel_worker_context_is_deferred(void) {
    atomic_store(&parallel_context_entered, 0);
    atomic_store(&parallel_context_invalid, false);

    ecs_with_features({ .worker_threads = 1 });

    ECS_COMPONENT_REGISTER(SystemBatchA);
    ECS_COMPONENT_REGISTER(SystemBatchB);

    ecs_entity_t first = ecs_new();
    ecs_entity_t second = ecs_new();

    ecs_set(first, SystemBatchA, { 1 });
    ecs_set(second, SystemBatchB, { 2 });

    ecs_system(
        {
            .name = "ParallelContextA",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemBatchA) } },
            .callback = parallel_context_system,
        }
    );

    ecs_system(
        {
            .name = "ParallelContextB",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemBatchB) } },
            .callback = parallel_context_system,
        }
    );

    ecs_progress();

    test_uint(2, atomic_load(&parallel_context_entered));
    const bool parallel_context_is_valid = !atomic_load(&parallel_context_invalid);
    test_assert(parallel_context_is_valid);

    ecs_fini();
}

void system_parallel_query_table_conflicts(void) {
    atomic_store(&parallel_entered, 0);
    atomic_store(&parallel_stacks[0], 0);
    atomic_store(&parallel_stacks[1], 0);
    atomic_store(&same_table_active, 0);
    atomic_store(&same_table_overlap, 0);

    ecs_with_features({ .worker_threads = 1 });
    ECS_COMPONENT_REGISTER(SystemBatchC);
    ecs_entity_t with_c = ecs_new();
    ecs_add(with_c, SystemBatchC);
    ecs_entity_t without_c = ecs_new();

    ecs_system(
        {
            .name = "WriteC",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_inout(SystemBatchC) } },
            .callback = parallel_system_a,
        }
    );
    ecs_system(
        {
            .name = "WriteNotC",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_not(SystemBatchC) } },
            .callback = parallel_system_b,
        }
    );
    ecs_progress();
    test_uint(2, atomic_load(&parallel_entered));
    assert_parallel_worker_stacks();

    ecs_system(
        {
            .name = "WriteSameTableA",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_inout(SystemBatchC) } },
            .callback = same_table_writer_system,
        }
    );
    ecs_system(
        {
            .name = "WriteSameTableB",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_inout(SystemBatchC) } },
            .callback = same_table_writer_system,
        }
    );
    ecs_progress();
    test_uint(0, atomic_load(&same_table_overlap));
    ecs_fini();
    (void)with_c;
    (void)without_c;
}

void system_batches_invalidate_after_table_creation(void) {
    ecs_with_features({ .worker_threads = 1 });
    ECS_COMPONENT_REGISTER(SystemBatchC);
    ecs_system(
        {
            .name = "LateTableA",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_inout(SystemBatchC) } },
            .callback = same_table_writer_system,
        }
    );
    ecs_system(
        {
            .name = "LateTableB",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_inout(SystemBatchC) } },
            .callback = same_table_writer_system,
        }
    );

    ecs_progress();
    ecs_phase_info_t *phase = ecs_system_index_get_phase(EcsOnUpdate);
    test_int(2, phase->plan_count);

    ecs_add(ecs_new(), SystemBatchC);
    ecs_progress();
    test_int(2, phase->plan_count);
    test_int(1, phase->dependencies.size);
    ecs_fini();
}

void system_indexed_queries_detect_late_overlap(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(SystemPosition);
    ecs_component_t groups[2] = { ecs_component({ 0 }), ecs_component({ 0 }) };
    for (uint32_t i = 0; i < 48; i++) {
        ecs_entity_t e = ecs_new();
        ecs_add_cid(e, ecs_component({ 0 }));
        ecs_add_cid(e, groups[i % 2]);
        ecs_set(e, SystemPosition, { (int)i });
    }
    for (uint32_t i = 0; i < 2; i++) {
        ecs_system(
            { .name = "IndexedWriter",
              .phase = EcsOnUpdate,
              .query = { .components = { ecs_inout(SystemPosition), { groups[i], EcsFilter } } },
              .callback = count_system }
        );
    }
    ecs_system_index_build_plan();
    ecs_phase_info_t *phase = ecs_system_index_get_phase(EcsOnUpdate);
    test_uint(2, phase->plan_count);
    ecs_entity_t shared = ecs_new();
    ecs_add_cid(shared, groups[0]);
    ecs_add_cid(shared, groups[1]);
    ecs_set(shared, SystemPosition, { 0 });
    ecs_system_index_build_plan();
    test_uint(2, phase->plan_count);
    test_uint(1, phase->dependencies.size);
    ecs_fini();
}

void system_main_thread_only(void) {
    atomic_store(&main_thread_only_marker, 0);
    ecs_with_features({ .worker_threads = 1 });
    ecs_system(
        {
            .name = "MainThreadOnly",
            .phase = EcsOnUpdate,
            .main_thread_only = true,
            .callback = main_thread_only_system,
        }
    );

    uintptr_t main_marker = (uintptr_t)&system_thread_marker;
    ecs_progress();
    test_uint(main_marker, atomic_load(&main_thread_only_marker));
    ecs_fini();
}

void system_parallel_after_is_a_barrier(void) {
    atomic_store(&after_stage, 0);
    ecs_with_features({ .worker_threads = 1 });

    ecs_system_id_t first = ecs_system(
        {
            .name = "First",
            .phase = EcsOnUpdate,
            .callback = after_first_system,
        }
    );
    ecs_system(
        {
            .name = "Second",
            .phase = EcsOnUpdate,
            .callback = after_second_system,
            .after = { first },
        }
    );

    ecs_progress();
    test_uint(2, atomic_load(&after_stage));
    ecs_fini();
}

void system_resource_access_conflicts(void) {
    atomic_store(&parallel_entered, 0);
    atomic_store(&parallel_stacks[0], 0);
    atomic_store(&parallel_stacks[1], 0);

    ecs_with_features({ .worker_threads = 1 });
    ecs_system(
        {
            .name = "ReadResourceA",
            .phase = EcsOnUpdate,
            .query = { .resources = { ecs_in(DeltaTime) } },
            .callback = parallel_system_a,
        }
    );
    ecs_system(
        {
            .name = "ReadResourceB",
            .phase = EcsOnUpdate,
            .query = { .resources = { ecs_in(DeltaTime) } },
            .callback = parallel_system_b,
        }
    );

    ecs_progress();
    test_uint(2, atomic_load(&parallel_entered));
    assert_parallel_worker_stacks();
    ecs_fini();

    atomic_store(&resource_stage, 0);
    ecs_with_features({ .worker_threads = 1 });
    ecs_system(
        {
            .name = "WriteResource",
            .phase = EcsOnUpdate,
            .query = { .resources = { ecs_out(DeltaTime) } },
            .callback = resource_writer_system,
        }
    );
    ecs_system(
        {
            .name = "ReadResourceAfterWrite",
            .phase = EcsOnUpdate,
            .query = { .resources = { ecs_in(DeltaTime) } },
            .callback = resource_reader_system,
        }
    );

    ecs_progress();
    test_uint(2, atomic_load(&resource_stage));
    ecs_fini();
}

void system_parallel_structural_changes_flush_at_barrier(void) {
    reset_system_test_state();
    ecs_with_features({ .worker_threads = 1 });
    ECS_COMPONENT_REGISTER(SystemPosition);
    ECS_COMPONENT_REGISTER(SystemTag);
    system_entity = create_system_entity(1);

    ecs_system_id_t writer = ecs_system(
        {
            .name = "AddTagAtBarrier",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = add_tag_to_global_entity,
        }
    );
    ecs_system(
        {
            .name = "ReadTagAfterBarrier",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemTag) } },
            .callback = count_tag_system,
            .after = { writer },
        }
    );

    ecs_progress();
    test_uint(1, system_seen);
    test_assert(ecs_has(system_entity, SystemTag));
    ecs_fini();
}

void system_manual_run_is_synchronous_with_workers(void) {
    atomic_store(&manual_system_calls, 0);
    ecs_with_features({ .worker_threads = 1 });
    ecs_system_id_t system = ecs_system(
        {
            .name = "Manual",
            .phase = EcsOnUpdate,
            .callback = manual_system,
        }
    );

    ecs_run_system(system);
    test_uint(1, atomic_load(&manual_system_calls));
    ecs_fini();
}

void system_worker_auto_and_reinit(void) {
    ecs_with_features({ .worker_threads = ECS_WORKERS_AUTO });
    ecs_fini();

    for (int i = 0; i < 3; i++) {
        ecs_with_features({ .worker_threads = 1 });
        ecs_progress();
        ecs_fini();
    }

    atomic_store(&stress_system_calls, 0);
    ecs_with_features({ .worker_threads = 1 });
    ECS_COMPONENT_REGISTER(SystemBatchA);
    ECS_COMPONENT_REGISTER(SystemBatchB);
    ecs_entity_t first = ecs_new();
    ecs_entity_t second = ecs_new();
    ecs_set(first, SystemBatchA, { 1 });
    ecs_set(second, SystemBatchB, { 2 });
    ecs_system(
        {
            .name = "StressA",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemBatchA) } },
            .callback = stress_system,
        }
    );
    ecs_system(
        {
            .name = "StressB",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemBatchB) } },
            .callback = stress_system,
        }
    );
    for (int frame = 0; frame < 500; frame++) {
        ecs_progress();
    }
    test_uint(1000, atomic_load(&stress_system_calls));
    ecs_fini();
    test_assert(true);
}

void system_name_returns_registered_name(void) {
    reset_system_test_state();

    ecs_init();
    ecs_system_id_t system = ecs_system(
        {
            .name = "NamedSystem",
            .phase = EcsOnUpdate,
            .callback = count_system,
        }
    );

    test_str("NamedSystem", ecs_system_name(system));

    ecs_fini();
}

void system_phase_order(void) {
    reset_system_test_state();

    ecs_init();

    ECS_COMPONENT_REGISTER(SystemPosition);
    create_system_entity(0);

    ecs_system(
        {
            .name = "Render",
            .phase = EcsOnRender,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = order_render,
        }
    );
    ecs_system(
        {
            .name = "Update",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = order_update,
        }
    );
    ecs_system(
        {
            .name = "PreUpdate",
            .phase = EcsPreUpdate,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = order_pre_update,
        }
    );

    ecs_progress();

    test_assert(system_order_count == 3);
    test_assert(system_order[0] == 1);
    test_assert(system_order[1] == 4);
    test_assert(system_order[2] == 5);

    ecs_fini();
}

void system_start_phases_run_once(void) {
    reset_system_test_state();

    ecs_init();

    ECS_COMPONENT_REGISTER(SystemPosition);
    create_system_entity(0);

    ecs_system(
        {
            .name = "PreStart",
            .phase = EcsPreStart,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = order_pre_start,
        }
    );
    ecs_system(
        {
            .name = "Start",
            .phase = EcsStart,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = order_start,
        }
    );
    ecs_system(
        {
            .name = "PostStart",
            .phase = EcsPostStart,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = order_post_start,
        }
    );
    ecs_system(
        {
            .name = "Update",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = order_update,
        }
    );

    ecs_progress();
    ecs_progress();

    test_assert(system_order_count == 5);
    test_assert(system_order[0] == 1);
    test_assert(system_order[1] == 2);
    test_assert(system_order[2] == 3);
    test_assert(system_order[3] == 4);
    test_assert(system_order[4] == 4);

    ecs_fini();
}

void system_after_order(void) {
    reset_system_test_state();

    ecs_init();

    ECS_COMPONENT_REGISTER(SystemPosition);
    create_system_entity(0);

    ecs_system_id_t first = ecs_system(
        {
            .name = "First",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = order_first,
        }
    );
    ecs_system(
        {
            .name = "Second",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = order_second,
            .after = { first },
        }
    );

    ecs_run_phase(EcsOnUpdate);

    test_assert(system_order_count == 2);
    test_assert(system_order[0] == 10);
    test_assert(system_order[1] == 20);

    ecs_fini();
}

void system_enable(void) {
    reset_system_test_state();

    ecs_init();

    ECS_COMPONENT_REGISTER(SystemPosition);
    create_system_entity(0);

    ecs_system_id_t system = ecs_system(
        {
            .name = "Disabled",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_inout(SystemPosition) } },
            .callback = count_system,
            .disabled = true,
        }
    );

    ecs_progress();
    test_assert(system_calls == 0);

    ecs_system_enable(system);
    ecs_progress();
    test_assert(system_calls == 1);

    ecs_system_disable(system);
    ecs_progress();
    test_assert(system_calls == 1);

    ecs_fini();
}

void system_without_query_runs_once(void) {
    reset_system_test_state();

    ecs_init();

    ecs_system(
        {
            .name = "NoQuery",
            .phase = EcsOnUpdate,
            .callback = no_query_system,
        }
    );

    ecs_progress();
    test_uint(1, system_calls);

    ecs_fini();
}

void system_user_data_is_passed_and_destroyed(void) {
    reset_system_test_state();
    system_user_data_dtor_calls = 0;

    ecs_init();
    int *value = malloc(sizeof(int));
    *value = 42;

    ecs_system(
        {
            .name = "UserData",
            .callback = user_data_system,
            .user_data = (uintptr_t)value,
            .user_data_dtor = user_data_dtor,
            .phase = EcsOnUpdate,
        }
    );

    ecs_progress();
    test_int(1, system_calls);
    test_int(0, system_user_data_dtor_calls);

    ecs_fini();
    test_int(1, system_user_data_dtor_calls);
}

void system_callback_can_advance_iterator(void) {
    reset_system_test_state();

    ecs_init();

    ECS_COMPONENT_REGISTER(SystemPosition);
    ECS_COMPONENT_REGISTER(SystemTag);

    ecs_entity_t first = create_system_entity(1);
    ecs_add(first, Disabled);

    ecs_entity_t second = create_system_entity(2);
    ecs_add(second, Disabled);
    ecs_set(second, SystemTag, { 1 });

    ecs_system(
        {
            .name = "ConsumeNextBatch",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemPosition), ecs_filter(Disabled) } },
            .callback = consume_next_batch_system,
        }
    );

    ecs_progress();

    test_uint(2, system_calls);
    test_uint(2, system_seen);

    ecs_fini();
}

void system_skips_disabled_by_default(void) {
    reset_system_test_state();

    ecs_init();

    ECS_COMPONENT_REGISTER(SystemPosition);
    ecs_entity_t enabled = create_system_entity(0);
    ecs_entity_t disabled = create_system_entity(0);
    ecs_add(disabled, Disabled);

    ecs_system(
        {
            .name = "SkipDisabled",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_inout(SystemPosition) } },
            .callback = count_system,
        }
    );

    ecs_progress();

    test_int(1, system_seen);
    test_int(1, ecs_get(enabled, SystemPosition)->value);
    test_int(0, ecs_get(disabled, SystemPosition)->value);

    ecs_fini();
}

void system_can_run_on_disabled_when_requested(void) {
    reset_system_test_state();

    ecs_init();

    ECS_COMPONENT_REGISTER(SystemPosition);
    ecs_entity_t enabled = create_system_entity(0);
    ecs_entity_t disabled = create_system_entity(0);
    ecs_add(disabled, Disabled);

    ecs_system(
        {
            .name = "RunDisabled",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_inout(SystemPosition), ecs_filter(Disabled) } },
            .callback = count_system,
        }
    );

    ecs_progress();

    test_int(1, system_seen);
    test_int(0, ecs_get(enabled, SystemPosition)->value);
    test_int(1, ecs_get(disabled, SystemPosition)->value);

    ecs_fini();
}

void system_defers_structural_changes_until_iteration_end(void) {
    reset_system_test_state();

    ecs_init();

    ECS_COMPONENT_REGISTER(SystemPosition);

    ecs_entity_t first = create_system_entity(1);
    ecs_entity_t second = create_system_entity(2);
    ecs_entity_t third = create_system_entity(3);

    ecs_system(
        {
            .name = "RemovePosition",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_inout(SystemPosition) } },
            .callback = remove_position_system,
        }
    );

    ecs_progress();

    test_uint(1, system_calls);
    test_uint(3, system_seen);
    test_assert(!ecs_has(first, SystemPosition));
    test_assert(!ecs_has(second, SystemPosition));
    test_assert(!ecs_has(third, SystemPosition));

    ecs_fini();
}

void system_flushes_between_ordered_systems(void) {
    reset_system_test_state();

    ecs_init();

    ECS_COMPONENT_REGISTER(SystemPosition);
    ECS_COMPONENT_REGISTER(SystemTag);

    system_entity = create_system_entity(1);
    ecs_system_id_t writer = ecs_system(
        {
            .name = "AddTag",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemPosition) } },
            .callback = add_tag_to_global_entity,
        }
    );
    ecs_system(
        {
            .name = "CountTag",
            .phase = EcsOnUpdate,
            .query = { .components = { ecs_in(SystemTag) } },
            .callback = count_tag_system,
            .after = { writer },
        }
    );

    ecs_progress();

    test_uint(2, system_calls);
    test_uint(1, system_seen);
    test_assert(ecs_has(system_entity, SystemTag));

    ecs_fini();
}

void system_manual_defer_coalesces_to_final_state(void) {
    ecs_init();

    ECS_COMPONENT_REGISTER(SystemBatchA);
    ECS_COMPONENT_REGISTER(SystemBatchB);
    ECS_COMPONENT_REGISTER(SystemBatchC);

    ecs_entity_t entity = ecs_new();

    ecs_defer_begin();
    ecs_add(entity, SystemBatchA);
    ecs_remove(entity, SystemBatchA);
    ecs_set(entity, SystemBatchB, { 10 });
    ecs_defer_begin();
    ecs_set(entity, SystemBatchC, { 20 });
    ecs_defer_end();

    test_assert(ecs_is_deferred());
    test_assert(!ecs_has(entity, SystemBatchA));
    test_assert(!ecs_has(entity, SystemBatchB));
    test_assert(!ecs_has(entity, SystemBatchC));

    ecs_defer_end();

    test_assert(!ecs_is_deferred());
    test_assert(!ecs_has(entity, SystemBatchA));
    test_assert(ecs_has(entity, SystemBatchB));
    test_assert(ecs_has(entity, SystemBatchC));
    test_int(10, ecs_get(entity, SystemBatchB)->value);
    test_int(20, ecs_get(entity, SystemBatchC)->value);

    ecs_fini();
}

void system_deferred_many_sets_survive_arena_growth(void) {
    enum { entity_count = 512 };

    ecs_init();
    ECS_COMPONENT_REGISTER(SystemBatchA);

    ecs_entity_t entities[entity_count];
    for (int i = 0; i < entity_count; i++) {
        entities[i] = ecs_new();
    }

    ecs_defer_begin();
    for (int i = 0; i < entity_count; i++) {
        ecs_set(entities[i], SystemBatchA, { i });
    }
    ecs_defer_end();

    for (int i = 0; i < entity_count; i++) {
        test_int(i, ecs_get(entities[i], SystemBatchA)->value);
    }

    ecs_fini();
}

void system_deferred_set_overwrite_keeps_latest_value(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(SystemBatchA);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, SystemBatchA, { -1 });

    ecs_defer_begin();
    for (int i = 0; i < 1024; i++) {
        ecs_set(entity, SystemBatchA, { i });
    }
    ecs_defer_end();

    test_int(1023, ecs_get(entity, SystemBatchA)->value);
    ecs_fini();
}

void system_deferred_set_adds_required_components(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(SystemBatchA);
    ECS_COMPONENT_REGISTER(SystemBatchB);
    ecs_with(SystemBatchA, SystemBatchB);

    ecs_entity_t entity = ecs_new();
    ecs_defer_begin();
    ecs_set(entity, SystemBatchA, { 42 });
    ecs_defer_end();

    test_assert(ecs_has(entity, SystemBatchA));
    test_assert(ecs_has(entity, SystemBatchB));
    test_int(42, ecs_get(entity, SystemBatchA)->value);
    ecs_fini();
}

void system_deferred_changes_coalesce_by_component(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(SystemBatchA);
    ECS_COMPONENT_REGISTER(SystemBatchB);
    ECS_COMPONENT_REGISTER(SystemBatchC);

    ecs_entity_t entity = ecs_new();
    ecs_entity_t existing = ecs_new();
    ecs_set(existing, SystemBatchA, { 1 });

    int moved = 22;
    ecs_defer_begin();
    ecs_add(entity, SystemBatchA);
    ecs_remove(entity, SystemBatchA);
    ecs_remove(entity, SystemBatchB);
    ecs_set(entity, SystemBatchB, { 10 });
    ecs_move_cid(entity, ecs_id(SystemBatchB), &moved);
    ecs_set(entity, SystemBatchC, { 3 });
    ecs_remove(entity, SystemBatchC);
    ecs_move_cid(existing, ecs_id(SystemBatchA), &moved);
    ecs_defer_end();

    test_false(ecs_has(entity, SystemBatchA));
    test_int(22, ecs_get(entity, SystemBatchB)->value);
    test_false(ecs_has(entity, SystemBatchC));
    test_int(22, ecs_get(existing, SystemBatchA)->value);

    ecs_fini();
}

void system_quit_makes_progress_return_false(void) {
    reset_system_test_state();

    ecs_init();

    ecs_system(
        {
            .name = "Quit",
            .phase = EcsOnUpdate,
            .callback = quit_system,
        }
    );

    test_false(ecs_progress());
    test_uint(1, system_calls);

    ecs_fini();
}

void system_interval_throttles_progress(void) {
    reset_system_test_state();
    ecs_init();

    ecs_system(
        {
            .name = "Interval",
            .phase = EcsOnUpdate,
            .callback = interval_system,
            .interval = 0.5,
        }
    );

    ecs_progress();
    test_uint(0, system_calls);

    ecs_world.last_time = ecs_platform_time_now_sec() - 2.0;

    ecs_progress();

    test_uint(1, system_calls);
    test_assert(system_delta_time >= 1.9f);

    ecs_world.last_time = ecs_platform_time_now_sec() - 0.001;

    ecs_progress();

    test_uint(1, system_calls);

    ecs_world.last_time = ecs_platform_time_now_sec() - 0.6;

    ecs_progress();

    test_uint(2, system_calls);
    test_assert(system_delta_time >= 0.5f);

    ecs_fini();
}

void system_interval_manual_execution_is_forced(void) {
    reset_system_test_state();
    ecs_init();

    ecs_system_id_t system = ecs_system(
        {
            .name = "IntervalManual",
            .phase = EcsOnUpdate,
            .callback = interval_system,
            .interval = 0.5,
        }
    );

    ecs_progress();
    test_uint(0, system_calls);

    ecs_world.last_time = ecs_platform_time_now_sec() - 0.1;

    ecs_progress();
    test_uint(0, system_calls);

    ecs_run_system(system);
    test_uint(1, system_calls);

    ecs_run_phase(EcsOnUpdate);
    test_uint(2, system_calls);

    ecs_world.last_time = ecs_platform_time_now_sec() - 0.45;

    ecs_progress();

    test_uint(3, system_calls);

    ecs_fini();
}

void system_interval_resets_on_enable_transition(void) {
    reset_system_test_state();
    ecs_init();

    ecs_system_id_t system = ecs_system(
        {
            .name = "IntervalEnable",
            .phase = EcsOnUpdate,
            .callback = interval_system,
            .interval = 0.5,
        }
    );

    ecs_progress();

    ecs_world.last_time = ecs_platform_time_now_sec() - 0.3;

    ecs_progress();
    test_uint(0, system_calls);

    ecs_system_disable(system);
    ecs_system_enable(system);

    ecs_world.last_time = ecs_platform_time_now_sec() - 0.25;

    ecs_progress();
    test_uint(0, system_calls);

    ecs_world.last_time = ecs_platform_time_now_sec() - 0.3;

    ecs_progress();
    test_uint(1, system_calls);

    ecs_fini();
}

void system_interval_does_not_gate_start_phase(void) {
    reset_system_test_state();
    ecs_init();

    ecs_system(
        {
            .name = "IntervalStart",
            .phase = EcsStart,
            .callback = interval_system,
            .interval = 60.0,
        }
    );

    ecs_progress();
    ecs_progress();

    test_uint(1, system_calls);

    ecs_fini();
}

static int custom_phase_order[10];
static int custom_phase_count = 0;

static void custom_phase_sys1(ecs_iter_t *it) {
    (void)it;
    custom_phase_order[custom_phase_count++] = 1;
}

static void custom_phase_sys2(ecs_iter_t *it) {
    (void)it;
    custom_phase_order[custom_phase_count++] = 2;
}

static void custom_phase_sys3(ecs_iter_t *it) {
    (void)it;
    custom_phase_order[custom_phase_count++] = 3;
}

void system_custom_phase(void) {
    custom_phase_count = 0;
    ecs_init();

    ecs_phase_t physics_phase = ecs_phase(
        {
            .name = "Physics",
            .after = EcsOnUpdate,
            .before = EcsPostUpdate,
        }
    );

    test_assert(physics_phase >= EcsPhaseCount);
    test_str("Physics", ecs_phase_name(physics_phase));

    ecs_system(
        {
            .name = "Sys1_OnUpdate",
            .phase = EcsOnUpdate,
            .callback = custom_phase_sys1,
        }
    );

    ecs_system(
        {
            .name = "Sys2_Physics",
            .phase = physics_phase,
            .callback = custom_phase_sys2,
        }
    );

    ecs_system(
        {
            .name = "Sys3_PostUpdate",
            .phase = EcsPostUpdate,
            .callback = custom_phase_sys3,
        }
    );

    ecs_progress();

    test_assert(custom_phase_count == 3);
    test_assert(custom_phase_order[0] == 1);
    test_assert(custom_phase_order[1] == 2);
    test_assert(custom_phase_order[2] == 3);

    ecs_fini();
}

static void custom_phase_expect_frames(int frames) {
    for (int frame = 0; frame < frames; frame++) {
        test_assert(custom_phase_order[frame * 3 + 0] == 1);
        test_assert(custom_phase_order[frame * 3 + 1] == 2);
        test_assert(custom_phase_order[frame * 3 + 2] == 3);
    }
    test_int(frames * 3, custom_phase_count);
}

static void custom_phase_register_around(ecs_phase_t middle) {
    ecs_system({ .phase = EcsOnUpdate, .callback = custom_phase_sys1 });
    ecs_system({ .phase = middle, .callback = custom_phase_sys2 });
    ecs_system({ .phase = EcsPostUpdate, .callback = custom_phase_sys3 });
}

void system_default_phase_is_on_update(void) {
    custom_phase_count = 0;
    ecs_init();

    ecs_system({ .phase = EcsOnUpdate, .callback = custom_phase_sys1 });
    ecs_system({ .callback = custom_phase_sys2 });
    ecs_system({ .phase = EcsPostUpdate, .callback = custom_phase_sys3 });

    ecs_progress();
    ecs_progress();
    custom_phase_expect_frames(2);

    ecs_fini();
}

void system_custom_phase_after_only(void) {
    custom_phase_count = 0;
    ecs_init();

    ecs_phase_t phase = ecs_phase({ .name = "AfterUpdate", .after = EcsOnUpdate });
    test_assert(phase >= EcsPhaseCount);
    custom_phase_register_around(phase);

    ecs_progress();
    ecs_progress();
    custom_phase_expect_frames(2);

    ecs_fini();
}

void system_custom_phase_unconstrained(void) {
    custom_phase_count = 0;
    ecs_init();

    custom_phase_register_around(ecs_phase({ .name = "Free" }));

    ecs_progress();
    ecs_progress();
    custom_phase_expect_frames(2);

    ecs_fini();
}

static atomic_uint portion_calls, portion_rows, portion_errors;
static atomic_uint portion_seen[2053];
static atomic_uintptr_t portion_threads[2];

static void portion_system(ecs_iter_t *it) {
    SystemPosition *positions = ecs_field(it, 0);
    uint32_t call = atomic_fetch_add(&portion_calls, 1);
    if (it->count > 1024) atomic_fetch_add(&portion_errors, 1);
    if (it->user_data && call < 2) {
        atomic_store(&portion_threads[call], (uintptr_t)&system_thread_marker);
        while (atomic_load(&portion_calls) < 2) {}
    }
    for (uint32_t i = 0; i < it->count; i++) {
        int value = positions[i].value;
        if (value < 0 || value >= 2053 || !ecs_is_alive(it->entities[i])) {
            atomic_fetch_add(&portion_errors, 1);
            continue;
        }
        atomic_fetch_add(&portion_seen[value], 1);
        positions[i].value += 10;
    }
    atomic_fetch_add(&portion_rows, it->count);
}

static void reset_portions(void) {
    atomic_store(&portion_calls, 0);
    atomic_store(&portion_rows, 0);
    atomic_store(&portion_errors, 0);
    for (uint32_t i = 0; i < 2053; i++) atomic_store(&portion_seen[i], 0);
}

void system_parallel_portions_cover_rows(void) {
    for (uint16_t workers = 0; workers <= 2; workers++) {
        ecs_with_features({ .worker_threads = workers });
        ECS_COMPONENT_REGISTER(SystemPosition);
        ECS_COMPONENT_REGISTER(SystemTag);
        reset_portions();
        ecs_entity_t entities[2053];
        for (int i = 0; i < 2053; i++) {
            entities[i] = create_system_entity(i);
            if (i >= 2050) ecs_add(entities[i], SystemTag);
        }
        ecs_system({ .query.components = { ecs_inout(SystemPosition) },
                     .callback = portion_system, .parallel = true, .user_data = workers != 0 });
        ecs_progress();
        test_uint(4, atomic_load(&portion_calls));
        test_uint(2053, atomic_load(&portion_rows));
        test_uint(0, atomic_load(&portion_errors));
        if (workers) {
            const uintptr_t first_thread = atomic_load(&portion_threads[0]);
            const uintptr_t second_thread = atomic_load(&portion_threads[1]);
            test_assert(first_thread != second_thread);
        }
        for (int i = 0; i < 2053; i++) {
            test_uint(1, atomic_load(&portion_seen[i]));
            test_int(i + 10, ecs_get(entities[i], SystemPosition)->value);
        }
        ecs_fini();
    }
}

void system_parallel_portions_empty_and_manual(void) {
    ecs_with_features({ .worker_threads = 2 });
    ECS_COMPONENT_REGISTER(SystemPosition);
    reset_portions();
    ecs_system_id_t sys = ecs_system({ .query.components = { ecs_inout(SystemPosition) },
                            .callback = portion_system, .parallel = true, .chunk_size = 2 });
    ecs_progress();
    test_uint(0, atomic_load(&portion_calls));
    ecs_entity_t entities[5];
    for (int i = 0; i < 5; i++) entities[i] = create_system_entity(i);
    ecs_run_system(sys);
    test_uint(3, atomic_load(&portion_calls));
    test_uint(5, atomic_load(&portion_rows));
    test_uint(0, atomic_load(&portion_errors));
    for (int i = 0; i < 5; i++) test_int(i + 10, ecs_get(entities[i], SystemPosition)->value);
    ecs_fini();
}

static atomic_uint dag_ready, dag_done;
static void dag_long(ecs_iter_t *it) {
    (void)it;
    atomic_store(&dag_ready, 1);
    while (!atomic_load(&dag_done)) {}
}
static void dag_short(ecs_iter_t *it) {
    (void)it;
    while (!atomic_load(&dag_ready)) {}
}
static void dag_successor(ecs_iter_t *it) {
    (void)it;
    atomic_store(&dag_done, 1);
}

void system_dependency_releases_without_group_barrier(void) {
    ecs_with_features({ .worker_threads = 1 });
    atomic_store(&dag_ready, 0);
    atomic_store(&dag_done, 0);
    ecs_system({ .callback = dag_long });
    ecs_system_id_t short_system = ecs_system({ .callback = dag_short });
    ecs_system({ .callback = dag_successor, .after = { short_system } });
    ecs_progress();
    test_uint(1, atomic_load(&dag_done));
    ecs_fini();
}

static ecs_entity_t created_entities[2][1100];
static ecs_entity_t recycled_entities[16];
static atomic_uint creation_entered, creation_errors;
static void concurrent_creator(ecs_iter_t *it) {
    uint32_t group = (uint32_t)it->user_data;
    atomic_fetch_add(&creation_entered, 1);
    while (atomic_load(&creation_entered) < 2) {}
    for (uint32_t i = 0; i < 1100; i++) {
        ecs_entity_t entity = i & 1 ? ecs_new_no_reuse() : ecs_new();
        created_entities[group][i] = entity;
        for (uint32_t j = 0; j < 16; j++) {
            ecs_entity_t current = ecs_entity_from_index(ecs_entity_id(recycled_entities[j]));
            if (ecs_is_alive(recycled_entities[j]) || (current && !ecs_is_alive(current)))
                atomic_fetch_add(&creation_errors, 1);
        }
        if (!ecs_is_alive(entity) || ecs_has(entity, SystemPosition) ||
            ecs_get(system_entity, SystemPosition)->value != 73)
            atomic_fetch_add(&creation_errors, 1);
        if (i % 3 == 0) ecs_set(entity, SystemPosition, { (int)(group * 1100 + i) });
        if (i % 3 == 1) ecs_kill(entity);
    }
}

static void check_concurrent_creation(bool cross_page) {
    ecs_with_features({ .worker_threads = 1 });
    ECS_COMPONENT_REGISTER(SystemPosition);
    system_entity = create_system_entity(73);
    ecs_entity_record_t *original = ecs_get_record(system_entity);
    if (cross_page) {
        while (ecs_entity_index_count() < ECS_ENTITY_PAGE_SIZE - 64)
            ecs_new_no_reuse();
    }
    for (uint32_t i = 0; i < 16; i++) recycled_entities[i] = ecs_new();
    for (uint32_t i = 0; i < 16; i++) ecs_kill(recycled_entities[i]);
    atomic_store(&creation_entered, 0);
    atomic_store(&creation_errors, 0);
    for (uint32_t group = 0; group < 2; group++)
        ecs_system({ .callback = concurrent_creator, .user_data = group });
    ecs_progress();
    test_uint(0, atomic_load(&creation_errors));
    test_assert(original == ecs_get_record(system_entity));
    for (uint32_t group = 0; group < 2; group++) {
        for (uint32_t i = 0; i < 1100; i++) {
            ecs_entity_t entity = created_entities[group][i];
            test_assert(ecs_is_alive(entity) == (i % 3 != 1));
            if (i % 3 == 0) test_int(group * 1100 + i, ecs_get(entity, SystemPosition)->value);
            if (i % 3 == 2) test_assert(!ecs_has(entity, SystemPosition));
        }
    }
    ecs_entity_t dead = created_entities[0][1];
    ecs_entity_t reused = ecs_new();
    test_assert(reused != dead);
    test_assert(!ecs_is_alive(dead));
    ecs_fini();
}

void system_parallel_creation_grows_stable_index(void) {
    check_concurrent_creation(false);
}

void system_parallel_creation_crosses_page_boundary(void) {
    check_concurrent_creation(true);
}

static int publication_values[512];
static ecs_entity_t publication_entities[512];
static uint32_t publication_count;
static void publication_observer(ecs_observer_event_t *event) {
    if (publication_count < 512) {
        publication_entities[publication_count] = event->entity;
        publication_values[publication_count++] = ((const SystemTag *)event->trigger_data)->value;
    }
}
static void portion_commands(ecs_iter_t *it) {
    const SystemPosition *positions = ecs_field(it, 0);
    for (uint32_t i = 0; i < it->count; i++)
        ecs_set(it->entities[i], SystemTag, { (int)it->user_data + positions[i].value });
}
void system_parallel_commands_have_logical_order(void) {
    for (uint16_t workers = 0; workers <= 2; workers++) {
        ecs_with_features({ .worker_threads = workers });
        ECS_COMPONENT_REGISTER(SystemPosition);
        ECS_COMPONENT_REGISTER(SystemTag);
        ecs_entity_t entities[9];
        for (int i = 0; i < 9; i++) entities[i] = create_system_entity(i);
        publication_count = 0;
        ecs_observer({ .on = EcsOnSet, .query.components = { ecs_in(SystemTag) },
                       .callback = publication_observer });
        for (int group = 0; group < 2; group++)
            ecs_system({ .query.components = { ecs_in(SystemPosition) },
                         .callback = portion_commands, .parallel = true,
                         .chunk_size = 2, .user_data = group * 100 });
        for (int frame = 0; frame < 2; frame++) {
            publication_count = 0;
            ecs_progress();
            test_uint(18, publication_count);
            for (uint32_t i = 0; i < 18; i++) {
                test_uint(entities[i % 9], publication_entities[i]);
                test_int((i / 9) * 100 + i % 9, publication_values[i]);
            }
        }
        ecs_fini();
    }
}

static void create_overlapping_table(ecs_iter_t *it) {
    (void)it;
    ecs_entity_t entity = ecs_new();
    system_entity = entity;
    ecs_set(entity, SystemPosition, { 0 });
    ecs_add(entity, SystemBatchA);
    ecs_add(entity, SystemBatchB);
}
static void ordered_overlap_writer(ecs_iter_t *it) {
    SystemPosition *positions = ecs_field(it, 0);
    for (uint32_t i = 0; i < it->count; i++) {
        if (positions[i].value != (int)it->user_data) atomic_fetch_add(&portion_errors, 1);
        positions[i].value++;
        atomic_fetch_add(&portion_rows, 1);
    }
}
void system_publication_rechecks_same_phase_conflicts(void) {
    for (uint32_t exclusive = 0; exclusive < 2; exclusive++) {
        ecs_with_features({ .worker_threads = 2 });
        ECS_COMPONENT_REGISTER(SystemPosition, SystemBatchA, SystemBatchB);
        reset_portions();
        ecs_system_id_t creator = ecs_system({ .callback = create_overlapping_table, .main_thread_only = exclusive != 0 });
        ecs_system({ .after = { creator }, .callback = ordered_overlap_writer,
                     .query.components = { ecs_inout(SystemPosition), ecs_filter(SystemBatchA) } });
        ecs_system({ .after = { creator }, .callback = ordered_overlap_writer, .user_data = 1,
                     .query.components = { ecs_inout(SystemPosition), ecs_filter(SystemBatchB) } });
        ecs_progress();
        test_uint(3, ecs_system_index_get_phase(EcsOnUpdate)->dependencies.size);
        test_uint(2, atomic_load(&portion_rows));
        test_uint(0, atomic_load(&portion_errors));
        test_int(2, ecs_get(system_entity, SystemPosition)->value);
        ecs_fini();
    }
}

ECS_COMPONENT_DECLARE(SystemShared, { int value; });
ECS_COMPONENT_DEFINE(SystemShared, .inheritance = EcsInheritShared);
ECS_RELATION_DECLARE(SystemDense);
ECS_RELATION_DEFINE(SystemDense, { .storage = EcsRelationDense, .on_delete_target = EcsRemoveRelation });

static void shared_portion(ecs_iter_t *it) {
    SystemPosition *positions = ecs_field(it, 0);
    const SystemShared *shared = ecs_field(it, 1);
    if (!shared || !ecs_field_is_shared(it, 1) || shared->value != 81)
        atomic_fetch_add(&portion_errors, 1);
    for (uint32_t i = 0; i < it->count; i++) {
        if (ecs_get(it->entities[i], SystemPosition)->value != positions[i].value)
            atomic_fetch_add(&portion_errors, 1);
        positions[i].value += shared ? shared->value : 0;
    }
    atomic_fetch_add(&portion_calls, 1);
    atomic_fetch_add(&portion_rows, it->count);
}
void system_parallel_shared_and_up_fields(void) {
    for (uint32_t up = 0; up < 2; up++) {
        ecs_with_features({ .worker_threads = 2 });
        ECS_COMPONENT_REGISTER(SystemPosition, SystemShared);
        reset_portions();
        ecs_entity_t base = ecs_new();
        ecs_set(base, SystemShared, { 81 });
        ecs_add(base, Abstract);
        ecs_entity_t entities[7];
        for (int i = 0; i < 7; i++) {
            entities[i] = create_system_entity(i);
            ecs_is_a(entities[i], base);
        }
        ecs_component_term_t shared = up ? ecs_up(SystemShared, IsA) : ecs_in(SystemShared);
        ecs_system({ .parallel = true, .chunk_size = 2, .callback = shared_portion,
                     .query.components = { ecs_inout(SystemPosition), shared } });
        ecs_progress();
        test_uint(4, atomic_load(&portion_calls));
        test_uint(7, atomic_load(&portion_rows));
        test_uint(0, atomic_load(&portion_errors));
        for (int i = 0; i < 7; i++) test_int(i + 81, ecs_get(entities[i], SystemPosition)->value);
        ecs_fini();
    }
}

static ecs_entity_t relation_targets[9];
static void relation_portion(ecs_iter_t *it) {
    const SystemPosition *positions = ecs_field(it, 0);
    const ecs_relation_target_t *targets = ecs_targets(it, SystemDense);
    const ecs_relation_target_t *parents = ecs_targets(it, ChildOf);
    ecs_relation_batch_t batch = ecs_relation_batch(it, SystemDense);
    ecs_relation_batch_t shared = ecs_relation_batch(it, IsA);
    for (uint32_t i = 0; i < it->count; i++) {
        int row = positions[i].value;
        if (row < 0 || row >= 9 || !targets || !parents || batch.shared ||
            targets[i].entity != relation_targets[row] || batch.targets != targets ||
            parents[i].entity != relation_targets[0] || !shared.shared ||
            shared.shared_target != relation_targets[1]) atomic_fetch_add(&portion_errors, 1);
    }
    atomic_fetch_add(&portion_rows, it->count);
    atomic_fetch_add(&portion_calls, 1);
}
void system_parallel_relation_views_follow_portions(void) {
    ecs_with_features({ .worker_threads = 2 });
    ECS_COMPONENT_REGISTER(SystemPosition);
    ECS_RELATION_REGISTER(SystemDense);
    reset_portions();
    for (int i = 0; i < 9; i++) relation_targets[i] = ecs_new();
    for (int i = 0; i < 9; i++) {
        ecs_entity_t entity = create_system_entity(i);
        ecs_relate(entity, SystemDense, relation_targets[i]);
        ecs_relate(entity, ChildOf, relation_targets[0]);
        ecs_is_a(entity, relation_targets[1]);
    }
    ecs_system({ .parallel = true, .chunk_size = 2, .callback = relation_portion,
                 .query.components = { ecs_in(SystemPosition) },
                 .query.relations = { ecs_rel(SystemDense), ecs_rel(ChildOf), ecs_rel(IsA) } });
    ecs_progress();
    test_uint(9, atomic_load(&portion_rows));
    test_uint(5, atomic_load(&portion_calls));
    test_uint(0, atomic_load(&portion_errors));
    ecs_fini();
}

void system_graph_keeps_all_after_dependencies(void) {
    ecs_init();
    ecs_system_id_t first = ecs_system({ .callback = manual_system });
    ecs_system_id_t second = ecs_system({ .callback = manual_system });
    ecs_system_id_t third = ecs_system({ .callback = manual_system, .after = { first, second } });
    ecs_system_index_build_plan();
    const ecs_phase_info_t *phase = ecs_system_index_get_phase(EcsOnUpdate);
    const ecs_system_edge_t *edges = phase->dependencies.data;
    test_uint(2, phase->dependencies.size);
    test_uint(first, edges[0].before);
    test_uint(second, edges[1].before);
    test_uint(third, edges[0].after);
    test_uint(third, edges[1].after);
    ecs_fini();
}

#ifdef _WIN32
#include <process.h>
#include <string.h>
#include <windows.h>
#endif
static bool system_invalid_contract(const char *testcase) {
#ifdef _WIN32
    const char *child = getenv("SIECS_SYSTEM_ABORT_TEST");
    if (child && strcmp(child, testcase) == 0) {
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        return false;
    }
    char executable[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, executable, sizeof executable);
    test_assert(length > 0 && length < sizeof executable);
    test_int(0, _putenv_s("SIECS_SYSTEM_ABORT_TEST", testcase));
    const char *arguments[] = { executable, testcase, NULL };
    intptr_t status = _spawnv(_P_WAIT, executable, arguments);
    test_int(0, _putenv_s("SIECS_SYSTEM_ABORT_TEST", ""));
    test_int(3, status);
    return true;
#else
    (void)testcase;
    test_expect_abort();
    return false;
#endif
}
void system_parallel_rejects_tasks(void) {
    if (system_invalid_contract("system.parallel_rejects_tasks")) return;
    ecs_init();
    ecs_system({ .parallel = true, .callback = manual_system });
}
void system_parallel_rejects_resource_writes(void) {
    if (system_invalid_contract("system.parallel_rejects_resource_writes")) return;
    ecs_init();
    ECS_COMPONENT_REGISTER(SystemPosition);
    ecs_system({ .parallel = true, .callback = manual_system,
                 .query.components = { ecs_in(SystemPosition) },
                 .query.resources = { ecs_inout(DeltaTime) } });
}
void system_parallel_rejects_main_thread(void) {
    if (system_invalid_contract("system.parallel_rejects_main_thread")) return;
    ecs_init();
    ECS_COMPONENT_REGISTER(SystemPosition);
    ecs_system({ .parallel = true, .main_thread_only = true, .callback = manual_system,
                 .query.components = { ecs_in(SystemPosition) } });
}
void system_parallel_rejects_no_defer(void) {
    if (system_invalid_contract("system.parallel_rejects_no_defer")) return;
    ecs_init();
    ECS_COMPONENT_REGISTER(SystemPosition);
    ecs_system({ .parallel = true, .no_defer = true, .callback = manual_system,
                 .query.components = { ecs_in(SystemPosition) } });
}
void system_parallel_rejects_ordered_queries(void) {
    if (system_invalid_contract("system.parallel_rejects_ordered_queries")) return;
    ecs_init();
    ECS_COMPONENT_REGISTER(SystemPosition);
    ecs_system({ .parallel = true, .callback = manual_system,
                 .query.components = { ecs_in(SystemPosition) },
                 .query.order_by = ecs_order_by_target(IsA) });
}
void system_parallel_rejects_query_destruction(void) {
    if (system_invalid_contract("system.parallel_rejects_query_destruction")) return;
    ecs_init();
    ecs_query_id_t query = ecs_query({ 0 });
    ecs_world.main_context.scheduler_parallel = true;
    ecs_query_fini(query);
}

static void shared_base_writer(ecs_iter_t *it) {
    SystemShared *shared = ecs_field(it, 0);
    for (uint32_t i = 0; i < it->count; i++) shared[i].value = 81;
}
void system_shared_reads_conflict_across_tables(void) {
    for (uint32_t up = 0; up < 2; up++) {
        ecs_with_features({ .worker_threads = 2 });
        ECS_COMPONENT_REGISTER(SystemPosition, SystemShared);
        reset_portions();
        ecs_entity_t base = ecs_new();
        ecs_set(base, SystemShared, { 0 });
        ecs_add(base, Abstract);
        ecs_entity_t entity = create_system_entity(5);
        ecs_is_a(entity, base);
        ecs_system({ .query.components = { ecs_inout(SystemShared), ecs_filter(Abstract) },
                     .callback = shared_base_writer });
        ecs_component_term_t field = up ? ecs_up(SystemShared, IsA) : ecs_in(SystemShared);
        ecs_system({ .query.components = { ecs_inout(SystemPosition), field },
                     .callback = shared_portion, .parallel = true });
        ecs_system_index_build_plan();
        test_uint(1, ecs_system_index_get_phase(EcsOnUpdate)->dependencies.size);
        ecs_progress();
        test_uint(1, atomic_load(&portion_rows));
        test_uint(0, atomic_load(&portion_errors));
        test_int(86, ecs_get(entity, SystemPosition)->value);
        ecs_fini();
    }
}

void system_serial_commands_keep_registration_order(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(SystemPosition, SystemTag);
    ecs_entity_t entities[9];
    for (int i = 0; i < 9; i++) entities[i] = create_system_entity(i);
    publication_count = 0;
    ecs_observer({ .on = EcsOnSet, .query.components = { ecs_in(SystemTag) },
                   .callback = publication_observer });
    ecs_system_id_t prerequisite = ecs_system({ .callback = manual_system });
    ecs_system({ .query.components = { ecs_in(SystemPosition) },
                 .callback = portion_commands, .after = { prerequisite } });
    ecs_system({ .query.components = { ecs_in(SystemPosition) },
                 .callback = portion_commands, .user_data = 100 });
    ecs_progress();
    test_uint(18, publication_count);
    for (uint32_t i = 0; i < 18; i++) {
        test_uint(entities[i % 9], publication_entities[i]);
        test_int((i / 9) * 100 + i % 9, publication_values[i]);
    }
    ecs_fini();
}

void system_empty_portion_releases_main_thread_successor(void) {
    for (uint16_t workers = 0; workers < 2; workers++) {
        ecs_with_features({ .worker_threads = workers });
        ECS_COMPONENT_REGISTER(SystemPosition);
        atomic_store(&manual_system_calls, 0);
        ecs_system_id_t empty = ecs_system({ .parallel = true,
            .query.components = { ecs_in(SystemPosition) }, .callback = portion_system });
        ecs_system({ .main_thread_only = true, .after = { empty }, .callback = manual_system });
        ecs_progress();
        test_uint(1, atomic_load(&manual_system_calls));
        ecs_fini();
    }
}

static atomic_uint reservation_calls, reservation_errors;
static ecs_entity_t reservation_entities[2][2];
static bool reservation_fresh;

static void reserve_small_batch(ecs_iter_t *it) {
    uint32_t group = (uint32_t)it->user_data;
    atomic_fetch_add(&reservation_calls, 1);
    while (atomic_load(&reservation_calls) < 2) {}
    reservation_entities[group][0] = ecs_new();
    if (reservation_fresh)
        reservation_entities[group][1] = ecs_new_no_reuse();
    for (uint32_t i = 0; i < (reservation_fresh ? 2u : 1u); i++) {
        ecs_entity_t entity = reservation_entities[group][i];
        if (!ecs_is_alive(entity) || ecs_entity_from_index(ecs_entity_id(entity)) != entity)
            atomic_fetch_add(&reservation_errors, 1);
    }
}

static void check_reservation_returns(bool fresh) {
    ecs_with_features({ .worker_threads = 1 });
    uint32_t initial_count = ecs_entity_index_count();
    if (!fresh) {
        ecs_entity_t entities[600];
        for (uint32_t i = 0; i < 600; i++) entities[i] = ecs_new();
        for (uint32_t i = 0; i < 600; i++) ecs_kill(entities[i]);
        initial_count = ecs_entity_index_count();
    }
    reservation_fresh = fresh;
    atomic_store(&reservation_calls, 0);
    atomic_store(&reservation_errors, 0);
    for (uint32_t group = 0; group < 2; group++)
        ecs_system({ .callback = reserve_small_batch, .user_data = group });
    ecs_progress();
    test_uint(2, atomic_load(&reservation_calls));
    test_uint(0, atomic_load(&reservation_errors));
    uint32_t end = ecs_entity_index_count();
    test_uint(fresh ? initial_count + 2 * ECS_ENTITY_RESERVATION_SIZE : initial_count, end);
    uint32_t expected = fresh ? 2 * ECS_ENTITY_RESERVATION_SIZE - 4 : 598;
    test_uint(expected, entity_index.available.size);
    for (uint32_t i = 0; i < expected; i++) {
        uint32_t id = ((const uint32_t *)entity_index.available.data)[i];
        test_uint(0, ecs_entity_from_index(id));
        test_assert(!ecs_is_alive(ecs_entity(id, ecs_get_record(ecs_entity(id, 0))->generation)));
    }
    // Every returned slot must be issued once, with no fresh high-water growth.
    uint8_t *seen = calloc(end, sizeof *seen);
    for (uint32_t group = 0; group < 2; group++) {
        for (uint32_t i = 0; i < (fresh ? 2u : 1u); i++) {
            ecs_entity_t entity = reservation_entities[group][i];
            seen[ecs_entity_id(entity)]++;
            test_assert(ecs_is_alive(entity));
        }
    }
    for (uint32_t i = 0; i < expected; i++) {
        ecs_entity_t entity = ecs_new();
        test_uint(0, seen[ecs_entity_id(entity)]++);
        test_assert(ecs_is_alive(entity));
        test_uint(entity, ecs_entity_from_index(ecs_entity_id(entity)));
    }
    free(seen);
    test_uint(end, ecs_entity_index_count());
    test_uint(0, entity_index.available.size);
    ecs_entity_t no_reuse = ecs_new_no_reuse();
    test_uint(end, ecs_entity_id(no_reuse));
    ecs_fini();
}

void system_parallel_returns_unused_recycled_ids(void) {
    check_reservation_returns(false);
}

void system_parallel_returns_unused_fresh_ids(void) {
    check_reservation_returns(true);
}

static ecs_entity_t exclusive_reserved_entity;
static atomic_uint exclusive_reservation_calls;

static void reserve_before_exclusive(ecs_iter_t *it) {
    (void)it;
    exclusive_reserved_entity = ecs_new();
}

static void change_exclusive_reservations(ecs_iter_t *it) {
    (void)it;
    atomic_fetch_add(&exclusive_reservation_calls, 1);
    ecs_entity_t entity = ecs_new();
    ecs_kill(entity);
    ecs_entity_t recycled = ecs_new();
    if (ecs_entity_id(entity) != ecs_entity_id(recycled) || ecs_is_alive(entity) ||
        !ecs_is_alive(recycled) || !ecs_is_alive(exclusive_reserved_entity))
        atomic_fetch_add(&reservation_errors, 1);
}

void system_parallel_reservations_suspend_for_exclusive_callbacks(void) {
    ecs_with_features({ .worker_threads = 2 });
    atomic_store(&reservation_errors, 0);
    atomic_store(&exclusive_reservation_calls, 0);
    ecs_system({ .callback = reserve_before_exclusive });
    ecs_system({ .callback = change_exclusive_reservations,
                 .main_thread_only = true, .no_defer = true });
    ecs_system({ .callback = reserve_before_exclusive });
    for (uint32_t frame = 0; frame < 3; frame++) ecs_progress();
    test_uint(3, atomic_load(&exclusive_reservation_calls));
    test_uint(0, atomic_load(&reservation_errors));
    test_assert(ecs_is_alive(exclusive_reserved_entity));
    ecs_fini();
}

static atomic_bool later_batch_ready;
static void batch_order_commands(ecs_iter_t *it) {
    const SystemPosition *positions = ecs_field(it, 0);
    if (it->user_data && positions[0].value == 0) {
        while (!atomic_load(&later_batch_ready)) {}
    } else if (positions[0].value >= 4) {
        atomic_store(&later_batch_ready, true);
    }
    portion_commands(it);
}

void system_parallel_batched_commands_keep_logical_order(void) {
    for (uint16_t workers = 0; workers <= 3; workers++) {
        ecs_with_features({ .worker_threads = workers });
        ECS_COMPONENT_REGISTER(SystemPosition, SystemTag);
        ecs_entity_t entities[193];
        for (int i = 0; i < 193; i++) entities[i] = create_system_entity(i);
        ecs_observer({ .on = EcsOnSet, .query.components = { ecs_in(SystemTag) },
                       .callback = publication_observer });
        for (uint32_t group = 0; group < 2; group++)
            ecs_system({ .query.components = { ecs_in(SystemPosition) },
                         .callback = batch_order_commands, .parallel = true,
                         .chunk_size = 1, .user_data = group * 100 });
        for (uint32_t frame = 0; frame < 3; frame++) {
            atomic_store(&later_batch_ready, workers == 0);
            publication_count = 0;
            ecs_progress();
            test_uint(386, publication_count);
            for (uint32_t i = 0; i < 386; i++) {
                test_uint(entities[i % 193], publication_entities[i]);
                test_int((i / 193) * 100 + i % 193, publication_values[i]);
            }
        }
        ecs_fini();
    }
}
