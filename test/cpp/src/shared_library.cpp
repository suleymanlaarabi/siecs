#include <atomic>
#include <cpp_plugin.hpp>
#include <cstdio>
#include <test.h>
#include <thread>

static int shared_library_calls;

void shared_library_type_slots_survive_growth(void) {
    ecs_test_scope _ecs_scope;
    auto id = ecs::component<CppPluginValue>().id();
    auto *slot = &ecs::detail::typed_id<CppPluginValue, ecs::detail::id_kind::component>();
    for (unsigned i = 0; i < 4096; i++) {
        char key[64];
        std::snprintf(key, sizeof key, "component:CppSlotGrowth%u", i);
        ecs_type_slot(key, 0);
    }
    test_ptr(slot, (&ecs::detail::typed_id<CppPluginValue, ecs::detail::id_kind::component>()));
    test_int(id, *slot);
    test_int(id, cpp_plugin_component_id());
    test_int(id, ecs_component_lookup("CppPluginValue"));
    auto entity = ecs::spawn();
    cpp_plugin_set(entity.id(), 83);
    test_int(83, entity.get<CppPluginValue>().value);
}

void shared_library_concurrent_type_slots(void) {
    uint16_t *shared = ecs_type_slot("component:ConcurrentSharedSlot", 73);
    std::atomic<bool> valid{ true };
    std::thread workers[8];
    for (unsigned worker = 0; worker < 8; worker++) {
        workers[worker] = std::thread([worker, shared, &valid]() {
            for (unsigned i = 0; i < 256; i++) {
                char key[80];
                std::snprintf(key, sizeof key, "component:ConcurrentSlot%u:%u", worker, i);
                uint16_t *slot = ecs_type_slot(key, 0);
                *slot = static_cast<uint16_t>(worker * 256 + i);
                if (ecs_type_slot(key, 99) != slot || *slot != worker * 256 + i ||
                    ecs_type_slot("component:ConcurrentSharedSlot", 0) != shared || *shared != 73)
                    valid.store(false);
            }
        });
    }
    for (auto &worker : workers)
        worker.join();
    test_true(valid.load());
    test_int(73, *shared);
}

void shared_library_event_ids_match(void) {
    ecs_test_scope _ecs_scope;

    test_int(cpp_plugin_event_id(), ecs::event_id<CppPluginEvent>());
}

void shared_library_component_ids_match(void) {
    ecs_test_scope _ecs_scope;
    auto entity = ecs::spawn();

    cpp_plugin_set(entity.id(), 7);

    test_int(cpp_plugin_component_id(), ecs::component<CppPluginValue>().id());
    test_int(7, entity.get<CppPluginValue>().value);
}

void shared_library_plugin_trigger_reaches_app_observer(void) {
    ecs_test_scope _ecs_scope;
    shared_library_calls = 0;
    ecs::observe<CppPluginEvent>().each([](ecs::entity) { shared_library_calls++; });
    auto entity = ecs::spawn();

    cpp_plugin_trigger(entity.id());

    test_int(1, shared_library_calls);
}

struct RestartEvent {
    int value;
};
struct UnrelatedRestartEvent {};

void shared_library_events_survive_world_restart(void) {
    for (int cycle = 0; cycle < 3; cycle++) {
        ecs_test_scope scope;
        auto source = ecs::spawn();
        const auto event_id = ecs::event_id<RestartEvent>();
        test_assert(event_id > EcsOnPairRemove);
        test_int(cpp_plugin_event_id(), ecs::event_id<CppPluginEvent>());
        test_true(event_id != cpp_plugin_event_id());
        test_assert(cpp_plugin_event_id() > EcsOnPairRemove);
        RestartEvent payload{ 37 + cycle };
        unsigned calls = 0, plugin_calls = 0;
        ecs::observe<RestartEvent>().each([&](ecs::entity entity, ecs::event<RestartEvent> event) {
            test_uint(source.id(), entity.id());
            test_uint(source.id(), event.target().id());
            test_int(event_id, event.id());
            test_ptr(&payload, &*event);
            test_int(37 + cycle, event->value);
            calls++;
        });
        ecs::observe<CppPluginEvent>().each([&](ecs::entity entity,
                                                ecs::event<CppPluginEvent> event) {
            test_uint(source.id(), entity.id());
            test_int(cpp_plugin_event_id(), event.id());
            test_true(event.trigger_data<CppPluginEvent>() != nullptr);
            plugin_calls++;
        });
        ecs::emit(source, UnrelatedRestartEvent{});
        test_uint(0, calls);
        test_uint(0, plugin_calls);
        ecs::emit(source, payload);
        test_uint(1, calls);
        test_uint(0, plugin_calls);
        cpp_plugin_trigger(source.id());
        test_uint(1, calls);
        test_uint(1, plugin_calls);
    }
}
