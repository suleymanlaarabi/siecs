#include <siecs.h>
#include <test.h>

struct ParamPosition { int value; };
struct ParamTime { int value; };
struct ParamOther { int value; };

static int world_calls;
static int table_calls;
static int row_calls;

struct custom_param {
    ecs_entity_t current = 0;
    int batch_size = 0;

    static custom_param init() noexcept { return {}; }
    void world(ecs_world_t *world) noexcept {
        test_true(world == ecs_world_current());
        ++world_calls;
    }
    void table(ecs_iter_t *it) noexcept {
        batch_size = static_cast<int>(it->count);
        ++table_calls;
    }
    void row(ecs_iter_t *it, uint32_t row) noexcept {
        current = it->entities[row];
        ++row_calls;
    }
};

struct custom_time {
    const ParamTime *value = nullptr;
    static custom_time init() noexcept { return {}; }
    static void query(ecs::system_param_query &query) { query.read_resource<ParamTime>(); }
    void world(ecs_world_t *) noexcept { value = &ecs::resource<const ParamTime>(); }
};

static_assert(ecs::SystemParam<custom_param>);
static_assert(ecs::HasSystemParamQuery<custom_time>);
static_assert(ecs::IsSystemParamWorld<custom_param>);
static_assert(ecs::IsSystemParamTable<custom_param>);
static_assert(ecs::IsSystemParamRow<custom_param>);
static_assert(ecs::SystemParam<ecs::res<const ParamTime>>);
static_assert(ecs::IsSystemParamWorld<ecs::res<const ParamTime>>);
static_assert(ecs::SystemParam<ecs::entity>);
static_assert(ecs::IsSystemParamRow<ecs::entity>);
static_assert(!ecs::SystemParam<ParamPosition &>);

void system_param_custom_query_phases(void) {
    ecs_test_scope scope;
    auto a = ecs::entity::create().set(ParamPosition{1});
    auto b = ecs::entity::create().set(ParamPosition{2});
    world_calls = table_calls = row_calls = 0;
    int calls = 0;
    ecs::query().each([&](custom_param first, custom_param second, ecs::entity e,
                          ParamPosition &position) {
        test_uint(e.id(), first.current);
        test_uint(e.id(), second.current);
        test_int(2, first.batch_size);
        test_int(2, second.batch_size);
        test_true(e.id() == a.id() || e.id() == b.id());
        position.value += 10;
        ++calls;
    });
    test_int(2, calls);
    test_int(2, world_calls);
    test_int(2, table_calls);
    test_int(4, row_calls);
    test_int(11, a.get<ParamPosition>().value);
    test_int(12, b.get<ParamPosition>().value);
}

void system_param_custom_system_phases(void) {
    ecs_test_scope scope;
    auto a = ecs::entity::create().set(ParamPosition{3});
    auto b = ecs::entity::create().set(ParamPosition{4});
    auto c = ecs::entity::create().set(ParamPosition{5}).set(ParamOther{1});
    world_calls = table_calls = row_calls = 0;
    int calls = 0;
    auto id = ecs::system("custom param").each([&](custom_param value, ParamPosition &position) {
        test_true(value.current != 0);
        position.value += value.batch_size;
        ++calls;
    });
    ecs::run_system(id);
    test_int(3, calls);
    test_int(1, world_calls);
    test_int(2, table_calls);
    test_int(3, row_calls);
    ecs::run_system(id);
    test_int(6, calls);
    test_int(2, world_calls);
    test_int(4, table_calls);
    test_int(6, row_calls);
    test_int(7, a.get<ParamPosition>().value);
    test_int(8, b.get<ParamPosition>().value);
    test_int(7, c.get<ParamPosition>().value);
}

void system_param_resource_and_component(void) {
    ecs_test_scope scope;
    ecs::resource_handle<ParamTime>().set(ParamTime{5});
    auto entity = ecs::entity::create().set(ParamPosition{2});
    int calls = 0;
    ecs::query().each([&](ecs::res<const ParamTime> time, custom_time custom,
                          ParamPosition &position) {
        test_int(time->value, custom.value->value);
        position.value += time->value;
        ++calls;
    });
    test_int(1, calls);
    test_int(7, entity.get<ParamPosition>().value);
}

void system_param_custom_observer(void) {
    ecs_test_scope scope;
    auto entity = ecs::entity::create().set(ParamPosition{0});
    world_calls = table_calls = row_calls = 0;
    ecs::observe<ecs::OnSet>().each([](custom_param value, const ParamPosition &position) {
        test_true(value.current != 0);
        test_int(1, value.batch_size);
        test_int(0, position.value);
    });
    entity.set(ParamPosition{9});
    test_int(1, world_calls);
    test_int(1, table_calls);
    test_int(1, row_calls);
}
