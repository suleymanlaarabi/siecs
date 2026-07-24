#include "module_test.hpp"
#include <test.h>

void module_state_lookup_empty(void) {
    ecs_test_scope _ecs_scope;

    auto module = ecs::module<cpp_physics>();

    test_assert(!module);
    test_int(0, module.id());
}

void module_state_enable(void) {
    reset_module_state();

    ecs_test_scope _ecs_scope;
    (void)ecs::import<cpp_physics>();
    create_module_entity(10, 2);

    ecs::module<cpp_physics>().disable();
    test_false(ecs::module<cpp_physics>().is_enabled());
    ecs::progress();
    test_int(0, module_system_calls);

    ecs::module<cpp_physics>().enable();
    test_true(ecs::module<cpp_physics>().is_enabled());
    ecs::progress();
    test_int(1, module_system_calls);
}
