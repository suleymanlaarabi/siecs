#include <siecs.h>
#include <test.h>

struct CppQueryPosition {
    float x;
};

struct CppQueryVelocity {
    float x;
};

struct CppQueryMass {
    float value;
};

struct CppQueryScale {
    float value;
};

struct CppQueryOwnedInherited {
    int value;
};

static void register_shared_query_position() {
    ecs::component<CppQueryPosition>(ecs::component_options<CppQueryPosition>{
        .inheritance = EcsInheritShared,
    });
}

static CppQueryVelocity *cpp_query_velocity(ecs::entity entity) {
    return static_cast<CppQueryVelocity *>(ecs_get_cid(
                entity.id(),
        ecs::detail::ecs_cpp_component_id<CppQueryVelocity>()
    ));
}

static CppQueryPosition *cpp_query_position(ecs::entity entity) {
    return static_cast<CppQueryPosition *>(ecs_get_cid(
                entity.id(),
        ecs::detail::ecs_cpp_component_id<CppQueryPosition>()
    ));
}

void query_reads_shared_inherited_field(void) {
    ecs_test_scope _ecs_scope;

    register_shared_query_position();
    auto base = ecs::entity::create().set(CppQueryPosition{ .x = 10.0f });
    ecs_add(base.id(), Abstract);

    (void)ecs::entity::create().is_a(base);
    (void)ecs::entity::create().is_a(base);

    int calls = 0;
    float sum = 0.0f;

    ecs::query().each([&](const CppQueryPosition &position) {
        sum += position.x;
        calls++;
    });

    test_int(2, calls);
    test_assert(sum == 20.0f);
}

void query_mutable_does_not_match_shared_inherited_field(void) {
    ecs_test_scope _ecs_scope;

    register_shared_query_position();
    auto base = ecs::entity::create().set(CppQueryPosition{ .x = 10.0f });
    ecs_add(base.id(), Abstract);

    (void)ecs::entity::create().is_a(base);

    int calls = 0;

    ecs::query().each([&](CppQueryPosition &position) {
        position.x += 1.0f;
        calls++;
    });

    test_int(0, calls);
}

void query_reads_shared_and_writes_owned_field(void) {
    ecs_test_scope _ecs_scope;

    register_shared_query_position();
    auto base = ecs::entity::create().set(CppQueryPosition{ .x = 3.0f });
    ecs_add(base.id(), Abstract);

    auto a = ecs::entity::create().is_a(base).set(CppQueryVelocity{ .x = 1.0f });
    auto b = ecs::entity::create().is_a(base).set(CppQueryVelocity{ .x = 2.0f });

    int calls = 0;

    ecs::query().each([&](const CppQueryPosition &position, CppQueryVelocity &velocity) {
        velocity.x += position.x;
        calls++;
    });

    test_int(2, calls);
    test_assert(cpp_query_velocity(a)->x == 4.0f);
    test_assert(cpp_query_velocity(b)->x == 5.0f);
}

void query_owned_override_wins_over_shared_field(void) {
    ecs_test_scope _ecs_scope;

    register_shared_query_position();
    auto base = ecs::entity::create().set(CppQueryPosition{ .x = 10.0f });
    ecs_add(base.id(), Abstract);

    auto entity = ecs::entity::create()
                      .is_a(base)
                      .set(CppQueryPosition{ .x = 2.0f })
                      .set(CppQueryVelocity{ .x = 1.0f });

    int calls = 0;

    ecs::query().each([&](const CppQueryPosition &position, CppQueryVelocity &velocity) {
        velocity.x += position.x;
        calls++;
    });

    test_int(1, calls);
    test_assert(cpp_query_position(entity)->x == 2.0f);
    test_assert(cpp_query_velocity(entity)->x == 3.0f);
}

void query_owned_inherited_field_is_owned_by_default(void) {
    ecs_test_scope _ecs_scope;

    auto base = ecs::entity::create().set(CppQueryOwnedInherited{ .value = 10 });
    base.abstract();
    auto child = ecs::entity::create().is_a(base);

    ecs_component_t component = ecs::component<CppQueryOwnedInherited>();
    test_true(ecs_has_cid_owned(child.id(), component));
    test_assert(ecs_get_cid(child.id(), component) != ecs_get_cid(base.id(), component));
    test_int(10, child.get<CppQueryOwnedInherited>().value);

    int calls = 0;
    ecs::query().each([&](CppQueryOwnedInherited &value) {
        value.value++;
        calls++;
    });

    test_int(1, calls);
    test_int(11, child.get<CppQueryOwnedInherited>().value);
    test_int(10, base.get<CppQueryOwnedInherited>().value);
}

void query_each_receives_entity(void) {
    ecs_test_scope _ecs_scope;

    auto first = ecs::entity::create().set(CppQueryPosition{ .x = 1.0f });
    auto second = ecs::entity::create().set(CppQueryPosition{ .x = 2.0f });
    int calls = 0;
    bool saw_first = false;
    bool saw_second = false;

    ecs::query().each([&](ecs::entity current, const CppQueryPosition &position) {
        calls++;
        saw_first |= current.id() == first.id() && position.x == 1.0f;
        saw_second |= current.id() == second.id() && position.x == 2.0f;
    });

    test_int(2, calls);
    test_true(saw_first);
    test_true(saw_second);
}

void query_system_reads_shared_fields_with_interleaved_resource(void) {
    ecs_test_scope _ecs_scope;
    register_shared_query_position();
    ecs::component<CppQueryMass>(ecs::component_options<CppQueryMass>{
        .inheritance = EcsInheritShared,
    });
    ecs::set_resource(CppQueryScale{ .value = 2.0f });

    auto base =
        ecs::entity::create().set(CppQueryPosition{ .x = 3.0f }).set(CppQueryMass{ .value = 4.0f });
    ecs_add(base.id(), Abstract);

    auto a = ecs::entity::create().is_a(base).set(CppQueryVelocity{ .x = 1.0f });
    auto b = ecs::entity::create().is_a(base).set(CppQueryVelocity{ .x = 2.0f });
    int calls = 0;

    ecs::system("CppSharedFields")
        .each([&](const CppQueryPosition &position,
                  ecs::res<const CppQueryScale> scale,
                  const CppQueryMass &mass,
                  CppQueryVelocity &velocity) {
            velocity.x += (position.x + mass.value) * scale->value;
            calls++;
        });

    ecs::progress();

    test_int(2, calls);
    test_assert(cpp_query_velocity(a)->x == 15.0f);
    test_assert(cpp_query_velocity(b)->x == 16.0f);
}

void query_system_each_receives_entity(void) {
    ecs_test_scope _ecs_scope;

    auto target = ecs::entity::create().set(CppQueryPosition{ .x = 1.0f });
    int calls = 0;
    bool saw_target = false;

    ecs::system("CppEntityArgument").each([&](ecs::entity current, CppQueryPosition &position) {
        calls++;
        saw_target = current.id() == target.id();
        position.x += 1.0f;
    });

    ecs::progress();

    test_int(1, calls);
    test_true(saw_target);
    test_assert(cpp_query_position(target)->x == 2.0f);
}
