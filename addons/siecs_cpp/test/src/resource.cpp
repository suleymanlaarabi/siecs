#include <siecs_cpp/siecs_cpp.hpp>
#include <test.h>

struct CppTime {
    float dt;
    float elapsed;
};

struct CppFrame {
    float dt;
};

struct CppPosition {
    float x;
};

struct CppVelocity {
    float x;
};

static int cpp_resource_system_calls;
static int cpp_resource_query_calls;

static CppPosition *cpp_resource_position(ecs::world &world, ecs::entity entity) {
    return static_cast<CppPosition *>(ecs_get_cid(
        world.c_ptr(),
        entity.id(),
        ecs::ecs_cpp_component_id<CppPosition>(world.c_ptr())
    ));
}

void resource_world_api(void) {
    ecs::world world;

    world.set_resource(CppTime{ .dt = 0.016f, .elapsed = 1.0f });

    test_true(world.has_resource<CppTime>());
    test_assert(world.resource<CppTime>().dt == 0.016f);

    world.resource<CppTime>().elapsed = 2.0f;
    test_assert(world.resource<const CppTime>().elapsed == 2.0f);
    test_assert(world.try_resource<CppTime>() != nullptr);
    test_assert(world.try_resource<const CppTime>() != nullptr);

    world.remove_resource<CppTime>();
    test_false(world.has_resource<CppTime>());
    test_assert(world.try_resource<CppTime>() == nullptr);
}

void resource_system_read(void) {
    cpp_resource_system_calls = 0;

    ecs::world world;
    world.set_resource(CppTime{ .dt = 0.5f, .elapsed = 0.0f });

    auto entity = world.entity().set(CppPosition{ .x = 1.0f }).set(CppVelocity{ .x = 2.0f });

    world.system("CppResourceRead").each(
        [](ecs::res<const CppTime> time, CppPosition &position, const CppVelocity &velocity) {
            position.x += velocity.x * time->dt;
            cpp_resource_system_calls++;
        }
    );

    world.progress();

    test_int(1, cpp_resource_system_calls);
    test_assert(cpp_resource_position(world, entity)->x == 2.0f);
}

void resource_system_write(void) {
    ecs::world world;
    world.set_resource(CppTime{ .dt = 0.0f, .elapsed = 0.0f });

    world.entity().set(CppFrame{ .dt = 0.25f });

    world.system("CppResourceWrite").each([](ecs::res<CppTime> time, const CppFrame &frame) {
        time->dt = frame.dt;
        time->elapsed += frame.dt;
    });

    world.progress();

    test_assert(world.resource<CppTime>().dt == 0.25f);
    test_assert(world.resource<CppTime>().elapsed == 0.25f);
}

void resource_system_without_query_runs_once(void) {
    cpp_resource_system_calls = 0;

    ecs::world world;
    world.set_resource(CppTime{ .dt = 0.25f, .elapsed = 1.0f });

    world.system("CppResourceNoQuery").each([](ecs::res<CppTime> time) {
        time->elapsed += time->dt;
        cpp_resource_system_calls++;
    });

    world.progress();

    test_int(1, cpp_resource_system_calls);
    test_assert(world.resource<CppTime>().elapsed == 1.25f);
}

void resource_system_empty_callback_runs_once(void) {
    cpp_resource_system_calls = 0;

    ecs::world world;

    world.system("CppEmptyNoQuery").each([] { cpp_resource_system_calls++; });

    world.progress();

    test_int(1, cpp_resource_system_calls);
}

void resource_query_read(void) {
    cpp_resource_query_calls = 0;

    ecs::world world;
    world.set_resource(CppTime{ .dt = 2.0f, .elapsed = 0.0f });

    auto entity = world.entity().set(CppPosition{ .x = 3.0f });

    world.query().each([](ecs::res<const CppTime> time, CppPosition &position) {
        position.x += time->dt;
        cpp_resource_query_calls++;
    });

    test_int(1, cpp_resource_query_calls);
    test_assert(cpp_resource_position(world, entity)->x == 5.0f);
}

void resource_does_not_create_query_term(void) {
    cpp_resource_query_calls = 0;

    ecs::world world;
    world.set_resource(CppTime{ .dt = 1.0f, .elapsed = 0.0f });

    auto matched = world.entity().set(CppPosition{ .x = 0.0f });
    world.entity().set(CppTime{ .dt = 100.0f, .elapsed = 100.0f });

    world.query().each([](ecs::res<const CppTime>, CppPosition &position) {
        position.x += 1.0f;
        cpp_resource_query_calls++;
    });

    test_int(1, cpp_resource_query_calls);
    test_assert(cpp_resource_position(world, matched)->x == 1.0f);
}

void resource_field_index_stays_correct(void) {
    cpp_resource_query_calls = 0;

    ecs::world world;
    world.set_resource(CppTime{ .dt = 3.0f, .elapsed = 0.0f });

    auto entity = world.entity().set(CppPosition{ .x = 1.0f }).set(CppVelocity{ .x = 4.0f });

    world.query().each([](ecs::res<const CppTime> time,
                          CppPosition &position,
                          CppVelocity &velocity) {
        position.x += velocity.x + time->dt;
        velocity.x = 8.0f;
        cpp_resource_query_calls++;
    });

    test_int(1, cpp_resource_query_calls);
    test_assert(cpp_resource_position(world, entity)->x == 8.0f);
}
