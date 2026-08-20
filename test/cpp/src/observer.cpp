#include <siecs.h>
#include <test.h>

struct CppObserverEvent {};

struct CppObserverPosition {
    int value;
};

struct CppObserverVelocity {
    int value;
};

struct CppObserverTime {
    int dt;
    int elapsed;
};

struct CppObserverRelation {};

static int cpp_observer_calls;
static int cpp_observer_read_value;

struct CppRelationObserverState {
    int set_calls;
    int remove_calls;
    ecs_entity_t old_target;
    ecs_entity_t new_target;
};

static CppObserverPosition *cpp_observer_position(ecs::entity entity) {
    return static_cast<CppObserverPosition *>(ecs_get_cid(
                entity.id(),
        ecs::detail::ecs_cpp_component_id<CppObserverPosition>()
    ));
}

void observer_custom_event(void) {
    cpp_observer_calls = 0;

    ecs_test_scope _ecs_scope;
    auto entity = ecs::entity::create().set(CppObserverPosition{ .value = 1 });

    ecs::observe<CppObserverEvent>().each([](CppObserverPosition &position) {
        position.value += 2;
        cpp_observer_calls++;
    });

    ecs::trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(3, cpp_observer_position(entity)->value);
}

void observer_const_arg(void) {
    cpp_observer_calls = 0;
    cpp_observer_read_value = 0;

    ecs_test_scope _ecs_scope;
    auto entity = ecs::entity::create().set(CppObserverPosition{ .value = 4 });

    ecs::observe<CppObserverEvent>().each([](const CppObserverPosition &position) {
        cpp_observer_read_value = position.value;
        cpp_observer_calls++;
    });

    ecs::trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(4, cpp_observer_read_value);
}

void observer_multi_arg_terms(void) {
    cpp_observer_calls = 0;

    ecs_test_scope _ecs_scope;
    auto entity = ecs::entity::create()
                      .set(CppObserverPosition{ .value = 1 })
                      .set(CppObserverVelocity{ .value = 3 });

    ecs::observe<CppObserverEvent>().each([](CppObserverPosition &position,
                                              const CppObserverVelocity &velocity) {
        position.value += velocity.value;
        cpp_observer_calls++;
    });

    ecs::trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(4, cpp_observer_position(entity)->value);
}

void observer_does_not_match_missing_component(void) {
    cpp_observer_calls = 0;

    ecs_test_scope _ecs_scope;
    auto entity = ecs::entity::create().set(CppObserverPosition{ .value = 1 });

    ecs::observe<CppObserverEvent>().each([](CppObserverPosition &position,
                                              const CppObserverVelocity &velocity) {
        position.value += velocity.value;
        cpp_observer_calls++;
    });

    ecs::trigger<CppObserverEvent>(entity);

    test_int(0, cpp_observer_calls);
    test_int(1, cpp_observer_position(entity)->value);
}

void observer_resource_read(void) {
    cpp_observer_calls = 0;

    ecs_test_scope _ecs_scope;
    ecs::set_resource(CppObserverTime{ .dt = 3, .elapsed = 0 });
    auto entity = ecs::entity::create().set(CppObserverPosition{ .value = 1 });

    ecs::observe<CppObserverEvent>().each([](ecs::res<const CppObserverTime> time,
                                              CppObserverPosition &position) {
        position.value += time->dt;
        cpp_observer_calls++;
    });

    ecs::trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(4, cpp_observer_position(entity)->value);
}

void observer_resource_write(void) {
    cpp_observer_calls = 0;

    ecs_test_scope _ecs_scope;
    ecs::set_resource(CppObserverTime{ .dt = 2, .elapsed = 1 });
    auto entity = ecs::entity::create().set(CppObserverPosition{ .value = 1 });

    ecs::observe<CppObserverEvent>().each([](ecs::res<CppObserverTime> time,
                                              CppObserverPosition &position) {
        time->elapsed += time->dt + position.value;
        cpp_observer_calls++;
    });

    ecs::trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(4, ecs::resource<CppObserverTime>().elapsed);
}

void observer_resource_does_not_create_component_term(void) {
    cpp_observer_calls = 0;

    ecs_test_scope _ecs_scope;
    ecs::set_resource(CppObserverTime{ .dt = 1, .elapsed = 0 });
    auto entity = ecs::entity::create().set(CppObserverPosition{ .value = 1 });

    ecs::observe<CppObserverEvent>().each([](ecs::res<const CppObserverTime> time,
                                              CppObserverPosition &position) {
        position.value += time->dt;
        cpp_observer_calls++;
    });

    ecs::trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(2, cpp_observer_position(entity)->value);
}

void observer_resource_field_index_stays_correct(void) {
    cpp_observer_calls = 0;

    ecs_test_scope _ecs_scope;
    ecs::set_resource(CppObserverTime{ .dt = 5, .elapsed = 0 });
    auto entity = ecs::entity::create()
                      .set(CppObserverPosition{ .value = 1 })
                      .set(CppObserverVelocity{ .value = 2 });

    ecs::observe<CppObserverEvent>().each([](ecs::res<const CppObserverTime> time,
                                              CppObserverPosition &position,
                                              CppObserverVelocity &velocity) {
        position.value += time->dt + velocity.value;
        velocity.value = 8;
        cpp_observer_calls++;
    });

    ecs::trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(8, cpp_observer_position(entity)->value);
}

void observer_relation_events(void) {
    ecs_test_scope _ecs_scope;
    ecs::relation<CppObserverRelation>({
        .storage = EcsRelationByTarget,
        .on_delete_target = EcsRemoveRelation,
    });

    CppRelationObserverState state{};
    auto target_a = ecs::entity::create();
    auto target_b = ecs::entity::create();
    auto source = ecs::entity::create();

    auto set_observer = ecs::observe<ecs::OnRelationSet>();
    set_observer.with_relation<CppObserverRelation>();
    set_observer.user_data(&state).each([](ecs::observer_event event) {
        auto *state = event.user_data<CppRelationObserverState>();
        const auto *data = event.trigger_data<ecs_relation_event_t>();
        state->set_calls++;
        state->old_target = data->old_target;
        state->new_target = data->new_target;
    });

    auto remove_observer = ecs::observe<ecs::OnRelationRemove>();
    remove_observer.with_relation<CppObserverRelation>();
    remove_observer.user_data(&state).each([](ecs::observer_event event) {
        auto *state = event.user_data<CppRelationObserverState>();
        const auto *data = event.trigger_data<ecs_relation_event_t>();
        state->remove_calls++;
        state->old_target = data->old_target;
        state->new_target = data->new_target;
    });

    source.relate<CppObserverRelation>(target_a);
    test_int(1, state.set_calls);
    test_uint(0, state.old_target);
    test_uint(target_a.id(), state.new_target);

    source.relate<CppObserverRelation>(target_b);
    test_int(2, state.set_calls);
    test_uint(target_a.id(), state.old_target);
    test_uint(target_b.id(), state.new_target);

    source.unrelate<CppObserverRelation>();
    test_int(1, state.remove_calls);
    test_uint(target_b.id(), state.old_target);
    test_uint(0, state.new_target);
}
