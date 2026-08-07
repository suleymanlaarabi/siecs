#include <siecs.h>
#include "c_types_test.h"
#include <test.h>
#include <string>

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

struct CppNeverRegisteredResource {
    int value;
};

struct CppText {
    std::string value;
};

static int cpp_resource_system_calls;
static int cpp_resource_query_calls;

static CppPosition *cpp_resource_position(ecs::entity entity) {
    return static_cast<CppPosition *>(ecs_get_cid(
                entity.id(),
        ecs::detail::ecs_cpp_component_id<CppPosition>()
    ));
}

static CppText *cpp_text(ecs::entity entity) {
    return static_cast<CppText *>(ecs_get_cid(
                entity.id(),
        ecs::detail::ecs_cpp_component_id<CppText>()
    ));
}

void resource_world_api(void) {
    ecs_test_scope _ecs_scope;

    ecs::set_resource(CppTime{ .dt = 0.016f, .elapsed = 1.0f });

    test_true(ecs::has_resource<CppTime>());
    test_assert(ecs::resource<CppTime>().dt == 0.016f);

    ecs::resource<CppTime>().elapsed = 2.0f;
    test_assert(ecs::resource<const CppTime>().elapsed == 2.0f);
    test_assert(ecs::try_resource<CppTime>() != nullptr);
    test_assert(ecs::try_resource<const CppTime>() != nullptr);

    ecs::remove_resource<CppTime>();
    test_false(ecs::has_resource<CppTime>());
    test_assert(ecs::try_resource<CppTime>() == nullptr);
}

void resource_presence_checks_do_not_register(void) {
    ecs_test_scope _ecs_scope;

    test_false(ecs::has_resource<CppNeverRegisteredResource>());
    test_assert(ecs::try_resource<CppNeverRegisteredResource>() == nullptr);
    ecs::remove_resource<CppNeverRegisteredResource>();
    test_false(ecs::has_resource<CppNeverRegisteredResource>());
}

void resource_system_read(void) {
    cpp_resource_system_calls = 0;

    ecs_test_scope _ecs_scope;
    ecs::set_resource(CppTime{ .dt = 0.5f, .elapsed = 0.0f });

    auto entity = ecs::entity::create().set(CppPosition{ .x = 1.0f }).set(CppVelocity{ .x = 2.0f });

    ecs::system("CppResourceRead")
        .each([](ecs::res<const CppTime> time, CppPosition &position, const CppVelocity &velocity) {
            position.x += velocity.x * time->dt;
            cpp_resource_system_calls++;
        });

    ecs::progress();

    test_int(1, cpp_resource_system_calls);
    test_assert(cpp_resource_position(entity)->x == 2.0f);
}

void resource_system_write(void) {
    ecs_test_scope _ecs_scope;
    ecs::set_resource(CppTime{ .dt = 0.0f, .elapsed = 0.0f });

    ecs::entity::create().set(CppFrame{ .dt = 0.25f });

    ecs::system("CppResourceWrite").each([](ecs::res<CppTime> time, const CppFrame &frame) {
        time->dt = frame.dt;
        time->elapsed += frame.dt;
    });

    ecs::progress();

    test_assert(ecs::resource<CppTime>().dt == 0.25f);
    test_assert(ecs::resource<CppTime>().elapsed == 0.25f);
}

void resource_system_without_query_runs_once(void) {
    cpp_resource_system_calls = 0;

    ecs_test_scope _ecs_scope;
    ecs::set_resource(CppTime{ .dt = 0.25f, .elapsed = 1.0f });

    ecs::system("CppResourceNoQuery").each([](ecs::res<CppTime> time) {
        time->elapsed += time->dt;
        cpp_resource_system_calls++;
    });

    ecs::progress();

    test_int(1, cpp_resource_system_calls);
    test_assert(ecs::resource<CppTime>().elapsed == 1.25f);
}

void resource_system_empty_callback_runs_once(void) {
    cpp_resource_system_calls = 0;

    ecs_test_scope _ecs_scope;

    ecs::system("CppEmptyNoQuery").each([] { cpp_resource_system_calls++; });

    ecs::progress();

    test_int(1, cpp_resource_system_calls);
}

void resource_query_read(void) {
    cpp_resource_query_calls = 0;

    ecs_test_scope _ecs_scope;
    ecs::set_resource(CppTime{ .dt = 2.0f, .elapsed = 0.0f });

    auto entity = ecs::entity::create().set(CppPosition{ .x = 3.0f });

    ecs::query().each([](ecs::res<const CppTime> time, CppPosition &position) {
        position.x += time->dt;
        cpp_resource_query_calls++;
    });

    test_int(1, cpp_resource_query_calls);
    test_assert(cpp_resource_position(entity)->x == 5.0f);
}

void resource_does_not_create_query_term(void) {
    cpp_resource_query_calls = 0;

    ecs_test_scope _ecs_scope;
    ecs::set_resource(CppTime{ .dt = 1.0f, .elapsed = 0.0f });

    auto matched = ecs::entity::create().set(CppPosition{ .x = 0.0f });
    ecs::entity::create().set(CppTime{ .dt = 100.0f, .elapsed = 100.0f });

    ecs::query().each([](ecs::res<const CppTime>, CppPosition &position) {
        position.x += 1.0f;
        cpp_resource_query_calls++;
    });

    test_int(1, cpp_resource_query_calls);
    test_assert(cpp_resource_position(matched)->x == 1.0f);
}

void resource_field_index_stays_correct(void) {
    cpp_resource_query_calls = 0;

    ecs_test_scope _ecs_scope;
    ecs::set_resource(CppTime{ .dt = 3.0f, .elapsed = 0.0f });

    auto entity = ecs::entity::create().set(CppPosition{ .x = 1.0f }).set(CppVelocity{ .x = 4.0f });

    ecs::query().each(
        [](ecs::res<const CppTime> time, CppPosition &position, CppVelocity &velocity) {
            position.x += velocity.x + time->dt;
            velocity.x = 8.0f;
            cpp_resource_query_calls++;
        }
    );

    test_int(1, cpp_resource_query_calls);
    test_assert(cpp_resource_position(entity)->x == 8.0f);
}

void resource_cpp_raii_component_survives_table_migrations(void) {
    ecs_test_scope _ecs_scope;

    auto entity = ecs::entity::create().set(CppText{ .value = "initial" });
    entity.set(CppPosition{ .x = 1.0f });
    entity.remove<CppPosition>();

    test_str("initial", cpp_text(entity)->value.c_str());
}

void resource_capturing_system_keeps_state(void) {
    ecs_test_scope _ecs_scope;
    ecs::entity::create().set(CppPosition{ .x = 1.0f });

    int total = 0;
    ecs::system("Capturing").each([&total](CppPosition &position) {
        total += 3;
        position.x += 2.0f;
    });

    ecs::progress();

    test_int(3, total);
}

void resource_c_declared_resource(void) {
    cpp_c_time_on_set_calls = 0;

    ecs_test_scope _ecs_scope;
    auto time = ecs::resource_handle<cpp_c_time>();
    test_int(ecs_id(cpp_c_time), time.id());

    time.set(cpp_c_time{ .dt = 0.016f });
    test_true(time.has());
    test_assert(ecs::resource<cpp_c_time>().dt == 0.016f);
    test_true(cpp_c_time_on_set_calls > 0);

    ecs::resource<cpp_c_time>().dt = 0.25f;
    test_assert(time.get().dt == 0.25f);
    time.remove();
    test_false(time.has());
}

void resource_cpp_only_methods(void) {
    ecs_test_scope _ecs_scope;

    auto time = ecs::resource_handle<cpp_c_method_time>();
    time.set(cpp_c_method_time{ .dt = 0.016f });
    test_true(time.get().valid());
    test_true(!cpp_c_method_time{ .dt = 0.0f }.valid());
    test_true(ecs::resource_handle<cpp_c_method_time>().has());
}
