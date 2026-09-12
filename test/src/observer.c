#include "siecs.h"
#include <siecs_test.h>

ECS_COMPONENT_DECLARE(ObserverValue, { int value; });
ECS_COMPONENT_DEFINE(ObserverValue);

ECS_COMPONENT_DECLARE(ObserverPosition, {
    int x;
    int y;
});
ECS_COMPONENT_DEFINE(ObserverPosition);

static uint32_t observer_calls;
static int observer_last_value;
static ecs_entity_t observer_last_entity;
static uint32_t observer_tag_calls;

typedef struct {
    uint32_t calls;
    ecs_component_t component;
} ObserverComponentState;

static void observer_capture_component(ecs_observer_event_t *event) {
    ObserverComponentState *state = (ObserverComponentState *)event->user_data;
    state->calls++;
    state->component = event->component;
}

static void observer_count_event(ecs_observer_event_t *event) {
    uint32_t *calls = (uint32_t *)event->user_data;
    (*calls)++;
}

void observer_global_registration_does_not_duplicate_queries(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverValue);
    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ObserverValue, { 1 });
    ecs_query_id_t q = ecs_query({ .components = { ecs_in(ObserverValue) } });
    ecs_event_t event = ecs_event();
    uint32_t filtered = 0, first_global = 0, second_global = 0;
    ecs_observer({ .on = event, .query = { .components = { ecs_filter(ObserverValue) } },
        .callback = observer_count_event, .user_data = (uintptr_t)&filtered });
    ecs_observer({ .on = event, .callback = observer_count_event,
        .user_data = (uintptr_t)&first_global });
    ecs_observer({ .on = event, .callback = observer_count_event,
        .user_data = (uintptr_t)&second_global });
    test_uint(1, ecs_query_count(q));
    ecs_observer_trigger(entity, event, NULL);
    test_uint(1, filtered);
    test_uint(1, first_global);
    test_uint(1, second_global);
    ecs_component_t tag = ecs_component({ 0 });
    ecs_add_cid(entity, tag);
    test_uint(1, ecs_query_count(q));
    ecs_observer_trigger(entity, event, NULL);
    test_uint(2, filtered);
    test_uint(2, first_global);
    test_uint(2, second_global);
    ecs_query_fini(q);
    ecs_fini();
}

static void reset_observer_state(void) {
    observer_calls = 0;
    observer_last_value = 0;
    observer_last_entity = 0;
    observer_tag_calls = 0;
}

static void on_observer_tag_set(ecs_observer_event_t *event) {
    test_null((void *)event->trigger_data);
    observer_tag_calls++;
}

static void on_observer_value_set(ecs_observer_event_t *event) {
    const ObserverValue *value = event->trigger_data;

    observer_calls++;
    observer_last_value = value->value;
    observer_last_entity = event->entity;
}

static void on_observer_value_remove(ecs_observer_event_t *event) {
    const ObserverValue *value = event->trigger_data;

    observer_calls++;
    observer_last_value = value->value;
    observer_last_entity = event->entity;
}

void observer_enable(void) {
    reset_observer_state();

    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverValue);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ObserverValue, { 1 });

    ecs_observer_id_t observer = ecs_observer(
        {
            .on = EcsOnSet,
            .query = { .components = { ecs_in(ObserverValue) } },
            .callback = on_observer_value_set,
        }
    );

    ecs_set(entity, ObserverValue, { 2 });
    test_int(1, observer_calls);
    test_int(2, observer_last_value);

    ecs_observer_disable(observer);
    ecs_set(entity, ObserverValue, { 3 });
    test_int(1, observer_calls);
    test_int(2, observer_last_value);

    ecs_observer_enable(observer);
    ecs_set(entity, ObserverValue, { 4 });
    test_int(2, observer_calls);
    test_int(4, observer_last_value);

    ecs_fini();
}

void observer_skips_disabled_by_default(void) {
    reset_observer_state();

    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverValue);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ObserverValue, { 1 });
    ecs_add(entity, Disabled);

    ecs_observer(
        {
            .on = EcsOnSet,
            .query = { .components = { ecs_in(ObserverValue) } },
            .callback = on_observer_value_set,
        }
    );

    ecs_set(entity, ObserverValue, { 2 });
    test_int(0, observer_calls);
    test_int(0, observer_last_value);

    ecs_fini();
}

void observer_can_match_disabled_when_requested(void) {
    reset_observer_state();

    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverValue);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ObserverValue, { 1 });
    ecs_add(entity, Disabled);

    ecs_observer(
        {
            .on = EcsOnSet,
            .query = { .components = { ecs_in(ObserverValue), ecs_filter(Disabled) } },
            .callback = on_observer_value_set,
        }
    );

    ecs_set(entity, ObserverValue, { 2 });
    test_int(1, observer_calls);
    test_int(2, observer_last_value);

    ecs_fini();
}

void observer_on_remove_runs_when_entity_is_killed(void) {
    reset_observer_state();

    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverValue);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ObserverValue, { 7 });

    ecs_observer(
        {
            .on = EcsOnRemove,
            .query = { .components = { ecs_in(ObserverValue) } },
            .callback = on_observer_value_remove,
        }
    );

    ecs_kill(entity);

    test_int(1, observer_calls);
    test_int(7, observer_last_value);
    test_assert(observer_last_entity == entity);

    ecs_fini();
}

void observer_modified_emits_current_component_value(void) {
    reset_observer_state();

    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverValue);

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ObserverValue, { 1 });

    ecs_observer(
        {
            .on = EcsOnSet,
            .query = { .components = { ecs_in(ObserverValue) } },
            .callback = on_observer_value_set,
        }
    );

    ObserverValue *value = ecs_get(entity, ObserverValue);
    value->value = 42;
    ecs_modified(entity, ObserverValue);

    test_int(1, observer_calls);
    test_int(42, observer_last_value);
    test_assert(observer_last_entity == entity);
    test_int(42, ecs_get(entity, ObserverValue)->value);

    ecs_fini();
}

void observer_modified_supports_zero_sized_tags(void) {
    reset_observer_state();

    ecs_init();

    ecs_entity_t entity = ecs_new();
    ecs_add(entity, Disabled);
    ecs_observer(
        {
            .on = EcsOnSet,
            .query = { .components = { ecs_filter(Disabled) } },
            .callback = on_observer_tag_set,
        }
    );

    ecs_modified(entity, Disabled);
    test_int(1, observer_tag_calls);

    ecs_fini();
}

void observer_on_set_reports_component(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverPosition);
    ecs_entity_t entity = ecs_new();
    ObserverComponentState state = { 0 };
    ecs_observer({
        .on = EcsOnSet,
        .query.components = { ecs_filter(ObserverPosition) },
        .callback = observer_capture_component,
        .user_data = (uintptr_t)&state,
    });

    ecs_set(entity, ObserverPosition, { 10, 20 });

    test_uint(1, state.calls);
    test_uint(ecs_id(ObserverPosition), state.component);
    ecs_fini();
}

void observer_on_add_reports_component(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverPosition);
    ecs_entity_t entity = ecs_new();
    ObserverComponentState state = { 0 };
    ecs_observer({
        .on = EcsOnAdd,
        .query.components = { ecs_filter(ObserverPosition) },
        .callback = observer_capture_component,
        .user_data = (uintptr_t)&state,
    });

    ecs_add(entity, ObserverPosition);

    test_uint(1, state.calls);
    test_uint(ecs_id(ObserverPosition), state.component);
    ecs_fini();
}

void observer_on_remove_reports_component(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverPosition);
    ecs_entity_t entity = ecs_new();
    ecs_add(entity, ObserverPosition);
    ObserverComponentState state = { 0 };
    ecs_observer({
        .on = EcsOnRemove,
        .query.components = { ecs_filter(ObserverPosition) },
        .callback = observer_capture_component,
        .user_data = (uintptr_t)&state,
    });

    ecs_remove(entity, ObserverPosition);

    test_uint(1, state.calls);
    test_uint(ecs_id(ObserverPosition), state.component);
    ecs_fini();
}

void observer_custom_event_reports_zero_component(void) {
    ecs_init();
    ecs_entity_t entity = ecs_new();
    ecs_event_t event = ecs_event();
    int data = 42;
    ObserverComponentState state = { 0 };
    ecs_observer({
        .on = event,
        .callback = observer_capture_component,
        .user_data = (uintptr_t)&state,
    });

    ecs_observer_trigger(entity, event, &data);

    test_uint(1, state.calls);
    test_uint(0, state.component);
    ecs_fini();
}

void observer_deferred_on_set_reports_component_at_flush(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverPosition);
    ecs_entity_t entity = ecs_new();
    ObserverComponentState state = { 0 };
    ecs_observer({
        .on = EcsOnSet,
        .query.components = { ecs_filter(ObserverPosition) },
        .callback = observer_capture_component,
        .user_data = (uintptr_t)&state,
    });

    ecs_defer_begin();
    ecs_set(entity, ObserverPosition, { 10, 20 });
    test_uint(0, state.calls);
    ecs_defer_end();

    test_uint(1, state.calls);
    test_uint(ecs_id(ObserverPosition), state.component);
    ecs_fini();
}
