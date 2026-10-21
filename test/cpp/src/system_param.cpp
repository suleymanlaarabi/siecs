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
    void world(ecs_world_t *) noexcept { value = &ecs::resource<const ParamTime>().get(); }
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
    auto a = ecs::spawn().set(ParamPosition{1});
    auto b = ecs::spawn().set(ParamPosition{2});
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
    auto a = ecs::spawn().set(ParamPosition{3});
    auto b = ecs::spawn().set(ParamPosition{4});
    auto c = ecs::spawn().set(ParamPosition{5}).set(ParamOther{1});
    world_calls = table_calls = row_calls = 0;
    int calls = 0;
    auto id = ecs::system("custom param").each([&](custom_param value, ParamPosition &position) {
        test_true(value.current != 0);
        position.value += value.batch_size;
        ++calls;
    });
    id.run();
    test_int(3, calls);
    test_int(1, world_calls);
    test_int(2, table_calls);
    test_int(3, row_calls);
    id.run();
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
    ecs::resource<ParamTime>().set(ParamTime{5});
    auto entity = ecs::spawn().set(ParamPosition{2});
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
    auto entity = ecs::spawn().set(ParamPosition{0});
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

#include <array>
#include <cstdlib>
#include <cstring>
#include <atomic>

static std::atomic<unsigned> parallel_init_calls, parallel_world_calls, parallel_table_calls;
static std::atomic<unsigned> parallel_row_calls, parallel_errors, parallel_callbacks;

struct parallel_param {
    ecs_entity_t current = 0;
    uint32_t next_row = 0;
    uint32_t count = 0;
    static parallel_param init() noexcept {
        parallel_init_calls++;
        return {};
    }
    void world(ecs_world_t *world) noexcept {
        if (world != ecs_world_current()) parallel_errors++;
        parallel_world_calls++;
    }
    void table(ecs_iter_t *it) noexcept {
        count = it->count;
        next_row = 0;
        parallel_table_calls++;
    }
    void row(ecs_iter_t *it, uint32_t row) noexcept {
        if (row != next_row++ || row >= count) parallel_errors++;
        current = it->entities[row];
        parallel_row_calls++;
    }
};

static void reset_parallel_params() {
    parallel_init_calls = parallel_world_calls = parallel_table_calls = 0;
    parallel_row_calls = parallel_errors = parallel_callbacks = 0;
}

void system_param_parallel_params_are_local(void) {
    for (uint16_t workers = 0; workers <= 2; workers++) {
        ecs::init({ .worker_threads = workers });
        ecs::resource<ParamTime>().set(ParamTime{7});
        std::array<ecs::entity, 7> entities;
        for (int i = 0; i < 7; i++) entities[i] = ecs::spawn().set(ParamPosition{i});
        reset_parallel_params();
        auto system = ecs::system("portions").parallel(3).each(
            [](parallel_param param, ecs::entity entity, ParamPosition &position,
               ecs::res<const ParamTime> time) {
                if (param.current != entity.id() || param.count > 3 || time->value != 7)
                    parallel_errors++;
                position.value += time->value;
                parallel_callbacks++;
            });
        ecs::progress();
        test_uint(3, parallel_init_calls.load());
        test_uint(3, parallel_world_calls.load());
        test_uint(3, parallel_table_calls.load());
        test_uint(7, parallel_row_calls.load());
        test_uint(7, parallel_callbacks.load());
        test_uint(0, parallel_errors.load());
        for (int i = 0; i < 7; i++) test_int(i + 7, entities[i].get<ParamPosition>().value);
        reset_parallel_params();
        system.run();
        test_uint(3, parallel_world_calls.load());
        test_uint(7, parallel_callbacks.load());
        test_uint(0, parallel_errors.load());
        for (int i = 0; i < 7; i++) test_int(i + 14, entities[i].get<ParamPosition>().value);
        ecs::fini();
    }
}

void system_param_parallel_spawn_visible_after_dependency(void) {
    ecs::init({ .worker_threads = 2 });
    ecs::component<ParamOther>();
    std::array<ecs::entity, 9> sources;
    std::array<ecs_entity_t, 9> created{};
    for (int i = 0; i < 9; i++) sources[i] = ecs::spawn().set(ParamPosition{i});
    reset_parallel_params();
    auto creator = ecs::system("spawn").parallel(2).each([&](ecs::entity source_entity, const ParamPosition &source) {
        auto entity = ecs::spawn().child_of(source_entity).set(ParamOther{source.value + 100});
        created[source.value] = entity.id();
        parallel_callbacks++;
    });
    int observed = 0;
    ecs::system("consume").after(creator).each([&](ecs::entity entity, const ParamOther &other) {
        test_assert(other.value >= 100 && other.value < 109);
        test_uint(created[other.value - 100], entity.id());
        test_uint(sources[other.value - 100].id(), entity.target<ecs::ChildOf>().id());
        ++observed;
    });
    ecs::progress();
    test_int(9, observed);
    test_uint(9, parallel_callbacks.load());
    for (int i = 0; i < 9; i++) test_int(i + 100, ecs::entity::from(created[i]).get<ParamOther>().value);
    ecs::fini();
}

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#endif
static bool parallel_contract_subprocess(const char *testcase) {
#ifdef _WIN32
    const char *child = std::getenv("SIECS_CPP_SYSTEM_ABORT_TEST");
    if (child && std::strcmp(child, testcase) == 0) {
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        return false;
    }
    char executable[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, executable, sizeof executable);
    test_assert(length > 0 && length < sizeof executable);
    test_int(0, _putenv_s("SIECS_CPP_SYSTEM_ABORT_TEST", testcase));
    const char *arguments[] = { executable, testcase, nullptr };
    intptr_t status = _spawnv(_P_WAIT, executable, arguments);
    test_int(0, _putenv_s("SIECS_CPP_SYSTEM_ABORT_TEST", ""));
    test_int(3, status);
    return true;
#else
    (void)testcase;
    test_expect_abort();
    return false;
#endif
}

void system_param_parallel_rejects_mutable_resource(void) {
    if (parallel_contract_subprocess("system_param.parallel_rejects_mutable_resource")) return;
    ecs::init();
    ecs::resource<ParamTime>().set(ParamTime{0});
    ecs::system().parallel().each([](ParamPosition &, ecs::res<ParamTime>) {});
}

void system_param_parallel_rejects_main_thread(void) {
    if (parallel_contract_subprocess("system_param.parallel_rejects_main_thread")) return;
    ecs::init();
    ecs::system().parallel().main_thread().each([](ParamPosition &) {});
}

void system_param_parallel_builtin_relation_targets(void) {
    ecs::init({ .worker_threads = 2 });
    auto parent = ecs::spawn();
    for (int i = 0; i < 7; i++) ecs::spawn().set(ParamPosition{i}).child_of(parent);
    reset_parallel_params();
    ecs::system().parallel(2).each([&](const ParamPosition &position,
                                     ecs::target<ecs::ChildOf> target) {
        if (target.id() != parent.id() || position.value < 0 || position.value >= 7)
            parallel_errors++;
        parallel_callbacks++;
    });
    ecs::progress();
    test_uint(7, parallel_callbacks.load());
    test_uint(0, parallel_errors.load());
    ecs::fini();
}
