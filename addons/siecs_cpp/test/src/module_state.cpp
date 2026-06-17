#include "module_test.hpp"
#include <test.h>

void module_state_lookup_empty(void) {
    ecs::world world;

    auto module = world.module<cpp_physics>();

    test_assert(!module);
    test_int(0, module.id());
}

void module_state_enable(void) {
    reset_module_state();

    ecs::world world;
    world.import<cpp_physics>();
    create_module_entity(world, 10, 2);

    world.module<cpp_physics>().disable();
    test_false(world.module<cpp_physics>().is_enabled());
    world.progress();
    test_int(0, module_system_calls);

    world.module<cpp_physics>().enable();
    test_true(world.module<cpp_physics>().is_enabled());
    world.progress();
    test_int(1, module_system_calls);
}

void module_state_two_worlds(void) {
    reset_module_state();

    ecs::world first_world;
    ecs::world second_world;

    auto first = first_world.import<cpp_physics_with_props>({ .gravity = 1 });
    auto second = second_world.import<cpp_physics_with_props>({ .gravity = 2 });

    test_assert(static_cast<bool>(first));
    test_assert(static_cast<bool>(second));
    test_int(2, module_import_calls);
    test_true(first_world.module<cpp_physics_with_props>().is_enabled());
    test_true(second_world.module<cpp_physics_with_props>().is_enabled());
}
