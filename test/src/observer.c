#include "siecs.h"
#include "storage/observer_index.h"
#include "storage/query_index.h"
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

static void observer_ignore_event(ecs_observer_event_t *event) { (void)event; }

void observer_target_exact_entity(void) {
    ecs_init();
    ecs_entity_t target = ecs_new(), other = ecs_new();
    ecs_event_t event = ecs_event();
    uint32_t calls = 0;
    ecs_observer(
        { .on = event,
          .entity = target,
          .callback = observer_count_event,
          .user_data = (uintptr_t)&calls }
    );
    ecs_observer_trigger(other, event, NULL);
    test_uint(0, calls);
    ecs_observer_trigger(target, event, NULL);
    test_uint(1, calls);
    ecs_fini();
}

void observer_target_no_query(void) {
    ecs_init();
    ecs_entity_t target = ecs_new();
    uint32_t active = query_index.active_ids.size;
    ecs_observer_id_t id =
        ecs_observer({ .on = ecs_event(), .entity = target, .callback = observer_count_event });
    ecs_observer_t *observer = sicore_vec_get_mut(&observer_index.observers, id, ecs_observer_t);
    test_uint(ECS_OBSERVER_NO_QUERY, observer->query);
    test_uint(active, query_index.active_ids.size);
    ecs_fini();
}

void observer_target_filtered(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverPosition);
    ecs_entity_t target = ecs_new();
    ecs_event_t event = ecs_event();
    uint32_t calls = 0;
    ecs_observer(
        { .on = event,
          .entity = target,
          .query = { .components = { ecs_filter(ObserverPosition) } },
          .callback = observer_count_event,
          .user_data = (uintptr_t)&calls }
    );
    ecs_observer_trigger(target, event, NULL);
    test_uint(0, calls);
    ecs_set(target, ObserverPosition, { 1, 2 });
    ecs_observer_trigger(target, event, NULL);
    test_uint(1, calls);
    ecs_add_cid(target, ecs_component({ 0 }));
    ecs_observer_trigger(target, event, NULL);
    test_uint(2, calls);
    ecs_fini();
}

void observer_target_destroy(void) {
    ecs_init();
    ecs_entity_t target = ecs_new();
    ecs_event_t event = ecs_event();
    uint32_t calls = 0;
    ecs_observer_id_t id = ecs_observer(
        { .on = event,
          .entity = target,
          .callback = observer_count_event,
          .user_data = (uintptr_t)&calls }
    );
    ecs_observer_fini(id);
    ecs_observer_trigger(target, event, NULL);
    test_uint(0, calls);
    test_uint(
        id,
        ecs_observer({ .on = event, .entity = target, .callback = observer_count_event })
    );
    ecs_fini();
}

void observer_target_entity_kill(void) {
    ecs_init();
    ecs_entity_t target = ecs_new(), other = ecs_new();
    ecs_event_t event_a = ecs_event(), event_b = ecs_event();
    ecs_observer({ .on = event_a, .entity = target, .callback = observer_count_event });
    ecs_observer({ .on = event_b, .entity = target, .callback = observer_count_event });
    ecs_observer({ .on = event_a, .entity = other, .callback = observer_count_event });
    ecs_kill(target);
    test_uint(1, observer_index.target_keys.size);
    test_uint(1, observer_index.target_observers.size);
    test_uint(
        ecs_observer_target_key(ecs_entity_id(other), event_a),
        *(uint64_t *)observer_index.target_keys.data
    );
    ecs_fini();
}

void observer_target_index_reuse(void) {
    ecs_init();
    ecs_entity_t target = ecs_new();
    ecs_event_t event = ecs_event();
    uint32_t calls = 0;
    ecs_observer(
        { .on = event,
          .entity = target,
          .callback = observer_count_event,
          .user_data = (uintptr_t)&calls }
    );
    uint32_t index = ecs_entity_id(target);
    ecs_kill(target);
    ecs_entity_t reused = ecs_new();
    test_uint(index, ecs_entity_id(reused));
    ecs_observer_trigger(reused, event, NULL);
    test_uint(0, calls);
    ecs_fini();
}

void observer_target_on_remove(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverValue);
    ecs_entity_t target = ecs_new();
    ecs_set(target, ObserverValue, { 1 });
    uint32_t calls = 0;
    ecs_observer(
        { .on = EcsOnRemove,
          .entity = target,
          .callback = observer_count_event,
          .user_data = (uintptr_t)&calls }
    );
    ecs_kill(target);
    test_uint(1, calls);
    test_uint(0, observer_index.target_keys.size);
    ecs_fini();
}

void observer_target_many(void) {
    enum { observer_count = 65536, entity_count = 256 };
    ecs_init();
    ecs_event_t event = ecs_event();
    ecs_entity_t entities[entity_count];
    for (uint32_t i = 0; i < entity_count; i++)
        entities[i] = ecs_new();
    for (uint32_t i = 0; i < observer_count; i++) {
        ecs_observer(
            { .on = event, .entity = entities[i / 256], .callback = observer_ignore_event }
        );
    }
    uint32_t calls = 0;
    ecs_observer_id_t global = ecs_observer(
        { .on = event, .callback = observer_count_event, .user_data = (uintptr_t)&calls }
    );
    test_uint(observer_count, global);
    ecs_observer_trigger(entities[0], event, NULL);
    test_uint(1, calls);
    ecs_fini();
}

void observer_global_registration_does_not_duplicate_queries(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(ObserverValue);
    ecs_entity_t entity = ecs_new();
    ecs_set(entity, ObserverValue, { 1 });
    ecs_query_id_t q = ecs_query({ .components = { ecs_in(ObserverValue) } });
    ecs_event_t event = ecs_event();
    uint32_t filtered = 0, first_global = 0, second_global = 0;
    ecs_observer(
        { .on = event,
          .query = { .components = { ecs_filter(ObserverValue) } },
          .callback = observer_count_event,
          .user_data = (uintptr_t)&filtered }
    );
    ecs_observer(
        { .on = event, .callback = observer_count_event, .user_data = (uintptr_t)&first_global }
    );
    ecs_observer(
        { .on = event, .callback = observer_count_event, .user_data = (uintptr_t)&second_global }
    );
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
    ecs_observer(
        {
            .on = EcsOnSet,
            .query.components = { ecs_filter(ObserverPosition) },
            .callback = observer_capture_component,
            .user_data = (uintptr_t)&state,
        }
    );

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
    ecs_observer(
        {
            .on = EcsOnAdd,
            .query.components = { ecs_filter(ObserverPosition) },
            .callback = observer_capture_component,
            .user_data = (uintptr_t)&state,
        }
    );

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
    ecs_observer(
        {
            .on = EcsOnRemove,
            .query.components = { ecs_filter(ObserverPosition) },
            .callback = observer_capture_component,
            .user_data = (uintptr_t)&state,
        }
    );

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
    ecs_observer(
        {
            .on = event,
            .callback = observer_capture_component,
            .user_data = (uintptr_t)&state,
        }
    );

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
    ecs_observer(
        {
            .on = EcsOnSet,
            .query.components = { ecs_filter(ObserverPosition) },
            .callback = observer_capture_component,
            .user_data = (uintptr_t)&state,
        }
    );

    ecs_defer_begin();
    ecs_set(entity, ObserverPosition, { 10, 20 });
    test_uint(0, state.calls);
    ecs_defer_end();

    test_uint(1, state.calls);
    test_uint(ecs_id(ObserverPosition), state.component);
    ecs_fini();
}
