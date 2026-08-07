#include <siecs.h>
#include "c_types_test.h"
#include <test.h>

struct ApiPosition {
    int value;
};

struct ApiVelocity {
    int value;
};

struct ApiRelation {};

struct ApiGroup {};

struct ApiLocated {};

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

static int api_order_by_target_desc(
    const ecs_table_t *a,
    const ecs_table_t *b,
    uint64_t data
) {
    ecs_entity_t target_a = ecs_table_target_id(a, (ecs_relation_id_t)data);
    ecs_entity_t target_b = ecs_table_target_id(b, (ecs_relation_id_t)data);
    return target_a < target_b ? 1 : target_a > target_b ? -1 : 0;
}

void api_cpp_wrapper_helpers(void) {
    ecs_test_scope _ecs_scope;

    auto monotonic = ecs::entity::create_no_reuse();
    uint32_t monotonic_index = ecs_entity_id(monotonic.id());
    monotonic.kill();
    auto next_monotonic = ecs::entity::create_no_reuse();
    test_true(ecs_entity_id(next_monotonic.id()) > monotonic_index);

    test_true(ecs::relation<ApiRelation>({
                  .storage = EcsRelationDense,
                  .on_delete_target = EcsDeleteSources,
              }) != 0);

    ecs::relation<ApiGroup>({
        .storage = EcsRelationByTarget,
        .on_delete_target = EcsRemoveRelation,
    });
    ecs::relation<ApiLocated>({
        .storage = EcsRelationByTarget,
        .on_delete_target = EcsRemoveRelation,
        .acyclic = true,
    });

    auto relation_target = ecs::entity::create().set(ApiPosition{ 9 });
    auto related = ecs::entity::create().relate<ApiRelation>(relation_target);
    test_true(related.has_relation<ApiRelation>());
    test_true(related.target<ApiRelation>().id() == relation_target.id());
    related.unrelate<ApiRelation>();
    test_false(related.has_relation<ApiRelation>());

    auto group = ecs::entity::create();
    auto member = ecs::entity::create().relate<ApiGroup>(group);
    test_uint(member.id(), ecs::query().to<ApiGroup>(group).first().id());

    auto second_group = ecs::entity::create();
    auto second_member = ecs::entity::create().set(ApiPosition{ 20 });
    auto first_member = ecs::entity::create().set(ApiPosition{ 10 });
    second_member.relate<ApiGroup>(second_group);
    first_member.relate<ApiGroup>(group);

    auto ordered = ecs::query()
                       .require<ApiPosition>()
                       .order_by_target<ApiGroup>()
                       .build_handle();
    int ordered_sum = 0;
    ordered.each([&](ApiPosition &position) { ordered_sum = ordered_sum * 10 + position.value; });
    test_int(1020, ordered_sum);

    auto custom_ordered = ecs::query()
                              .require<ApiPosition>()
                              .order_by(ecs_query_order_t{
                                  .func = api_order_by_target_desc,
                                  .data = ecs::relation<ApiGroup>(),
                              })
                              .build_handle();
    ordered_sum = 0;
    custom_ordered.each(
        [&](ApiPosition &position) { ordered_sum = ordered_sum * 10 + position.value; }
    );
    test_int(2109, ordered_sum);

    auto location = ecs::entity::create().set(ApiPosition{ 12 });
    auto located = ecs::entity::create().relate<ApiLocated>(location);
    int inherited_position = 0;
    ecs::query().to<ApiLocated>(location).up<ApiPosition, ApiLocated>().each(
        [&](ecs::entity current, const ApiPosition &position) {
            test_true(current.id() == located.id());
            inherited_position = position.value;
        }
    );
    test_int(12, inherited_position);

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

void api_c_declared_component(void) {
    cpp_c_position_on_set_calls = 0;

    ecs_test_scope _ecs_scope;
    auto id = ecs::component<cpp_c_position>();
    test_int(ecs_id(cpp_c_position), id);

    auto entity = ecs::entity::create().set(cpp_c_position{ .value = 1 });
    test_true(entity.has<cpp_c_position>());
    test_int(1, entity.get<cpp_c_position>().value);

    entity.set(cpp_c_position{ .value = 2 });
    test_int(2, entity.get<cpp_c_position>().value);
    test_true(cpp_c_position_on_set_calls > 0);
}

void api_c_declared_relation(void) {
    ecs_test_scope _ecs_scope;
    auto relation = ecs::relation<cpp_c_parent>();
    test_int(ecs_rid(cpp_c_parent), relation);

    auto parent = ecs::entity::create();
    auto child = ecs::entity::create().relate<cpp_c_parent>(parent);
    test_true(child.has_relation<cpp_c_parent>());
    test_int(parent.id(), child.target<cpp_c_parent>().id());
    test_int(child.id(), ecs::query().to<cpp_c_parent>(parent).first().id());
}

void api_cpp_only_methods(void) {
    ecs_test_scope _ecs_scope;

    auto id = ecs::component<cpp_c_method_position>();
    test_int(ecs_id(cpp_c_method_position), id);
    test_str(
        "{ int value; }",
        ecs_id(cpp_c_method_position_desc).struct_desc->fields
    );

    auto position = cpp_c_method_position{ .value = 3 };
    test_int(6, position.doubled());
    position.reset();
    test_int(0, position.value);

    auto entity = ecs::entity::create().set(cpp_c_method_position{ .value = 4 });
    test_int(4, entity.get<cpp_c_method_position>().value);
}
