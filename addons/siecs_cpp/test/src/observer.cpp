#include <siecs_cpp/siecs_cpp.hpp>
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

static int cpp_observer_calls;
static int cpp_observer_read_value;

static CppObserverPosition *cpp_observer_position(ecs::world &world, ecs::entity entity) {
    return static_cast<CppObserverPosition *>(ecs_get_cid(
        world.c_ptr(),
        entity.id(),
        ecs::detail::ecs_cpp_component_id<CppObserverPosition>(world.c_ptr())
    ));
}

void observer_custom_event(void) {
    cpp_observer_calls = 0;

    ecs::world world;
    auto entity = world.entity().set(CppObserverPosition{ .value = 1 });

    world.observe<CppObserverEvent>().each([](CppObserverPosition &position) {
        position.value += 2;
        cpp_observer_calls++;
    });

    world.trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(3, cpp_observer_position(world, entity)->value);
}

void observer_const_arg(void) {
    cpp_observer_calls = 0;
    cpp_observer_read_value = 0;

    ecs::world world;
    auto entity = world.entity().set(CppObserverPosition{ .value = 4 });

    world.observe<CppObserverEvent>().each([](const CppObserverPosition &position) {
        cpp_observer_read_value = position.value;
        cpp_observer_calls++;
    });

    world.trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(4, cpp_observer_read_value);
}

void observer_multi_arg_terms(void) {
    cpp_observer_calls = 0;

    ecs::world world;
    auto entity = world.entity()
                      .set(CppObserverPosition{ .value = 1 })
                      .set(CppObserverVelocity{ .value = 3 });

    world.observe<CppObserverEvent>().each([](CppObserverPosition &position,
                                              const CppObserverVelocity &velocity) {
        position.value += velocity.value;
        cpp_observer_calls++;
    });

    world.trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(4, cpp_observer_position(world, entity)->value);
}

void observer_does_not_match_missing_component(void) {
    cpp_observer_calls = 0;

    ecs::world world;
    auto entity = world.entity().set(CppObserverPosition{ .value = 1 });

    world.observe<CppObserverEvent>().each([](CppObserverPosition &position,
                                              const CppObserverVelocity &velocity) {
        position.value += velocity.value;
        cpp_observer_calls++;
    });

    world.trigger<CppObserverEvent>(entity);

    test_int(0, cpp_observer_calls);
    test_int(1, cpp_observer_position(world, entity)->value);
}

void observer_resource_read(void) {
    cpp_observer_calls = 0;

    ecs::world world;
    world.set_resource(CppObserverTime{ .dt = 3, .elapsed = 0 });
    auto entity = world.entity().set(CppObserverPosition{ .value = 1 });

    world.observe<CppObserverEvent>().each([](ecs::res<const CppObserverTime> time,
                                              CppObserverPosition &position) {
        position.value += time->dt;
        cpp_observer_calls++;
    });

    world.trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(4, cpp_observer_position(world, entity)->value);
}

void observer_resource_write(void) {
    cpp_observer_calls = 0;

    ecs::world world;
    world.set_resource(CppObserverTime{ .dt = 2, .elapsed = 1 });
    auto entity = world.entity().set(CppObserverPosition{ .value = 1 });

    world.observe<CppObserverEvent>().each([](ecs::res<CppObserverTime> time,
                                              CppObserverPosition &position) {
        time->elapsed += time->dt + position.value;
        cpp_observer_calls++;
    });

    world.trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(4, world.resource<CppObserverTime>().elapsed);
}

void observer_resource_does_not_create_query_term(void) {
    cpp_observer_calls = 0;

    ecs::world world;
    world.set_resource(CppObserverTime{ .dt = 1, .elapsed = 0 });
    auto entity = world.entity().set(CppObserverPosition{ .value = 1 });

    world.observe<CppObserverEvent>().each([](ecs::res<const CppObserverTime> time,
                                              CppObserverPosition &position) {
        position.value += time->dt;
        cpp_observer_calls++;
    });

    world.trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(2, cpp_observer_position(world, entity)->value);
}

void observer_resource_field_index_stays_correct(void) {
    cpp_observer_calls = 0;

    ecs::world world;
    world.set_resource(CppObserverTime{ .dt = 5, .elapsed = 0 });
    auto entity = world.entity()
                      .set(CppObserverPosition{ .value = 1 })
                      .set(CppObserverVelocity{ .value = 2 });

    world.observe<CppObserverEvent>().each([](ecs::res<const CppObserverTime> time,
                                              CppObserverPosition &position,
                                              CppObserverVelocity &velocity) {
        position.value += time->dt + velocity.value;
        velocity.value = 8;
        cpp_observer_calls++;
    });

    world.trigger<CppObserverEvent>(entity);

    test_int(1, cpp_observer_calls);
    test_int(8, cpp_observer_position(world, entity)->value);
}
