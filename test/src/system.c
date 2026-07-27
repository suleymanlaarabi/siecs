#include "world_internal.h"
#include <siecs_test.h>
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
    ecs_with(ecs_id(SystemBatchA), ecs_id(SystemBatchB));

    ecs_entity_t entity = ecs_new();
    ecs_defer_begin();
    ecs_set(entity, SystemBatchA, { 42 });
    ecs_defer_end();

    test_assert(ecs_has(entity, SystemBatchA));
    test_assert(ecs_has(entity, SystemBatchB));
    test_int(42, ecs_get(entity, SystemBatchA)->value);
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
