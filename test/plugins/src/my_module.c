#include <siecs.h>

ECS_COMPONENT_DECLARE(PluginPosition, { int value; });

SIECS_PUBLIC_API uint32_t plugin_import_count;
SIECS_PUBLIC_API uint32_t plugin_system_count;
SIECS_PUBLIC_API uint32_t plugin_observer_count;
SIECS_PUBLIC_API uint32_t plugin_remove_count;

static void plugin_position_remove(
    ecs_entity_t entity,
    ecs_component_t component,
    void *value
) {
    (void)entity;
    (void)component;
    (void)value;
    plugin_remove_count++;
}

ECS_COMPONENT_DEFINE(
    PluginPosition,
    .on_remove = plugin_position_remove,
);

static void plugin_system(ecs_iter_t *it) {
    PluginPosition *positions = ecs_field(it, 0);

    for (uint32_t i = 0; i < it->count; i++) {
        positions[i].value++;
    }

    plugin_system_count++;
}

static void plugin_observer(ecs_observer_event_t *event) {
    (void)event;
    plugin_observer_count++;
}

SIECS_PUBLIC_API void ecs_module_import(void) {
    plugin_import_count++;

    ECS_COMPONENT_REGISTER(PluginPosition);

    ecs_system(
        {
            .name = "PluginSystem",
            .query = { .components = { ecs_inout(PluginPosition) } },
            .callback = plugin_system,
            .phase = EcsOnUpdate,
        }
    );

    ecs_observer(
        {
            .on = EcsOnSet,
            .query = { .components = { ecs_in(PluginPosition) } },
            .callback = plugin_observer,
        }
    );
}
