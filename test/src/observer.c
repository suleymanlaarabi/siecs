#include "siecs.h"
#include <siecs_test.h>

ECS_COMPONENT_DECLARE(ObserverValue, { int value; });
ECS_COMPONENT_DEFINE(ObserverValue);

static uint32_t observer_calls;
static int observer_last_value;
static ecs_entity_t observer_last_entity;

static void reset_observer_state(void) {
    observer_calls = 0;
    observer_last_value = 0;
    observer_last_entity = 0;
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

    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, ObserverValue);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, ObserverValue, { 1 });

    ecs_observer_id_t observer = ecs_observer(
        world,
        {
            .on = EcsOnSet,
            .query = { .terms = { ecs_in(ObserverValue) } },
            .callback = on_observer_value_set,
        }
    );

    ecs_set(world, entity, ObserverValue, { 2 });
    test_int(1, observer_calls);
    test_int(2, observer_last_value);

    ecs_observer_disable(world, observer);
    ecs_set(world, entity, ObserverValue, { 3 });
    test_int(1, observer_calls);
    test_int(2, observer_last_value);

    ecs_observer_enable(world, observer);
    ecs_set(world, entity, ObserverValue, { 4 });
    test_int(2, observer_calls);
    test_int(4, observer_last_value);

    ecs_fini(world);
}

void observer_skips_disabled_by_default(void) {
    reset_observer_state();

    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, ObserverValue);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, ObserverValue, { 1 });
    ecs_add(world, entity, Disabled);

    ecs_observer(
        world,
        {
            .on = EcsOnSet,
            .query = { .terms = { ecs_in(ObserverValue) } },
            .callback = on_observer_value_set,
        }
    );

    ecs_set(world, entity, ObserverValue, { 2 });
    test_int(0, observer_calls);
    test_int(0, observer_last_value);

    ecs_fini(world);
}

void observer_can_match_disabled_when_requested(void) {
    reset_observer_state();

    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, ObserverValue);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, ObserverValue, { 1 });
    ecs_add(world, entity, Disabled);

    ecs_observer(
        world,
        {
            .on = EcsOnSet,
            .query = { .terms = { ecs_in(ObserverValue), ecs_filter(Disabled) } },
            .callback = on_observer_value_set,
        }
    );

    ecs_set(world, entity, ObserverValue, { 2 });
    test_int(1, observer_calls);
    test_int(2, observer_last_value);

    ecs_fini(world);
}

void observer_on_remove_runs_when_entity_is_killed(void) {
    reset_observer_state();

    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, ObserverValue);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, ObserverValue, { 7 });

    ecs_observer(
        world,
        {
            .on = EcsOnRemove,
            .query = { .terms = { ecs_in(ObserverValue) } },
            .callback = on_observer_value_remove,
        }
    );

    ecs_kill(world, entity);

    test_int(1, observer_calls);
    test_int(7, observer_last_value);
    test_assert(observer_last_entity == entity);

    ecs_fini(world);
}

void observer_event_register_reserves_static_ids(void) {
    ecs_event_t event_a = UINT16_MAX;
    ecs_event_t event_b = UINT16_MAX;

    ecs_world_t *first = ecs_init();
    ecs_event_t first_a = ecs_event_register(first, &event_a);
    ecs_event_t first_b = ecs_event_register(first, &event_b);
    test_assert(first_a != first_b);
    ecs_fini(first);

    ecs_world_t *second = ecs_init();
    ecs_event_t second_b = ecs_event_register(second, &event_b);
    ecs_event_t second_a = ecs_event_register(second, &event_a);
    ecs_event_t next = ecs_event(second);

    test_int(first_a, second_a);
    test_int(first_b, second_b);
    test_assert(next > first_a);
    test_assert(next > first_b);

    ecs_fini(second);
}
