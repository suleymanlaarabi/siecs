#include "module_test.hpp"
#include <test.h>

void module_import_import_without_props(void) {
    reset_module_state();

    ecs::world world;
    auto module = world.import<cpp_physics>();
    auto entity = create_module_entity(world, 10, 2);

    world.progress();

    test_assert(static_cast<bool>(module));
    test_true(world.module<cpp_physics>().is_enabled());
    test_int(1, module_system_calls);
    test_int(12, get_module_position(world, entity)->value);
}

void module_import_import_with_props(void) {
    reset_module_state();

    ecs::world world;
    auto module = world.import<cpp_physics_with_props>({ .gravity = 42 });
    create_module_entity(world, 0, 1);

    world.progress();

    test_assert(static_cast<bool>(module));
    test_int(1, module_import_calls);
    test_int(42, module_last_gravity);
    test_int(1, module_system_calls);
}

void module_import_double_import(void) {
    reset_module_state();

    ecs::world world;
    auto first = world.import<cpp_physics_with_props>({ .gravity = 10 });
    auto second = world.import<cpp_physics_with_props>({ .gravity = 99 });

    test_int(first.id(), second.id());
    test_int(1, module_import_calls);
    test_int(10, module_last_gravity);
}
