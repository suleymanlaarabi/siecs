#include <siecs_test.h>
#include <string.h>

ECS_RESOURCE_DECLARE(ResourceTime, {
    float dt;
    float elapsed;
});
ECS_RESOURCE_DEFINE(ResourceTime);

ECS_COMPONENT_DECLARE(ResourcePosition, { float x; });
ECS_COMPONENT_DEFINE(ResourcePosition);

ECS_RESOURCE_DECLARE(ResourceHooked, { int value; });

ECS_RESOURCE_DECLARE(ResourceTeardown, { int value; });
ECS_COMPONENT_DECLARE(ResourceTeardownUser, { int value; });

static uint32_t resource_on_set_calls;
static uint32_t resource_on_remove_calls;
static int resource_on_set_last;
static int resource_on_remove_last;
static uint32_t resource_lifecycle_copy_ctor_calls;
static uint32_t resource_lifecycle_copy_calls;
static uint32_t resource_lifecycle_move_calls;
static uint32_t resource_lifecycle_dtor_calls;
static uint32_t resource_teardown_order;
static bool resource_component_saw_resource;
static int resource_reverse_order[4];
static uint32_t resource_reverse_count;

static void resource_teardown_on_remove(const void *ptr) {
    const ResourceTeardown *value = ptr;
    resource_teardown_order = 2;
    test_assert(value->value == 42);
}

static void resource_component_on_remove(
    ecs_entity_t entity,
    ecs_component_t component,
    void *ptr
) {
    (void)entity;
    (void)component;
    const ResourceTeardown *resource = ecs_get_resource_read(ResourceTeardown);
    const ResourceTeardownUser *value = ptr;
    resource_component_saw_resource = resource && resource->value == value->value;
    resource_teardown_order = 1;
}

static void resource_reverse_on_remove(const void *ptr) {
    const int *value = ptr;
    resource_reverse_order[resource_reverse_count++] = *value;
}

ECS_RESOURCE_DEFINE(ResourceTeardown, .on_remove = resource_teardown_on_remove);
ECS_COMPONENT_DEFINE(ResourceTeardownUser, .on_remove = resource_component_on_remove);

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

static void resource_hook_on_set(const void *ptr) {

    const ResourceHooked *value = ptr;
    resource_on_set_calls++;
    resource_on_set_last = value->value;
}

static void resource_hook_on_remove(const void *ptr) {

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
static float resource_delta_time_seen;

static void resource_delta_time_system(ecs_iter_t *it) {
    resource_delta_time_seen = ecs_get_resource_read(DeltaTime)->value;
    test_assert(resource_delta_time_seen == it->delta_time);
}

static void resource_move_system(ecs_iter_t *it) {
    const ResourceTime *time = ecs_get_resource_read(ResourceTime);
    ResourcePosition *position = ecs_field(it, 0);

    for (uint32_t i = 0; i < it->count; i++) {
        position[i].x += time->dt;
        resource_system_seen++;
    }
}

void resource_set_get(void) {
    ecs_init();
    ECS_RESOURCE_REGISTER(ResourceTime);

    ecs_set_resource(ResourceTime, { .dt = 0.016f, .elapsed = 1.0f });

    ResourceTime *time = ecs_get_resource(ResourceTime);
    test_assert(time->dt == 0.016f);
    test_assert(time->elapsed == 1.0f);
    test_true(ecs_has_resource(ResourceTime));

    time->elapsed = 2.0f;
    test_assert(ecs_get_resource_read(ResourceTime)->elapsed == 2.0f);

    ecs_fini();
}

void resource_delta_time(void) {
    ecs_init();

    test_true(ecs_has_resource(DeltaTime));
    test_assert(ecs_get_resource_read(DeltaTime)->value == 0.0f);

    ecs_system({
        .name = "DeltaTimeResource",
        .phase = EcsOnUpdate,
        .callback = resource_delta_time_system,
    });
    ecs_progress();

    test_assert(resource_delta_time_seen >= 0.0f);
    test_str("DeltaTime", ecs_resource_name(ecs_id(DeltaTime)));

    ecs_fini();
}

void resource_name_lookup(void) {
    ecs_init();
    ECS_RESOURCE_REGISTER(ResourceTime);

    test_str("ResourceTime", ecs_resource_name(ecs_id(ResourceTime)));
    test_int(ecs_id(ResourceTime), ecs_resource_find("ResourceTime"));
    test_int(0, ecs_resource_find("MissingResource"));

    ecs_fini();
}

void resource_try_get_missing(void) {
    ecs_init();
    ECS_RESOURCE_REGISTER(ResourceTime);

    test_false(ecs_has_resource(ResourceTime));
    test_assert(ecs_try_get_resource(ResourceTime) == NULL);
    test_assert(ecs_try_get_resource_read(ResourceTime) == NULL);

    ecs_fini();
}

void resource_remove(void) {
    ecs_init();
    ECS_RESOURCE_REGISTER(ResourceTime);

    ecs_set_resource(ResourceTime, { .dt = 1.0f, .elapsed = 2.0f });
    ecs_remove_resource(ResourceTime);

    test_false(ecs_has_resource(ResourceTime));
    test_assert(ecs_try_get_resource(ResourceTime) == NULL);

    ecs_fini();
}

void resource_replace(void) {
    ecs_init();
    ECS_RESOURCE_REGISTER(ResourceTime);

    ecs_set_resource(ResourceTime, { .dt = 1.0f, .elapsed = 2.0f });
    ecs_set_resource(ResourceTime, { .dt = 3.0f, .elapsed = 4.0f });

    const ResourceTime *time = ecs_get_resource_read(ResourceTime);
    test_assert(time->dt == 3.0f);
    test_assert(time->elapsed == 4.0f);

    ecs_fini();
}

void resource_from_system_c(void) {
    resource_system_seen = 0;

    ecs_init();
    ECS_RESOURCE_REGISTER(ResourceTime);
    ECS_COMPONENT_REGISTER(ResourcePosition);
    ecs_set_resource(ResourceTime, { .dt = 0.5f, .elapsed = 0.0f });

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ResourcePosition, { .x = 1.0f });

    ecs_system(
        {
            .name = "ResourceMove",
            .phase = EcsOnUpdate,
            .query = { .terms = { ecs_inout(ResourcePosition) } },
            .callback = resource_move_system,
        }
    );

    ecs_run_phase(EcsOnUpdate);

    test_int(1, resource_system_seen);
    test_assert(ecs_get(entity, ResourcePosition)->x == 1.5f);

    ecs_fini();
}

void resource_hooks(void) {
    resource_reset_hooks();

    ecs_init();
    ECS_RESOURCE_REGISTER(ResourceHooked);

    ecs_set_resource(ResourceHooked, { .value = 12 });
    test_int(1, resource_on_set_calls);
    test_int(12, resource_on_set_last);

    ecs_set_resource(ResourceHooked, { .value = 24 });
    test_int(2, resource_on_set_calls);
    test_int(24, resource_on_set_last);
    test_int(0, resource_on_remove_calls);

    ecs_remove_resource(ResourceHooked);
    test_int(1, resource_on_remove_calls);
    test_int(24, resource_on_remove_last);

    ecs_fini();
}

void resource_lifecycle_ops_are_used_for_set_move_and_remove(void) {
    resource_lifecycle_reset();

    ecs_init();
    ecs_resource_t id = ecs_resource_init(
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
    ecs_set_resource_rid(id, &value);
    test_int(1, resource_lifecycle_copy_ctor_calls);
    test_int(1, *(int *)ecs_resource_rid(id));

    value = 2;
    ecs_set_resource_rid(id, &value);
    test_int(1, resource_lifecycle_copy_calls);
    test_int(2, *(int *)ecs_resource_rid(id));

    value = 3;
    ecs_move_resource_rid(id, &value);
    test_int(1, resource_lifecycle_move_calls);
    test_int(-100, value);
    test_int(3, *(int *)ecs_resource_rid(id));

    ecs_remove_resource_rid(id);
    test_int(1, resource_lifecycle_dtor_calls);

    ecs_fini();
}

void resource_fini_runs_after_component_remove(void) {
    resource_teardown_order = 0;
    resource_component_saw_resource = false;

    ecs_init();
    ECS_RESOURCE_REGISTER(ResourceTeardown);
    ECS_COMPONENT_REGISTER(ResourceTeardownUser);
    ecs_set_resource(ResourceTeardown, { .value = 42 });

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ResourceTeardownUser, { .value = 42 });

    ecs_fini();

    test_true(resource_component_saw_resource);
    test_int(2, resource_teardown_order);
}

void resource_fini_uses_reverse_registration_order(void) {
    memset(resource_reverse_order, 0, sizeof(resource_reverse_order));
    resource_reverse_count = 0;

    ecs_init();
    ecs_resource_t a = ecs_resource_init(&(ecs_resource_desc_t){
        .name = "ReverseA",
        .size = sizeof(int),
        .on_remove = resource_reverse_on_remove,
    });
    ecs_resource_t b = ecs_resource_init(&(ecs_resource_desc_t){
        .name = "ReverseB",
        .size = sizeof(int),
        .on_remove = resource_reverse_on_remove,
    });
    ecs_resource_t c = ecs_resource_init(&(ecs_resource_desc_t){
        .name = "ReverseC",
        .size = sizeof(int),
        .on_remove = resource_reverse_on_remove,
    });
    ecs_resource_t absent = ecs_resource_init(&(ecs_resource_desc_t){
        .name = "ReverseAbsent",
        .size = sizeof(int),
        .on_remove = resource_reverse_on_remove,
    });

    int value = 1;
    ecs_set_resource_rid(a, &value);
    value = 2;
    ecs_set_resource_rid(b, &value);
    value = 3;
    ecs_set_resource_rid(c, &value);
    (void)absent;

    ecs_fini();

    test_int(3, resource_reverse_order[0]);
    test_int(2, resource_reverse_order[1]);
    test_int(1, resource_reverse_order[2]);
    test_int(0, resource_reverse_order[3]);
    test_int(3, resource_reverse_count);
}
