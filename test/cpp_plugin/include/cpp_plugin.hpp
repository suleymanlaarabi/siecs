#pragma once

#include <siecs.h>

/* Types shared by the test executable and the C++ shared library below. */
struct CppPluginEvent {};

struct CppPluginValue {
    int value;
};

/* Implemented in a separate shared library: ids come from its own template instances. */
ecs_event_t cpp_plugin_event_id();
ecs_component_t cpp_plugin_component_id();
void cpp_plugin_trigger(ecs_entity_t entity);
void cpp_plugin_set(ecs_entity_t entity, int value);
