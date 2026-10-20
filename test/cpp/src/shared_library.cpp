#include <cpp_plugin.hpp>
#include <test.h>
#include <atomic>
#include <cstdio>
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
