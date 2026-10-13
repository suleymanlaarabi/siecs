#include <cpp_plugin.hpp>
#include <test.h>

static int shared_library_calls;

void shared_library_event_ids_match(void) {
    ecs_test_scope _ecs_scope;

    test_int(cpp_plugin_event_id(), ecs::event<CppPluginEvent>());
}

void shared_library_component_ids_match(void) {
    ecs_test_scope _ecs_scope;
    auto entity = ecs::entity::create();

    cpp_plugin_set(entity.id(), 7);

    test_int(cpp_plugin_component_id(), ecs::component<CppPluginValue>().id());
    test_int(7, entity.get<CppPluginValue>().value);
}

void shared_library_plugin_trigger_reaches_app_observer(void) {
    ecs_test_scope _ecs_scope;
    shared_library_calls = 0;
    ecs::observe<CppPluginEvent>().each([](ecs::entity) { shared_library_calls++; });
    auto entity = ecs::entity::create();

    cpp_plugin_trigger(entity.id());

    test_int(1, shared_library_calls);
}
