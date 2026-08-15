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
static uint32_t system_seen;
static uint32_t system_order_count;
static int system_order[8];
static ecs_entity_t system_entity;
static uint32_t system_user_data_dtor_calls;
static atomic_uint parallel_entered;
static atomic_uint parallel_context_entered;
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

static ecs_entity_t create_system_entity(int value);
static void add_tag_to_global_entity(ecs_iter_t *it);
static void count_tag_system(ecs_iter_t *it);

static void reset_system_test_state(void) {
    system_calls = 0;
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

    test_assert(ecs_execution_context_current()->scheduler_parallel);

    atomic_fetch_add_explicit(
        &parallel_context_entered,
        1,
        memory_order_release
    );

    while (atomic_load_explicit(
               &parallel_context_entered,
               memory_order_acquire
           ) < 2) {
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
    test_assert(atomic_load_explicit(&resource_stage, memory_order_acquire) == 1);
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
            .query = { .terms = { ecs_inout(SystemPosition) } },
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

    ecs_system({
        .name = "ParallelA",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_in(SystemBatchA) } },
        .callback = parallel_system_a,
    });
    ecs_system({
        .name = "ParallelB",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_in(SystemBatchB) } },
        .callback = parallel_system_b,
    });

    ecs_progress();

    test_uint(2, atomic_load(&parallel_entered));
    test_assert(atomic_load(&parallel_stacks[0]) != atomic_load(&parallel_stacks[1]));
    ecs_fini();
}

void system_parallel_worker_context_is_deferred(void) {
    atomic_store(&parallel_context_entered, 0);

    ecs_with_features({ .worker_threads = 1 });

    ECS_COMPONENT_REGISTER(SystemBatchA);
    ECS_COMPONENT_REGISTER(SystemBatchB);

    ecs_entity_t first = ecs_new();
    ecs_entity_t second = ecs_new();

    ecs_set(first, SystemBatchA, { 1 });
    ecs_set(second, SystemBatchB, { 2 });

    ecs_system({
        .name = "ParallelContextA",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_in(SystemBatchA) } },
        .callback = parallel_context_system,
    });

    ecs_system({
        .name = "ParallelContextB",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_in(SystemBatchB) } },
        .callback = parallel_context_system,
    });

    ecs_progress();

    test_uint(2, atomic_load(&parallel_context_entered));

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

    ecs_system({
        .name = "WriteC",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_inout(SystemBatchC) } },
        .callback = parallel_system_a,
    });
    ecs_system({
        .name = "WriteNotC",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_not(SystemBatchC) } },
        .callback = parallel_system_b,
    });
    ecs_progress();
    test_uint(2, atomic_load(&parallel_entered));
    test_assert(atomic_load(&parallel_stacks[0]) != atomic_load(&parallel_stacks[1]));

    ecs_system({
        .name = "WriteSameTableA",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_inout(SystemBatchC) } },
        .callback = same_table_writer_system,
    });
    ecs_system({
        .name = "WriteSameTableB",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_inout(SystemBatchC) } },
        .callback = same_table_writer_system,
    });
    ecs_progress();
    test_uint(0, atomic_load(&same_table_overlap));
    ecs_fini();
    (void)with_c;
    (void)without_c;
}

void system_batches_invalidate_after_table_creation(void) {
    ecs_with_features({ .worker_threads = 1 });
    ECS_COMPONENT_REGISTER(SystemBatchC);
    ecs_system({
        .name = "LateTableA",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_inout(SystemBatchC) } },
        .callback = same_table_writer_system,
    });
    ecs_system({
        .name = "LateTableB",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_inout(SystemBatchC) } },
        .callback = same_table_writer_system,
    });

    ecs_progress();
    ecs_phase_info_t *phase = ecs_system_index_get_phase(EcsOnUpdate);
    test_int(1, phase->batches.size);

    ecs_add(ecs_new(), SystemBatchC);
    ecs_progress();
    test_int(2, phase->batches.size);
    ecs_fini();
}

void system_main_thread_only(void) {
    atomic_store(&main_thread_only_marker, 0);
    ecs_with_features({ .worker_threads = 1 });
    ecs_system({
        .name = "MainThreadOnly",
        .phase = EcsOnUpdate,
        .main_thread_only = true,
        .callback = main_thread_only_system,
    });

    uintptr_t main_marker = (uintptr_t)&system_thread_marker;
    ecs_progress();
    test_uint(main_marker, atomic_load(&main_thread_only_marker));
    ecs_fini();
}

void system_parallel_after_is_a_barrier(void) {
    atomic_store(&after_stage, 0);
    ecs_with_features({ .worker_threads = 1 });

    ecs_system_id_t first = ecs_system({
        .name = "First",
        .phase = EcsOnUpdate,
        .callback = after_first_system,
    });
    ecs_system({
        .name = "Second",
        .phase = EcsOnUpdate,
        .callback = after_second_system,
        .after = { first },
    });

    ecs_progress();
    test_uint(2, atomic_load(&after_stage));
    ecs_fini();
}

void system_resource_access_conflicts(void) {
    atomic_store(&parallel_entered, 0);
    atomic_store(&parallel_stacks[0], 0);
    atomic_store(&parallel_stacks[1], 0);

    ecs_with_features({ .worker_threads = 1 });
    ecs_system({
        .name = "ReadResourceA",
        .phase = EcsOnUpdate,
        .callback = parallel_system_a,
        .read_resources = { ecs_id(DeltaTime) },
    });
    ecs_system({
        .name = "ReadResourceB",
        .phase = EcsOnUpdate,
        .callback = parallel_system_b,
        .read_resources = { ecs_id(DeltaTime) },
    });

    ecs_progress();
    test_uint(2, atomic_load(&parallel_entered));
    test_assert(atomic_load(&parallel_stacks[0]) != atomic_load(&parallel_stacks[1]));
    ecs_fini();

    atomic_store(&resource_stage, 0);
    ecs_with_features({ .worker_threads = 1 });
    ecs_system({
        .name = "WriteResource",
        .phase = EcsOnUpdate,
        .callback = resource_writer_system,
        .write_resources = { ecs_id(DeltaTime) },
    });
    ecs_system({
        .name = "ReadResourceAfterWrite",
        .phase = EcsOnUpdate,
        .callback = resource_reader_system,
        .read_resources = { ecs_id(DeltaTime) },
    });

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

    ecs_system_id_t writer = ecs_system({
        .name = "AddTagAtBarrier",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_in(SystemPosition) } },
        .callback = add_tag_to_global_entity,
    });
    ecs_system({
        .name = "ReadTagAfterBarrier",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_in(SystemTag) } },
        .callback = count_tag_system,
        .after = { writer },
    });

    ecs_progress();
    test_uint(1, system_seen);
    test_assert(ecs_has(system_entity, SystemTag));
    ecs_fini();
}

void system_manual_run_is_synchronous_with_workers(void) {
    atomic_store(&manual_system_calls, 0);
    ecs_with_features({ .worker_threads = 1 });
    ecs_system_id_t system = ecs_system({
        .name = "Manual",
        .phase = EcsOnUpdate,
        .callback = manual_system,
    });

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
    ecs_system({
        .name = "StressA",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_in(SystemBatchA) } },
        .callback = stress_system,
    });
    ecs_system({
        .name = "StressB",
        .phase = EcsOnUpdate,
        .query = { .terms = { ecs_in(SystemBatchB) } },
        .callback = stress_system,
    });
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
            .query = { .terms = { ecs_in(SystemPosition) } },
            .callback = order_render,
        }
    );
    ecs_system(
        {
            .name = "Update",
            .phase = EcsOnUpdate,
            .query = { .terms = { ecs_in(SystemPosition) } },
            .callback = order_update,
        }
    );
    ecs_system(
        {
            .name = "PreUpdate",
            .phase = EcsPreUpdate,
            .query = { .terms = { ecs_in(SystemPosition) } },
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
            .query = { .terms = { ecs_in(SystemPosition) } },
            .callback = order_pre_start,
        }
    );
    ecs_system(
        {
            .name = "Start",
            .phase = EcsStart,
            .query = { .terms = { ecs_in(SystemPosition) } },
            .callback = order_start,
        }
    );
    ecs_system(
        {
            .name = "PostStart",
            .phase = EcsPostStart,
            .query = { .terms = { ecs_in(SystemPosition) } },
            .callback = order_post_start,
        }
    );
    ecs_system(
        {
            .name = "Update",
            .phase = EcsOnUpdate,
            .query = { .terms = { ecs_in(SystemPosition) } },
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
            .query = { .terms = { ecs_in(SystemPosition) } },
            .callback = order_first,
        }
    );
    ecs_system(
        {
            .name = "Second",
            .phase = EcsOnUpdate,
            .query = { .terms = { ecs_in(SystemPosition) } },
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
            .query = { .terms = { ecs_inout(SystemPosition) } },
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
            .query = { .terms = { ecs_in(SystemPosition), ecs_filter(Disabled) } },
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
            .query = { .terms = { ecs_inout(SystemPosition) } },
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
            .query = { .terms = { ecs_inout(SystemPosition), ecs_filter(Disabled) } },
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
            .query = { .terms = { ecs_inout(SystemPosition) } },
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
            .query = { .terms = { ecs_in(SystemPosition) } },
            .callback = add_tag_to_global_entity,
        }
    );
    ecs_system(
        {
            .name = "CountTag",
            .phase = EcsOnUpdate,
            .query = { .terms = { ecs_in(SystemTag) } },
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

    ecs_phase_t physics_phase = ecs_phase({
        .name = "Physics",
        .after = EcsOnUpdate,
        .before = EcsPostUpdate,
    });

    test_assert(physics_phase >= 11);
    test_str("Physics", ecs_phase_name(physics_phase));

    ecs_system({
        .name = "Sys1_OnUpdate",
        .phase = EcsOnUpdate,
        .callback = custom_phase_sys1,
    });

    ecs_system({
        .name = "Sys2_Physics",
        .phase = physics_phase,
        .callback = custom_phase_sys2,
    });

    ecs_system({
        .name = "Sys3_PostUpdate",
        .phase = EcsPostUpdate,
        .callback = custom_phase_sys3,
    });

    ecs_progress();

    test_assert(custom_phase_count == 3);
    test_assert(custom_phase_order[0] == 1);
    test_assert(custom_phase_order[1] == 2);
    test_assert(custom_phase_order[2] == 3);

    ecs_fini();
}
