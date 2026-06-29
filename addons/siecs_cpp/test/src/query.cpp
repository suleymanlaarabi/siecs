#include <siecs_cpp/siecs_cpp.hpp>
#include <test.h>

struct CppQueryPosition {
    float x;
};

struct CppQueryVelocity {
    float x;
};

static CppQueryVelocity *cpp_query_velocity(ecs::world &world, ecs::entity entity) {
    return static_cast<CppQueryVelocity *>(ecs_get_cid(
        world.c_ptr(),
        entity.id(),
        ecs::detail::ecs_cpp_component_id<CppQueryVelocity>(world.c_ptr())
    ));
}

static CppQueryPosition *cpp_query_position(ecs::world &world, ecs::entity entity) {
    return static_cast<CppQueryPosition *>(ecs_get_cid(
        world.c_ptr(),
        entity.id(),
        ecs::detail::ecs_cpp_component_id<CppQueryPosition>(world.c_ptr())
    ));
}

void query_reads_shared_inherited_field(void) {
    ecs::world world;

    auto base = world.entity().set(CppQueryPosition{ .x = 10.0f });
    ecs_add(world.c_ptr(), base.id(), Abstract);

    (void)world.entity().is_a(base);
    (void)world.entity().is_a(base);

    int calls = 0;
    float sum = 0.0f;

    world.query().each([&](const CppQueryPosition &position) {
        sum += position.x;
        calls++;
    });

    test_int(2, calls);
    test_assert(sum == 20.0f);
}

void query_mutable_does_not_match_shared_inherited_field(void) {
    ecs::world world;

    auto base = world.entity().set(CppQueryPosition{ .x = 10.0f });
    ecs_add(world.c_ptr(), base.id(), Abstract);

    (void)world.entity().is_a(base);

    int calls = 0;

    world.query().each([&](CppQueryPosition &position) {
        position.x += 1.0f;
        calls++;
    });

    test_int(0, calls);
}

void query_reads_shared_and_writes_owned_field(void) {
    ecs::world world;

    auto base = world.entity().set(CppQueryPosition{ .x = 3.0f });
    ecs_add(world.c_ptr(), base.id(), Abstract);

    auto a = world.entity().is_a(base).set(CppQueryVelocity{ .x = 1.0f });
    auto b = world.entity().is_a(base).set(CppQueryVelocity{ .x = 2.0f });

    int calls = 0;

    world.query().each([&](const CppQueryPosition &position, CppQueryVelocity &velocity) {
        velocity.x += position.x;
        calls++;
    });

    test_int(2, calls);
    test_assert(cpp_query_velocity(world, a)->x == 4.0f);
    test_assert(cpp_query_velocity(world, b)->x == 5.0f);
}

void query_owned_override_wins_over_shared_field(void) {
    ecs::world world;

    auto base = world.entity().set(CppQueryPosition{ .x = 10.0f });
    ecs_add(world.c_ptr(), base.id(), Abstract);

    auto entity = world.entity()
                      .is_a(base)
                      .set(CppQueryPosition{ .x = 2.0f })
                      .set(CppQueryVelocity{ .x = 1.0f });

    int calls = 0;

    world.query().each([&](const CppQueryPosition &position, CppQueryVelocity &velocity) {
        velocity.x += position.x;
        calls++;
    });

    test_int(1, calls);
    test_assert(cpp_query_position(world, entity)->x == 2.0f);
    test_assert(cpp_query_velocity(world, entity)->x == 3.0f);
}
