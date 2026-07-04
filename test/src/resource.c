#include <siecs_test.h>

ECS_RESOURCE_DECLARE(ResourceTime, {
    float dt;
    float elapsed;
});
ECS_RESOURCE_DEFINE(ResourceTime);

ECS_COMPONENT_DECLARE(ResourcePosition, { float x; });
ECS_COMPONENT_DEFINE(ResourcePosition);

ECS_RESOURCE_DECLARE(ResourceHooked, { int value; });

static uint32_t resource_on_set_calls;
static uint32_t resource_on_remove_calls;
static int resource_on_set_last;
static int resource_on_remove_last;
static uint32_t resource_lifecycle_copy_ctor_calls;
static uint32_t resource_lifecycle_copy_calls;
static uint32_t resource_lifecycle_move_calls;
static uint32_t resource_lifecycle_dtor_calls;

static void resource_reset_hooks(void) {
    resource_on_set_calls = 0;
    resource_on_remove_calls = 0;
    resource_on_set_last = 0;
    resource_on_remove_last = 0;
}

static void resource_lifecycle_reset(void) {
    resource_lifecycle_copy_ctor_calls = 0;
    resource_lifecycle_copy_calls = 0;
    resource_lifecycle_move_calls = 0;
    resource_lifecycle_dtor_calls = 0;
}

static void resource_lifecycle_copy_ctor(void *dst, const void *src, uint32_t count) {
    int *out = dst;
    const int *in = src;
    for (uint32_t i = 0; i < count; i++) {
        out[i] = in[i];
        resource_lifecycle_copy_ctor_calls++;
    }
}

static void resource_lifecycle_copy(void *dst, const void *src, uint32_t count) {
    int *out = dst;
    const int *in = src;
    for (uint32_t i = 0; i < count; i++) {
        out[i] = in[i];
        resource_lifecycle_copy_calls++;
    }
}

static void resource_lifecycle_move(void *dst, void *src, uint32_t count) {
    int *out = dst;
    int *in = src;
    for (uint32_t i = 0; i < count; i++) {
        out[i] = in[i];
        in[i] = -100;
        resource_lifecycle_move_calls++;
    }
}

static void resource_lifecycle_dtor(void *ptr, uint32_t count) {
    (void)ptr;
    resource_lifecycle_dtor_calls += count;
}

static void resource_hook_on_set(ecs_world_t *world, const void *ptr) {
    (void)world;

    const ResourceHooked *value = ptr;
    resource_on_set_calls++;
    resource_on_set_last = value->value;
}

static void resource_hook_on_remove(ecs_world_t *world, const void *ptr) {
    (void)world;

    const ResourceHooked *value = ptr;
    resource_on_remove_calls++;
    resource_on_remove_last = value->value;
}

ECS_RESOURCE_DEFINE(
    ResourceHooked,
    .on_set = resource_hook_on_set,
    .on_remove = resource_hook_on_remove
);

static uint32_t resource_system_seen;

static void resource_move_system(ecs_iter_t *it) {
    const ResourceTime *time = ecs_get_resource_read(it->world, ResourceTime);
    ResourcePosition *position = ecs_field(it, 0);

    for (uint32_t i = 0; i < it->count; i++) {
        position[i].x += time->dt;
        resource_system_seen++;
    }
}

void resource_set_get(void) {
    ecs_world_t *world = ecs_init();
    ECS_RESOURCE_REGISTER(world, ResourceTime);

    ecs_set_resource(world, ResourceTime, { .dt = 0.016f, .elapsed = 1.0f });

    ResourceTime *time = ecs_get_resource(world, ResourceTime);
    test_assert(time->dt == 0.016f);
    test_assert(time->elapsed == 1.0f);
    test_true(ecs_has_resource(world, ResourceTime));

    time->elapsed = 2.0f;
    test_assert(ecs_get_resource_read(world, ResourceTime)->elapsed == 2.0f);

    ecs_fini(world);
}

void resource_try_get_missing(void) {
    ecs_world_t *world = ecs_init();
    ECS_RESOURCE_REGISTER(world, ResourceTime);

    test_false(ecs_has_resource(world, ResourceTime));
    test_assert(ecs_try_get_resource(world, ResourceTime) == NULL);
    test_assert(ecs_try_get_resource_read(world, ResourceTime) == NULL);

    ecs_fini(world);
}

void resource_remove(void) {
    ecs_world_t *world = ecs_init();
    ECS_RESOURCE_REGISTER(world, ResourceTime);

    ecs_set_resource(world, ResourceTime, { .dt = 1.0f, .elapsed = 2.0f });
    ecs_remove_resource(world, ResourceTime);

    test_false(ecs_has_resource(world, ResourceTime));
    test_assert(ecs_try_get_resource(world, ResourceTime) == NULL);

    ecs_fini(world);
}

void resource_replace(void) {
    ecs_world_t *world = ecs_init();
    ECS_RESOURCE_REGISTER(world, ResourceTime);

    ecs_set_resource(world, ResourceTime, { .dt = 1.0f, .elapsed = 2.0f });
    ecs_set_resource(world, ResourceTime, { .dt = 3.0f, .elapsed = 4.0f });

    const ResourceTime *time = ecs_get_resource_read(world, ResourceTime);
    test_assert(time->dt == 3.0f);
    test_assert(time->elapsed == 4.0f);

    ecs_fini(world);
}

void resource_from_system_c(void) {
    resource_system_seen = 0;

    ecs_world_t *world = ecs_init();
    ECS_RESOURCE_REGISTER(world, ResourceTime);
    ECS_COMPONENT_REGISTER(world, ResourcePosition);
    ecs_set_resource(world, ResourceTime, { .dt = 0.5f, .elapsed = 0.0f });

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, ResourcePosition, { .x = 1.0f });

    ecs_system(
        world,
        {
            .name = "ResourceMove",
            .phase = EcsOnUpdate,
            .query = { .terms = { ecs_inout(ResourcePosition) } },
            .callback = resource_move_system,
        }
    );

    ecs_run_phase(world, EcsOnUpdate);

    test_int(1, resource_system_seen);
    test_assert(ecs_get(world, entity, ResourcePosition)->x == 1.5f);

    ecs_fini(world);
}

void resource_hooks(void) {
    resource_reset_hooks();

    ecs_world_t *world = ecs_init();
    ECS_RESOURCE_REGISTER(world, ResourceHooked);

    ecs_set_resource(world, ResourceHooked, { .value = 12 });
    test_int(1, resource_on_set_calls);
    test_int(12, resource_on_set_last);

    ecs_set_resource(world, ResourceHooked, { .value = 24 });
    test_int(2, resource_on_set_calls);
    test_int(24, resource_on_set_last);
    test_int(0, resource_on_remove_calls);

    ecs_remove_resource(world, ResourceHooked);
    test_int(1, resource_on_remove_calls);
    test_int(24, resource_on_remove_last);

    ecs_fini(world);
}

void resource_lifecycle_ops_are_used_for_set_move_and_remove(void) {
    resource_lifecycle_reset();

    ecs_world_t *world = ecs_init();
    ecs_resource_t id = ecs_resource_init(
        world,
        &(ecs_resource_desc_t){
            .name = "LifecycleResource",
            .size = sizeof(int),
            .ops = {
                .dtor = resource_lifecycle_dtor,
                .copy_ctor = resource_lifecycle_copy_ctor,
                .copy = resource_lifecycle_copy,
                .move = resource_lifecycle_move,
            },
        }
    );

    int value = 1;
    ecs_set_resource_rid(world, id, &value);
    test_int(1, resource_lifecycle_copy_ctor_calls);
    test_int(1, *(int *)ecs_resource_rid(world, id));

    value = 2;
    ecs_set_resource_rid(world, id, &value);
    test_int(1, resource_lifecycle_copy_calls);
    test_int(2, *(int *)ecs_resource_rid(world, id));

    value = 3;
    ecs_move_resource_rid(world, id, &value);
    test_int(1, resource_lifecycle_move_calls);
    test_int(-100, value);
    test_int(3, *(int *)ecs_resource_rid(world, id));

    ecs_remove_resource_rid(world, id);
    test_int(1, resource_lifecycle_dtor_calls);

    ecs_fini(world);
}
