#include "siecs.h"
#include <siecs_test.h>

ECS_COMPONENT_DECLARE(ObserverValue, { int value; });
ECS_COMPONENT_DEFINE(ObserverValue);

static uint32_t observer_calls;
static int observer_last_value;

static void reset_observer_state(void) {
    observer_calls = 0;
    observer_last_value = 0;
}

static void on_observer_value_set(ecs_observer_event_t *event) {
    const ObserverValue *value = event->trigger_data;

    observer_calls++;
    observer_last_value = value->value;
}

void observer_enable(void) {
    reset_observer_state();

    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, ObserverValue);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, ObserverValue, { 1 });

    ecs_observer_id_t observer = ecs_observer(world, {
        .on = OnSet,
        .query = { .terms = { ecs_in(ObserverValue) } },
        .callback = on_observer_value_set,
    });

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

    ecs_observer(world, {
        .on = OnSet,
        .query = { .terms = { ecs_in(ObserverValue) } },
        .callback = on_observer_value_set,
    });

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

    ecs_observer(world, {
        .on = OnSet,
        .query = { .terms = { ecs_in(ObserverValue), ecs_filter(Disabled) } },
        .callback = on_observer_value_set,
    });

    ecs_set(world, entity, ObserverValue, { 2 });
    test_int(1, observer_calls);
    test_int(2, observer_last_value);

    ecs_fini(world);
}
