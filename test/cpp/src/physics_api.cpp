#include "physics_events.h"
#include <siecs.h>
#include <test.h>

namespace {
struct Position {
    int value;
};
struct Velocity {
    int value;
};
struct Scope {};
struct IndexedOwner {};
struct Calls {
    int added = 0, set = 0, removed = 0, incoming = 0;
};
} // namespace

void physics_api_component_event_filter(void) {
    ecs_test_scope scope;
    ecs::set_resource(Calls{});
    ecs::observe<ecs::OnAdd>().each_for<Position>([](ecs::res<Calls> calls) { ++calls->added; });
    ecs::observe<ecs::OnSet>().each_for<Position>([](ecs::observer_event event,
                                                     ecs::res<Calls> calls) {
        ++calls->set;
        calls->incoming = event.trigger_data<Position>()->value;
    });
    ecs::observe<ecs::OnRemove>().each_for<Position>([](ecs::res<Calls> calls) {
        ++calls->removed;
    });
    auto e = ecs::entity::create().set(Position{ 4 });
    e.set(Velocity{ 8 });
    e.set(Velocity{ 9 });
    e.remove<Velocity>();
    test_int(1, ecs::resource<Calls>().added);
    test_int(1, ecs::resource<Calls>().set);
    test_int(4, ecs::resource<Calls>().incoming);
    test_int(0, ecs::resource<Calls>().removed);
    e.set(Position{ 7 });
    e.remove<Position>();
    test_int(2, ecs::resource<Calls>().set);
    test_int(7, ecs::resource<Calls>().incoming);
    test_int(1, ecs::resource<Calls>().removed);
}

void physics_api_relation_sources_and_scoped_phase(void) {
    ecs_test_scope scope;
    ecs::relation<Scope>({ .storage = EcsRelationByTarget, .on_delete_target = EcsDeleteSources });
    ecs::relation<IndexedOwner>({ .storage = EcsRelationDense,
                                  .on_delete_target = EcsDeleteSources });
    auto a = ecs::entity::create(), b = ecs::entity::create();
    auto first = ecs::entity::create().relate<Scope>(a).set(Position{ 1 });
    auto second = ecs::entity::create().relate<Scope>(a).set(Position{ 2 });
    auto other = ecs::entity::create().relate<Scope>(b).set(Position{ 8 });
    first.relate<IndexedOwner>(a);
    second.relate<IndexedOwner>(a);
    other.relate<IndexedOwner>(b);
    int count = 0, total = 0;
    for (auto e : a.sources<IndexedOwner>()) {
        ++count;
        total += e.get<Position>().value;
    }
    test_int(2, count);
    test_int(3, total);
    auto update = ecs::system("scope first").manual().with_relation<Scope>().each([](Position &p) {
        p.value += 10;
    });
    ecs::system("scope after").manual().with_relation<Scope>().after(update).each([](Position &p) {
        p.value *= 2;
    });
    ecs::run_phase_target<Scope>(EcsOnUpdate, a);
    test_int(22, first.get<Position>().value);
    test_int(24, second.get<Position>().value);
    test_int(8, other.get<Position>().value);
    a.kill();
    test_false(first.is_alive());
    test_false(second.is_alive());
    test_true(other.is_alive());
}

void physics_api_manual_phase_with_resources(void) {
    ecs_test_scope scope;
    ecs::set_resource(Calls{});
    auto e = ecs::entity::create().set(Position{ 1 });
    auto phase = ecs::phase("manual physics").after(EcsPreUpdate).before(EcsOnUpdate).id();
    auto create = ecs::system("manual resource command")
                      .phase(phase)
                      .manual()
                      .immediate()
                      .no_defer()
                      .each([e](ecs::res<Calls> calls) mutable {
                          e.set(Velocity{ 3 });
                          ++calls->set;
                      });
    auto apply = ecs::system("manual entity update")
                     .phase(phase)
                     .manual()
                     .after(create)
                     .each([](Position &p, const Velocity &v) { p.value += v.value; });
    ecs::system("manual resource result")
        .phase(phase)
        .manual()
        .immediate()
        .after(apply)
        .each([e](ecs::res<Calls> calls) { calls->incoming = e.get<Position>().value; });
    ecs::system("automatic exclusion").phase(phase).each([](ecs::res<Calls> calls) {
        ++calls->added;
    });
    ecs::run_manual_phase(phase);
    test_int(1, ecs::resource<Calls>().set);
    test_int(4, ecs::resource<Calls>().incoming);
    test_int(0, ecs::resource<Calls>().added);
    ecs::run_phase(phase);
    test_int(1, ecs::resource<Calls>().set);
    test_int(4, e.get<Position>().value);
    test_int(1, ecs::resource<Calls>().added);
}

void physics_api_required_component_defaults(void) {
    ecs_test_scope scope;
    struct Box {
        float x = 12, y = 8;
    };
    struct Filter {
        unsigned mask = 0xFFFF;
    };
    struct Body {};
    static_assert(std::is_trivially_copyable_v<Box>);
    ecs::component<Body>().with<Box, Filter>();
    auto e = ecs::entity::create().add<Body>();
    test_int(12, e.get<Box>().x);
    test_int(8, e.get<Box>().y);
    test_int(0xFFFF, e.get<Filter>().mask);
}

void physics_api_deferred_relation_source_deletion(void) {
    ecs_test_scope scope;
    ecs::relation<IndexedOwner>({ .storage = EcsRelationDense });
    auto owner = ecs::entity::create();
    auto a = ecs::entity::create().relate<IndexedOwner>(owner);
    auto b = ecs::entity::create().relate<IndexedOwner>(owner);
    ecs::defer([&] {
        for (auto source : owner.sources<IndexedOwner>())
            source.kill();
        test_true(a.is_alive());
        test_true(b.is_alive());
    });
    test_false(a.is_alive());
    test_false(b.is_alive());
    test_true(owner.sources<IndexedOwner>().empty());
}

void physics_api_default_relation_and_move_observer(void) {
    ecs_test_scope scope;
    struct Body {};
    struct Changed {};
    ecs::relation<Scope>({ .storage = EcsRelationByTarget });
    auto world = ecs::entity::create();
    ecs::component<Body>().with<Position>().with_relation<Scope>(world);
    auto body = ecs::entity::create().add<Body>();
    test_true(body.has<Position>());
    test_int(world.id(), body.target<Scope>().id());
    ecs::observe<ecs::OnSet>().each_for<Position>([](ecs::entity e) { e.add<Changed>(); });
    body.set(Position{ 42 });
    test_true(body.has<Changed>());
    test_int(42, body.get<Position>().value);
    body.set(Position{ 51 });
    test_int(51, body.get<Position>().value);
}

void physics_api_events_across_files_and_worlds(void) {
    for (int world = 0; world < 2; ++world) {
        ecs::init();
        ecs::set_resource(Calls{});
        ecs::observe<PhysicsApiEvent>().each([](ecs::res<Calls> calls) { ++calls->set; });
        auto entity = ecs::entity::create();
        physics_api_emit_from_other_file(entity);
        test_int(1, ecs::resource<Calls>().set);
        ecs::trigger<PhysicsApiEvent>(entity);
        test_int(2, ecs::resource<Calls>().set);
        ecs::fini();
    }
}
