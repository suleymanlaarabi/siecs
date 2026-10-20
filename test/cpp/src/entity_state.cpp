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

    auto entity = ecs::spawn();

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

    auto enabled = ecs::spawn().set(EntityStatePosition{ 0 });
    auto disabled = ecs::spawn().set(EntityStatePosition{ 0 }).disable();

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
    auto entity = ecs::singleton<EntityStateTypedEntity>();

    test_true(entity.is_alive());
    test_true(entity.has<Name>());
}

void entity_state_variadic_set_and_read(void) {
    ecs_test_scope _ecs_scope;

    auto entity = ecs::spawn().set(EntityStatePosition{ 10 }, EntityStateVelocity{ 20 });
    const ecs::entity const_entity = entity;

    test_true((entity.has<EntityStatePosition, EntityStateVelocity>()));
    test_int(10, entity.get<EntityStatePosition>().value);
    test_int(20, const_entity.get<EntityStateVelocity>().value);
    test_true(const_entity.try_get<EntityStatePosition>() != nullptr);
    test_true(const_entity.try_get<EntityStateBatchA>() == nullptr);
}

void entity_state_variadic_add_remove_batches_migration(void) {
    ecs_test_scope _ecs_scope;

    auto entity = ecs::spawn().set(EntityStateBatchTracked{ 42 });

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

void entity_state_variadic_set_single_move_keeps_last_duplicate(void) {
    ecs_test_scope _ecs_scope;

    auto entity = ecs::spawn().set(
        EntityStatePosition{ 1 },
        EntityStateVelocity{ 2 },
        EntityStatePosition{ 3 }
    );
    test_int(3, entity.get<EntityStatePosition>().value);
    test_int(2, entity.get<EntityStateVelocity>().value);

    // Already owned components are overwritten in place.
    entity.set(EntityStateVelocity{ 4 }, EntityStatePosition{ 5 });
    test_int(5, entity.get<EntityStatePosition>().value);
    test_int(4, entity.get<EntityStateVelocity>().value);

    auto other = ecs::spawn().set(EntityStateVelocity{ 6 }, EntityStatePosition{ 7 });
    test_int(7, other.get<EntityStatePosition>().value);
    test_int(6, other.get<EntityStateVelocity>().value);
    test_int(5, entity.get<EntityStatePosition>().value);
}

void entity_state_variadic_set_observer_sees_every_value(void) {
    ecs_test_scope _ecs_scope;

    // Warm the archetype edges so the single-move path would be available.
    (void)ecs::spawn().set(EntityStatePosition{ 0 }, EntityStateVelocity{ 0 });

    int calls = 0;
    auto observer = ecs::observe<ecs::OnAdd>().each(
        [&](ecs::entity current, const EntityStatePosition &) {
            // A multi-value set is one structural change: when the observer
            // runs, every component of the call is already on the entity.
            test_true(current.has<EntityStateVelocity>());
            ++calls;
        }
    );
    (void)ecs::spawn().set(EntityStatePosition{ 10 }, EntityStateVelocity{ 20 });
    test_int(2, calls); // one OnAdd per added component, both after the move
    observer.remove();
}
