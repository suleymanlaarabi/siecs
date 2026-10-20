#include <cpp_plugin.hpp>

ecs_event_t cpp_plugin_event_id() { return ecs::event_id<CppPluginEvent>(); }

ecs_component_t cpp_plugin_component_id() { return ecs::component<CppPluginValue>().id(); }

void cpp_plugin_trigger(ecs_entity_t entity) { ecs::emit(ecs::entity::from(entity), CppPluginEvent{}); }

void cpp_plugin_set(ecs_entity_t entity, int value) {
    ecs::entity::from(entity).set(CppPluginValue{ value });
}
