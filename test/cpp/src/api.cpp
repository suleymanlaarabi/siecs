#include <siecs.h>
#include <test.h>

struct ApiPosition {
    int value;
};

struct ApiVelocity {
    int value;
};

struct ApiRelation {
    ecs::entity target;
};

struct ApiTime {
    float dt;
};

struct ApiHooked {
    int value;
};

struct ApiHookTime {
    float dt;
};

static uint32_t api_component_set_calls = 0;
static uint32_t api_resource_set_calls = 0;

static void api_hooked_on_set(ecs_entity_t, const ApiHooked &, ApiHooked &) {
    api_component_set_calls++;
}

static void api_hook_time_on_set(const ApiHookTime &) {
    api_resource_set_calls++;
}

void api_cpp_wrapper_helpers(void) {
    ecs_test_scope _ecs_scope;

    ecs::relation<ApiRelation>(EcsRelationCascadeDelete);
    test_true(ecs::relation_source<ApiRelation>() != 0);

    auto entity = ecs::entity::create().set(ApiPosition{ 1 });
    ecs::query().require<ApiPosition>().each(
        [](ApiPosition &position, ecs::optional<const ApiVelocity> velocity) {
            if (velocity) position.value += velocity->value;
        }
    );
    auto persistent = ecs::query().require<ApiPosition>().build_handle();
    persistent.each([](ApiPosition &position) { position.value++; });

    auto first = ecs::system("ApiFirst").each([](ApiPosition &position) {
        position.value++;
    });
    auto second = ecs::system("ApiSecond")
                      .after(first)
                      .disabled()
                      .each([](ApiPosition &position) {
                          position.value++;
                      });

    ecs::enable_system(second);
    ecs::run_system(first);
    ecs::run_system(second);
    ecs::disable_system(second);
    test_int(4, entity.get<ApiPosition>().value);

    uint32_t observer_calls = 0;
    ecs::observe<ecs::OnSet>()
        .user_data(&observer_calls)
        .each([](ecs::observer_event event, const ApiPosition &) {
            auto *calls = event.user_data<uint32_t>();
            *calls += event.trigger_data<ApiPosition>() != nullptr;
        });

    entity.set(ApiPosition{ 4 });
    test_int(1, observer_calls);
    test_int(4, entity.get<ApiPosition>().value);
    test_true(ecs::resource_id<ApiTime>() != 0);

    ecs::component_hooks<ApiHooked> component_hooks{ .on_set = api_hooked_on_set };
    ecs::component(component_hooks);
    auto hooked = ecs::entity::create().add<ApiHooked>();
    hooked.set(ApiHooked{ 7 });
    test_int(1, api_component_set_calls);

    ecs::resource_hooks<ApiHookTime> resource_hooks{ .on_set = api_hook_time_on_set };
    auto time = ecs::resource_handle<ApiHookTime>(resource_hooks);
    time.set(ApiHookTime{ .dt = 0.016f });
    test_int(1, api_resource_set_calls);
    test_true(time.has());
    test_true(time.get().dt == 0.016f);
    auto read_time = ecs::resource_handle<const ApiHookTime>();
    test_true(read_time.get().dt == 0.016f);
    time.remove();
    test_false(time.has());
}
