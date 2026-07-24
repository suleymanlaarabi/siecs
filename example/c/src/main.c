#include "siecs.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

ECS_COMPONENT(LeakCounter, { int value; });
ECS_COMPONENT_DECLARE(LeakHooked, { int *value; });

static uint32_t observer_calls;
static uint32_t system_calls;

static void leak_owned_on_remove(ecs_entity_t entity, ecs_component_t component, void *ptr) {
    (void)entity;
    (void)component;

    LeakHooked *owned = ptr;
    free(owned->value);
    owned->value = NULL;
}

ECS_COMPONENT_DEFINE(LeakHooked, .on_remove = leak_owned_on_remove);

static void on_counter_set(ecs_observer_event_t *event) {
    const LeakCounter *counter = event->trigger_data;

    assert(counter->value == 41);
    observer_calls++;
}

static void count_system(ecs_iter_t *it) {
    LeakCounter *counters = ecs_field(it, 0);

    for (uint32_t i = 0; i < it->count; i++) {
        counters[i].value++;
    }
    system_calls++;
}

int main(void) {
    ecs_init();

    ECS_COMPONENT_REGISTER(LeakCounter);
    ECS_COMPONENT_REGISTER(LeakHooked);

    ecs_observer(
        {
            .on = EcsOnSet,
            .query = { .terms = { ecs_in(LeakCounter) } },
            .callback = on_counter_set,
        }
    );

    ecs_system(
        {
            .name = "Count",
            .phase = EcsOnUpdate,
            .query = { .terms = { ecs_inout(LeakCounter) } },
            .callback = count_system,
        }
    );

    int *owned_value = malloc(sizeof(int));
    assert(owned_value != NULL);
    *owned_value = 7;

    ecs_entity_t entity = ecs_new();
    ecs_set(entity, LeakHooked, { owned_value });
    ecs_set(entity, LeakCounter, { 41 });

    ecs_run_phase(EcsOnUpdate);

    assert(observer_calls == 1);
    assert(system_calls == 1);
    assert(ecs_get(entity, LeakCounter)->value == 42);

    ecs_fini();
    return 0;
}
