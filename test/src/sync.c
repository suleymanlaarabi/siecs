#include <test.h>

static ecs_entity_t sync_entity;
static ecs_component_t sync_component;
static int sync_calls;
static int sync_value;

static void synchronized(void *context) {
    test_true(context == &sync_calls);
    const int *value = ecs_get_cid(sync_entity, sync_component);
    if (value)
        sync_value = *value;
    sync_calls++;
}

static void setup(void) {
    ecs_init();
    sync_component =
        ecs_component_init(&(ecs_component_desc_t){ .name = "SyncValue", .size = sizeof(int) });
    sync_entity = ecs_new();
    sync_calls = sync_value = 0;
    ecs_sync_register(synchronized, &sync_calls);
}

void sync_immediate_and_grouped(void) {
    setup();
    int value = 10;
    ecs_set_cid(sync_entity, sync_component, &value);
    test_int(sync_calls, 1);
    test_int(sync_value, 10);
    ecs_sync_begin();
    value = 20;
    ecs_set_cid(sync_entity, sync_component, &value);
    value = 30;
    ecs_set_cid(sync_entity, sync_component, &value);
    test_int(sync_calls, 1);
    ecs_sync_end();
    test_int(sync_calls, 2);
    test_int(sync_value, 30);
    ecs_fini();
}

void sync_deferred_boundary(void) {
    setup();
    ecs_defer_begin();
    int value = 7;
    ecs_set_cid(sync_entity, sync_component, &value);
    test_int(sync_calls, 0);
    ecs_defer_begin();
    value = 9;
    ecs_set_cid(sync_entity, sync_component, &value);
    ecs_defer_end();
    test_int(sync_calls, 0);
    ecs_defer_end();
    test_int(sync_calls, 1);
    test_int(sync_value, 9);
    ecs_fini();
}

static void reentrant_sync(void *context) {
    int *calls = context;
    (*calls)++;
    test_int(*calls, 1);
    int value = 42;
    ecs_set_cid(sync_entity, sync_component, &value);
}

void sync_reentrant_and_stale_handles(void) {
    ecs_init();
    sync_component =
        ecs_component_init(&(ecs_component_desc_t){ .name = "SyncValue", .size = sizeof(int) });
    sync_entity = ecs_new();
    int calls = 0;
    ecs_sync_id_t old = ecs_sync_register(reentrant_sync, &calls);
    ecs_sync();
    test_int(calls, 1);
    test_int(*(const int *)ecs_get_cid(sync_entity, sync_component), 42);
    ecs_sync_unregister(old);
    ecs_sync();
    test_int(calls, 1);
    calls = 0;
    ecs_sync_id_t current = ecs_sync_register(reentrant_sync, &calls);
    ecs_sync_unregister(old);
    ecs_sync();
    test_int(calls, 1);
    ecs_sync_unregister(current);
    ecs_fini();
    ecs_init();
    ecs_sync_unregister(old);
    ecs_sync();
    ecs_fini();
}

static ecs_entity_t batch_expected_entity;
static ecs_component_t batch_component;
static int batch_callbacks;
static int batch_destructors;

static void batched_observer(ecs_observer_event_t *event) {
    if (event->component != batch_component)
        return;
    test_uint(event->entity, batch_expected_entity);
    test_int(*(const int *)event->trigger_data, 42);
    batch_callbacks++;
}

static void batched_destructor(uintptr_t data) {
    test_uint(data, 7);
    batch_destructors++;
}

void sync_batched_observer_generation_reuse(void) {
    ecs_init();
    batch_component =
        ecs_component_init(&(ecs_component_desc_t){ .name = "BatchValue", .size = sizeof(int) });
    ecs_entity_t old = ecs_new();
    int value = 42;
    ecs_set_cid(old, batch_component, &value);
    batch_callbacks = batch_destructors = 0;
    ecs_observer_init(&(ecs_observer_desc_t){ .on = EcsOnRemove,
                                              .entity = old,
                                              .query.components = { { batch_component, EcsIn } },
                                              .callback = batched_observer,
                                              .user_data = 7,
                                              .user_data_dtor = batched_destructor });
    ecs_observer_batch_begin();
    ecs_kill(old);
    ecs_entity_t fresh = ecs_new();
    test_uint(ecs_entity_id(fresh), ecs_entity_id(old));
    ecs_observer_id_t observer = ecs_observer_init(&(ecs_observer_desc_t){
        .on = EcsOnSet,
        .entity = fresh,
        .query.components = { { batch_component, EcsInOptional } },
        .callback = batched_observer });
    ecs_set_cid(fresh, batch_component, &value);
    test_int(batch_callbacks, 0);
    /* The old event retains its original handle and is delivered first. */
    batch_expected_entity = old;
    ecs_observer_fini(observer);
    ecs_observer_batch_end();
    test_int(batch_callbacks, 1);
    test_int(batch_destructors, 1);
    batch_expected_entity = fresh;
    observer = ecs_observer_init(&(ecs_observer_desc_t){
        .on = EcsOnSet,
        .entity = fresh,
        .query.components = { { batch_component, EcsInOptional } },
        .callback = batched_observer });
    ecs_observer_batch_begin();
    ecs_set_cid(fresh, batch_component, &value);
    ecs_observer_batch_end();
    test_int(batch_callbacks, 2);
    ecs_observer_fini(observer);
    ecs_fini();
}

static int snapshot_calls;
static ecs_entity_t snapshot_entity;
static void reflected_snapshot(ecs_observer_event_t *event) {
    if (event->component != batch_component)
        return;
    const char *const *snapshot = event->trigger_data;
    const char *const *current = ecs_get_cid(snapshot_entity, batch_component);
    test_uint(event->entity, snapshot_entity);
    test_str(*snapshot, snapshot_calls ? "second" : "first");
    test_str(*current, "second");
    snapshot_calls++;
}

void sync_batched_owning_snapshots(void) {
    ecs_init();
    batch_component =
        ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){ .name = "OwnedLabel",
                                                                    .fields = "{ char *value; }" });
    snapshot_entity = ecs_new();
    snapshot_calls = 0;
    ecs_observer_init(&(ecs_observer_desc_t){
        .on = EcsOnSet,
        .entity = snapshot_entity,
        .query.components = { { batch_component, EcsInOptional } },
        .callback = reflected_snapshot });
    ecs_observer_batch_begin();
    const char *value = "first";
    ecs_set_cid(snapshot_entity, batch_component, &value);
    value = "second";
    ecs_set_cid(snapshot_entity, batch_component, &value);
    test_int(snapshot_calls, 0);
    ecs_observer_batch_end();
    test_int(snapshot_calls, 2);
    ecs_fini();
}
