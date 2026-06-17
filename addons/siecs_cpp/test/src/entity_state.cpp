#include <siecs_cpp/siecs_cpp.hpp>
#include <test.h>

struct EntityStatePosition {
    int value;
};

static int entity_state_system_calls;

static EntityStatePosition *entity_state_position(ecs::world &world, ecs::entity entity) {
    return static_cast<EntityStatePosition *>(ecs_get_cid(
        world.c_ptr(),
        entity.id(),
        ecs::ecs_cpp_component_id<EntityStatePosition>(world.c_ptr())
    ));
}

void entity_state_enable_disable(void) {
    ecs::world world;
    auto entity = world.entity();

    test_true(entity.is_enabled());
    test_false(entity.is_disabled());
    test_false(entity.has<Disabled>());

    entity.disable();
    test_false(entity.is_enabled());
    test_true(entity.is_disabled());
    test_true(entity.has<Disabled>());

    entity.enable();
    test_true(entity.is_enabled());
    test_false(entity.is_disabled());
    test_false(entity.has<Disabled>());
}

void entity_state_disabled_entities_are_skipped(void) {
    entity_state_system_calls = 0;

    ecs::world world;
    (void)world.component<EntityStatePosition>();

    auto enabled = world.entity().set(EntityStatePosition{ 0 });
    auto disabled = world.entity().set(EntityStatePosition{ 0 }).disable();

    world.system("EntityStateUpdate").each([](EntityStatePosition &position) {
        position.value++;
        entity_state_system_calls++;
    });

    world.progress();

    test_int(1, entity_state_system_calls);
    test_int(1, entity_state_position(world, enabled)->value);
    test_int(0, entity_state_position(world, disabled)->value);
}
