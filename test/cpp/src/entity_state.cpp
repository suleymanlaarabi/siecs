#include <siecs.h>
#include <test.h>

struct EntityStatePosition {
    int value;
};

struct EntityStateTypedEntity {};

static int entity_state_system_calls;

static EntityStatePosition *entity_state_position(ecs::entity entity) {
    return static_cast<EntityStatePosition *>(ecs_get_cid(
                entity.id(),
        ecs::detail::ecs_cpp_component_id<EntityStatePosition>()
    ));
}

void entity_state_enable_disable(void) {
    ecs_test_scope _ecs_scope;
    ecs::entity empty;
    test_false(empty.is_alive());

    auto entity = ecs::entity::create();

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

    ecs_test_scope _ecs_scope;
    (void)ecs::component<EntityStatePosition>();

    auto enabled = ecs::entity::create().set(EntityStatePosition{ 0 });
    auto disabled = ecs::entity::create().set(EntityStatePosition{ 0 }).disable();

    ecs::system("EntityStateUpdate").each([](EntityStatePosition &position) {
        position.value++;
        entity_state_system_calls++;
    });

    ecs::progress();

    test_int(1, entity_state_system_calls);
    test_int(1, entity_state_position(enabled)->value);
    test_int(0, entity_state_position(disabled)->value);
}

void entity_state_typed_entity_creation(void) {
    ecs_test_scope _ecs_scope;
    auto entity = ecs::entity::create<EntityStateTypedEntity>();

    test_true(entity.is_alive());
    test_true(entity.has<Name>());
}
