#include <siecs.h>
#include <test.h>

struct EntityStatePosition {
    int value;
};

struct EntityStateTypedEntity {};

struct EntityStateVelocity {
    int value;
};

struct EntityStateBatchA {};
struct EntityStateBatchB {};
struct EntityStateBatchC {};

struct EntityStateBatchTracked {
    static inline int moves;
    int value = 0;

    EntityStateBatchTracked() = default;
    explicit EntityStateBatchTracked(int value) : value(value) {}
    EntityStateBatchTracked(const EntityStateBatchTracked &) = default;
    EntityStateBatchTracked &operator=(const EntityStateBatchTracked &) = default;

    EntityStateBatchTracked(EntityStateBatchTracked &&other) noexcept : value(other.value) {
        moves++;
    }

    EntityStateBatchTracked &operator=(EntityStateBatchTracked &&other) noexcept {
        value = other.value;
        moves++;
        return *this;
    }
};

static int entity_state_system_calls;

static EntityStatePosition *entity_state_position(ecs::entity entity) {
    return static_cast<EntityStatePosition *>(
        ecs_get_cid(entity.id(), ecs::detail::ecs_cpp_component_id<EntityStatePosition>())
    );
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

void entity_state_variadic_set_and_read(void) {
    ecs_test_scope _ecs_scope;

    auto entity = ecs::entity::create().set(EntityStatePosition{ 10 }, EntityStateVelocity{ 20 });
    const ecs::entity const_entity = entity;

    test_true((entity.has<EntityStatePosition, EntityStateVelocity>()));
    test_int(10, entity.get<EntityStatePosition>().value);
    test_int(20, const_entity.get<EntityStateVelocity>().value);
    test_true(const_entity.try_get<EntityStatePosition>() != nullptr);
    test_true(const_entity.try_get<EntityStateBatchA>() == nullptr);
}

void entity_state_variadic_add_remove_batches_migration(void) {
    ecs_test_scope _ecs_scope;

    auto entity = ecs::entity::create().set(EntityStateBatchTracked{ 42 });

    EntityStateBatchTracked::moves = 0;
    entity.add<EntityStateBatchA, EntityStateBatchB, EntityStateBatchC>();

    test_int(1, EntityStateBatchTracked::moves);
    test_true((entity.has<EntityStateBatchA, EntityStateBatchB, EntityStateBatchC>()));
    test_int(42, entity.get<EntityStateBatchTracked>().value);

    EntityStateBatchTracked::moves = 0;
    entity.remove<EntityStateBatchA, EntityStateBatchB>();

    test_int(1, EntityStateBatchTracked::moves);
    test_false(entity.has<EntityStateBatchA>());
    test_false(entity.has<EntityStateBatchB>());
    test_true(entity.has<EntityStateBatchC>());
    test_int(42, entity.get<EntityStateBatchTracked>().value);
}
