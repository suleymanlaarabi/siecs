#ifndef SIECS_CPP_MODULE_TEST_HPP
#define SIECS_CPP_MODULE_TEST_HPP

#include <siecs.h>

struct ModulePosition {
    int value;
};

struct ModuleVelocity {
    int value;
};

inline int module_system_calls;
inline int module_last_gravity;
inline int module_import_calls;

inline void reset_module_state() {
    module_system_calls = 0;
    module_last_gravity = 0;
    module_import_calls = 0;
}

struct cpp_physics {
    void import() {
        (void)ecs::component<ModulePosition>();
        (void)ecs::component<ModuleVelocity>();

        ecs::system("CppMove").each([](ModulePosition &pos, const ModuleVelocity &vel) {
            pos.value += vel.value;
            module_system_calls++;
        });
    }
};

struct cpp_physics_with_props {
    int gravity;

    void import() {
        module_import_calls++;
        module_last_gravity = gravity;

        (void)ecs::component<ModulePosition>();
        (void)ecs::component<ModuleVelocity>();

        ecs::system("CppMoveWithProps").each([](ModulePosition &pos, const ModuleVelocity &vel) {
            pos.value += vel.value;
            module_system_calls++;
        });
    }
};

inline ecs::entity create_module_entity(int position, int velocity) {
    return ecs::entity::create().set(ModulePosition{ position }).set(ModuleVelocity{ velocity });
}

inline ModulePosition *get_module_position(ecs::entity entity) {
    return static_cast<ModulePosition *>(ecs_get_cid(
                entity.id(),
        ecs::detail::ecs_cpp_component_id<ModulePosition>()
    ));
}

#endif
