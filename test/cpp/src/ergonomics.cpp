#include <memory>
#include <siecs.h>
#include <test.h>
#include <type_traits>
#include <utility>

struct ErgoPosition {
    int value;
};
struct ErgoVelocity {
    int value;
};
struct ErgoFrozen {};
struct ErgoTeam {};
struct ErgoParent {};
struct ErgoDamage {
    int amount;
};
struct ErgoOther {};
struct ErgoClock {
    int tick;
};
struct ErgoAbsentResource {
    int value;
};
struct ErgoSingleton {};
struct ErgoToken {
    int *destroyed;
    ~ErgoToken() { ++*destroyed; }
};

static_assert(std::is_same_v<
              decltype(std::declval<ecs::observer<ecs::OnSet> &>().with<ErgoPosition>()),
              ecs::observer<ecs::OnSet> &>);
static_assert(std::is_same_v<
              decltype(std::declval<ecs::system &>().with<ErgoTeam>(ecs::entity{})),
              ecs::system &>);
static_assert(std::is_same_v<decltype(ecs::resource<ErgoClock>().get()), const ErgoClock &>);
static_assert(std::is_same_v<decltype(ecs::resource<ErgoClock>().try_get()), const ErgoClock *>);
static_assert(std::is_same_v<decltype(ecs::resource<ErgoClock>().edit()), ErgoClock &>);
static_assert(std::is_same_v<decltype(ecs::entity{}.get<ErgoPosition>()), const ErgoPosition &>);

template <typename T>
concept CanEdit = requires(T value) { value.edit(); };
static_assert(!CanEdit<ecs::resource_ref<const ErgoClock>>);

void ergonomics_observer_chaining_and_captures(void) {
    ecs_test_scope scope;
    auto entity = ecs::spawn().set(ErgoPosition{ 1 });
    auto ignored = ecs::spawn().set(ErgoPosition{ 2 }).add<ErgoFrozen>();
    int calls = 0;
    auto observer = ecs::observe<ecs::OnSet>().with<ErgoPosition>().without<ErgoFrozen>().each(
        [&](ecs::entity current, const ErgoPosition &position) {
            test_uint(current.id(), entity.id());
            test_int(position.value, 1);
            ++calls;
        }
    );
    test_int(calls, 0);
    entity.set(ErgoPosition{ 3 });
    test_int(calls, 1);
    ignored.set(ErgoPosition{ 4 });
    test_int(calls, 1);
    observer.disable();
    entity.set(ErgoPosition{ 5 });
    test_int(calls, 1);
    observer.enable();
    // OnSet sees the stored value before replacement, matching the C contract.
    observer.remove();
    entity.set(ErgoPosition{ 6 });
    test_int(calls, 1);
}

void ergonomics_observer_capture_lifetime(void) {
    int explicit_destroyed = 0, owner_destroyed = 0, world_destroyed = 0, calls = 0;
    ecs::init();
    auto entity = ecs::spawn();
    auto observer = ecs::observe<ErgoDamage>().each(
        [token = std::unique_ptr<ErgoToken>(new ErgoToken{ &explicit_destroyed }),
         &calls](ecs::event<ErgoDamage> value) {
            test_true(token != nullptr);
            test_int(value->amount, 7);
            ++calls;
        }
    );
    ecs::emit(entity, ErgoDamage{ 7 });
    test_int(calls, 1);
    observer.remove();
    test_int(explicit_destroyed, 1);
    entity.observe<ErgoDamage>([token = std::unique_ptr<ErgoToken>(new ErgoToken{
                                    &owner_destroyed })](ecs::event<ErgoDamage>) { (void)token; });
    ecs::observe<ErgoDamage>().each([token = std::unique_ptr<ErgoToken>(
                                         new ErgoToken{ &world_destroyed }
                                     )](ecs::event<ErgoDamage>) { (void)token; });
    entity.kill();
    test_int(owner_destroyed, 1);
    test_int(world_destroyed, 0);
    ecs::fini();
    test_int(explicit_destroyed, 1);
    test_int(owner_destroyed, 1);
    test_int(world_destroyed, 1);
}

void ergonomics_typed_event_and_owner_handle(void) {
    ecs_test_scope scope;
    auto owner = ecs::spawn().set(ErgoPosition{ 1 });
    auto other = ecs::spawn().set(ErgoPosition{ 2 });
    auto instance = ecs::spawn().is_a(owner);
    int calls = 0;
    auto observer = owner.observe<ErgoDamage>([&](ecs::event<ErgoDamage> damage) {
        test_true(damage.target().id() == owner.id() || damage.target().id() == instance.id());
        test_int((*damage).amount, 11);
        ++calls;
    });
    ecs::emit(other, ErgoDamage{ 11 });
    ecs::emit(owner, ErgoOther{});
    test_int(calls, 0);
    ecs::emit(owner, ErgoDamage{ 11 });
    ecs::emit(instance, ErgoDamage{ 11 });
    test_int(calls, 2);
    observer.disable();
    ecs::emit(owner, ErgoDamage{ 11 });
    test_int(calls, 2);
    observer.enable();
    ecs::emit(owner, ErgoDamage{ 11 });
    test_int(calls, 3);
    observer.remove();
    ecs::emit(owner, ErgoDamage{ 11 });
    test_int(calls, 3);
}

void ergonomics_system_relation_and_inheritance_filters(void) {
    ecs_test_scope scope;
    ecs::relation<ErgoParent>(
        { .storage = EcsRelationByTarget, .on_delete_target = EcsRemoveRelation, .acyclic = true }
    );
    ecs::relation<ErgoTeam>({ .storage = EcsRelationByTarget,
                              .on_delete_target = EcsRemoveRelation });
    auto team = ecs::spawn(), other_team = ecs::spawn();
    auto base = ecs::spawn().abstract();
    auto parent = ecs::spawn().set(ErgoPosition{ 10 });
    auto child = ecs::spawn().is_a(base).relate<ErgoParent>(parent).relate<ErgoTeam>(team).set(
        ErgoVelocity{ 1 }
    );
    ecs::spawn().relate<ErgoParent>(parent).relate<ErgoTeam>(team).set(ErgoVelocity{ 20 });
    ecs::spawn()
        .is_a(base)
        .relate<ErgoParent>(parent)
        .relate<ErgoTeam>(other_team)
        .set(ErgoVelocity{ 30 });
    int calls = 0;
    auto system =
        ecs::system("filtered")
            .with<ErgoTeam>(team)
            .is_a(base)
            .up<ErgoPosition, ErgoParent>()
            .without<ErgoFrozen>()
            .main_thread()
            .disabled()
            .each([&](ecs::entity current, const ErgoPosition &position, ErgoVelocity &velocity) {
                test_uint(current.id(), child.id());
                test_int(position.value, 10);
                velocity.value += position.value;
                ++calls;
            });
    system.run();
    test_int(calls, 0);
    system.enable();
    system.run();
    test_int(calls, 1);
    test_int(child.get<ErgoVelocity>().value, 11);
    system.disable();
    system.run();
    test_int(calls, 1);
    auto dependent = ecs::system("dependent").after(system).each([] {});
    test_true(dependent.id() != system.id());
}

void ergonomics_query_generic_and_ownership(void) {
    ecs_test_scope scope;
    auto entity = ecs::spawn().set(ErgoPosition{ 2 }, ErgoVelocity{ 3 });
    auto query = ecs::query<ecs::entity, ErgoPosition &, const ErgoVelocity &>()
                     .without<ErgoFrozen>()
                     .build();
    auto id = query.id();
    int calls = 0;
    query.each([&](auto current, auto &position, auto &velocity) {
        test_uint(current.id(), entity.id());
        position.value += velocity.value;
        ++calls;
    });
    query.each([&](ecs::entity, ErgoPosition &position, const ErgoVelocity &) {
        position.value++;
        ++calls;
    });
    test_uint(query.id(), id);
    test_int(calls, 2);
    test_int(entity.get<ErgoPosition>().value, 6);
    auto moved = std::move(query);
    test_int(query.id(), 0);
    auto replaced = ecs::query<ecs::entity, ErgoPosition &, const ErgoVelocity &>().build();
    replaced = std::move(moved);
    test_int(moved.id(), 0);
    replaced.each([](auto, auto &position, auto &) { position.value++; });
    test_int(entity.get<ErgoPosition>().value, 7);
    auto raw = ecs::query<ErgoPosition &>().build_raw();
    test_int(ecs_query_count(raw), 1);
    ecs_query_fini(raw);
    ecs::query<ErgoPosition &>().each([](auto &position) { position.value++; });
    test_int(entity.get<ErgoPosition>().value, 8);
}

void ergonomics_resource_read_edit_and_presence(void) {
    ecs_test_scope scope;
    auto absent = ecs::resource<ErgoAbsentResource>();
    test_false(absent.has());
    test_true(absent.try_get() == nullptr);
    test_true(absent.try_edit() == nullptr);
    absent.remove();
    test_int(ecs::ecs_cpp_try_resource_id<ErgoAbsentResource>(), 0);
    auto clock = ecs::resource<ErgoClock>();
    clock.set(ErgoClock{ 3 });
    clock.edit().tick++;
    test_int(clock.get().tick, 4);
    test_true(clock.try_edit() == &clock.edit());
    test_int(ecs::resource<const ErgoClock>().get().tick, 4);
    auto entity = ecs::spawn().set(ErgoPosition{ 2 });
    entity.edit<ErgoPosition>().value++;
    test_int(entity.get<ErgoPosition>().value, 3);
    test_true(entity.try_edit<ErgoVelocity>() == nullptr);
    clock.remove();
    test_false(clock.has());
}

void ergonomics_spawn_ensure_singleton_world_restart(void) {
    ecs::init();
    auto first = ecs::spawn(), second = ecs::spawn();
    test_true(first.id() != second.id());
    auto parent = ecs::ensure("ErgoScene");
    auto child = ecs::spawn(parent);
    test_uint(child.target<ecs::ChildOf>().id(), parent.id());
    auto named = ecs::ensure(parent, "Player");
    test_uint(ecs::ensure("ErgoScene::Player").id(), named.id());
    auto singleton = ecs::singleton<ErgoSingleton>();
    test_uint(ecs::singleton<ErgoSingleton>().id(), singleton.id());
    auto instance = ecs::spawn().is_a<ErgoSingleton>();
    test_true(instance.is<ErgoSingleton>());
    ecs::fini();
    ecs::init();
    for (int i = 0; i < 8; i++)
        ecs::spawn();
    auto current = ecs::singleton<ErgoSingleton>();
    test_true(current.is_alive());
    test_uint(ecs::entity::lookup("ErgoSingleton").id(), current.id());
    ecs::fini();
}

void ergonomics_observer_ancestor_access_and_optional(void) {
    ecs_test_scope scope;
    ecs::relation<ErgoParent>(
        { .storage = EcsRelationByTarget, .on_delete_target = EcsRemoveRelation, .acyclic = true }
    );
    auto parent = ecs::spawn().set(ErgoPosition{ 8 });
    auto child = ecs::spawn().relate<ErgoParent>(parent).set(ErgoVelocity{ 2 });
    auto unrelated = ecs::spawn().set(ErgoVelocity{ 3 });
    int calls = 0;
    auto observer =
        ecs::observe<ErgoDamage>().with<ErgoVelocity>().up<ErgoPosition, ErgoParent>().each(
            [&](ecs::event<ErgoDamage> damage,
                const ErgoPosition &position,
                ecs::optional<const ErgoClock> optional) {
                test_uint(damage.target().id(), child.id());
                test_int(position.value, 8);
                test_false(static_cast<bool>(optional));
                ++calls;
            }
        );
    ecs::emit(child, ErgoDamage{ 4 });
    ecs::emit(unrelated, ErgoDamage{ 4 });
    test_int(calls, 1);
    observer.remove();
}
