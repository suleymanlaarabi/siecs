#include <siecs_test.h>

ECS_COMPONENT_DECLARE(SystemPosition, { int value; });

ECS_COMPONENT_DEFINE(SystemPosition);

static uint32_t system_calls;
static uint32_t system_seen;
static uint32_t system_order_count;
static int system_order[8];

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

static void order_update(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 2;
}

static void order_render(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 3;
}

static void order_first(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 10;
}

static void order_second(ecs_iter_t *it) {
    (void)it;
    system_order[system_order_count++] = 20;
}

static ecs_entity_t create_system_entity(ecs_world_t *world, int value) {
    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, SystemPosition, { value });
    return entity;
}

void system_run(void) {
    reset_system_test_state();

    ecs_world_t *world = ecs_init();
    test_not_null(world);

    ECS_COMPONENT_REGISTER(world, SystemPosition);
    ecs_entity_t entity = create_system_entity(world, 41);

    ecs_system(
        world,
        {
            .name = "Count",
            .phase = EcsOnUpdate,
            .query = { .read = { ecs_id(SystemPosition) } },
            .callback = count_system,
        }
    );

    ecs_progress(world);

    SystemPosition *p = ecs_get(world, entity, SystemPosition);
    test_assert(system_calls == 1);
    test_assert(system_seen == 1);
    test_assert(p->value == 42);

    ecs_fini(world);
}

void system_phase_order(void) {
    reset_system_test_state();

    ecs_world_t *world = ecs_init();
    test_not_null(world);

    ECS_COMPONENT_REGISTER(world, SystemPosition);
    create_system_entity(world, 0);

    ecs_system(
        world,
        {
            .name = "Render",
            .phase = EcsOnRender,
            .query = { .read = { ecs_id(SystemPosition) } },
            .callback = order_render,
        }
    );
    ecs_system(
        world,
        {
            .name = "Update",
            .phase = EcsOnUpdate,
            .query = { .read = { ecs_id(SystemPosition) } },
            .callback = order_update,
        }
    );
    ecs_system(
        world,
        {
            .name = "PreUpdate",
            .phase = EcsPreUpdate,
            .query = { .read = { ecs_id(SystemPosition) } },
            .callback = order_pre_update,
        }
    );

    ecs_progress(world);

    test_assert(system_order_count == 3);
    test_assert(system_order[0] == 1);
    test_assert(system_order[1] == 2);
    test_assert(system_order[2] == 3);

    ecs_fini(world);
}

void system_after_order(void) {
    reset_system_test_state();

    ecs_world_t *world = ecs_init();
    test_not_null(world);

    ECS_COMPONENT_REGISTER(world, SystemPosition);
    create_system_entity(world, 0);

    ecs_system_id_t first = ecs_system(
        world,
        {
            .name = "First",
            .phase = EcsOnUpdate,
            .query = { .read = { ecs_id(SystemPosition) } },
            .callback = order_first,
        }
    );
    ecs_system(
        world,
        {
            .name = "Second",
            .phase = EcsOnUpdate,
            .query = { .read = { ecs_id(SystemPosition) } },
            .callback = order_second,
            .after = { first },
        }
    );

    ecs_run_phase(world, EcsOnUpdate);

    test_assert(system_order_count == 2);
    test_assert(system_order[0] == 10);
    test_assert(system_order[1] == 20);

    ecs_fini(world);
}

void system_enable(void) {
    reset_system_test_state();

    ecs_world_t *world = ecs_init();
    test_not_null(world);

    ECS_COMPONENT_REGISTER(world, SystemPosition);
    create_system_entity(world, 0);

    ecs_system_id_t system = ecs_system(
        world,
        {
            .name = "Disabled",
            .phase = EcsOnUpdate,
            .query = { .read = { ecs_id(SystemPosition) } },
            .callback = count_system,
            .disabled = true,
        }
    );

    ecs_progress(world);
    test_assert(system_calls == 0);

    ecs_enable_system(world, system, true);
    ecs_progress(world);
    test_assert(system_calls == 1);

    ecs_enable_system(world, system, false);
    ecs_progress(world);
    test_assert(system_calls == 1);

    ecs_fini(world);
}
