#include "module_test.hpp"
#include "c_module_test.h"
#include <test.h>

void module_import_import_without_props(void) {
    reset_module_state();

    ecs_test_scope _ecs_scope;
    auto module = ecs::import<cpp_physics>();
    auto entity = create_module_entity(10, 2);

    ecs::progress();

    test_assert(static_cast<bool>(module));
    test_true(ecs::module<cpp_physics>().is_enabled());
    test_int(1, module_system_calls);
    test_int(12, get_module_position(entity)->value);
}

void module_import_import_with_props(void) {
    reset_module_state();

    ecs_test_scope _ecs_scope;
    auto module = ecs::import<cpp_physics_with_props>(
        cpp_physics_with_props::props_t{ .gravity = 42 });
    create_module_entity(0, 1);

    ecs::progress();

    test_assert(static_cast<bool>(module));
    test_int(1, module_import_calls);
    test_int(42, module_last_gravity);
    test_int(1, module_system_calls);
}

void module_import_double_import(void) {
    reset_module_state();

    ecs_test_scope _ecs_scope;
    auto first = ecs::import<cpp_physics_with_props>(
        cpp_physics_with_props::props_t{ .gravity = 10 });
    auto second = ecs::import<cpp_physics_with_props>(
        cpp_physics_with_props::props_t{ .gravity = 99 });

    test_int(first.id(), second.id());
    test_int(1, module_import_calls);
    test_int(10, module_last_gravity);
}

void module_import_c_declared_module(void) {
    cpp_c_module_import_calls = 0;
    cpp_c_module_last_gravity = 0;

    ecs_test_scope _ecs_scope;
    auto first = ecs::import<cpp_c_module>(cpp_c_module::props_t{ .gravity = 42 });
    auto second = ecs::import<cpp_c_module>(cpp_c_module::props_t{ .gravity = 99 });

    test_assert(static_cast<bool>(first));
    test_int(first.id(), second.id());
    test_int(first.id(), ecs_id(cpp_c_module));
    test_int(1, cpp_c_module_import_calls);
    test_int(42, cpp_c_module_last_gravity);
    test_int(first.id(), ecs::module<cpp_c_module>().id());
}

void module_import_c_declared_module_default_props(void) {
    cpp_c_module_import_calls = 0;
    cpp_c_module_last_gravity = 42;

    ecs_test_scope _ecs_scope;
    auto module = ecs::import<cpp_c_module>();

    test_assert(static_cast<bool>(module));
    test_int(1, cpp_c_module_import_calls);
    test_int(0, cpp_c_module_last_gravity);
}
