#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <test.h>
#include <thread>
#include <vector>

struct SourceOwner {};
struct SourceState {
    int value;
};
struct SourcePosition {
    int value;
};
struct SourceEnabled {};
struct SourceExtra {};

static void source_relation() {
    ecs::relation<SourceOwner>(
        { .storage = EcsRelationByTarget, .on_delete_target = EcsRemoveRelation, .acyclic = false }
    );
}

void source_query_target_filters_and_mutable_source(void) {
    ecs_test_scope scope;
    source_relation();
    auto a = ecs::entity::create().set(SourceState{ 4 }).add<SourceEnabled>();
    auto b = ecs::entity::create().set(SourceState{ 20 });
    auto first = ecs::entity::create().relate<SourceOwner>(a).set(SourcePosition{ 1 });
    auto second = ecs::entity::create().relate<SourceOwner>(a).set(SourcePosition{ 2 });
    ecs::entity::create().relate<SourceOwner>(b).set(SourcePosition{ 100 });
    ecs::entity::create().set(SourcePosition{ 200 });
    auto query = ecs::query().require_target<SourceOwner, SourceEnabled>().build_handle();
    int calls = 0;
    query.each([&](SourcePosition &position, ecs::source<SourceOwner, SourceState> state) {
        test_true(position.value == 1 || position.value == 2);
        state->value += position.value;
        ++calls;
    });
    test_int(2, calls);
    test_int(7, a.get<SourceState>().value);
    a.remove<SourceEnabled>();
    b.add<SourceEnabled>();
    calls = 0;
    query.each([&](SourcePosition &, ecs::source<SourceOwner, const SourceState> state) {
        test_int(20, state->value);
        ++calls;
    });
    test_int(1, calls);
    first.relate<SourceOwner>(b);
    second.relate<SourceOwner>(b);
    calls = 0;
    ecs::query().exclude_target<SourceOwner, SourceEnabled>().each([&](const SourcePosition &p) {
        test_int(200, p.value);
        ++calls;
    });
    test_int(1, calls);
    b.kill();
    calls = 0;
    query.each([&](SourcePosition &, ecs::source<SourceOwner, const SourceState>) { ++calls; });
    test_int(0, calls);
}

void source_query_ancestor_filters_slice_fields_and_targets(void) {
    ecs_test_scope scope;
    auto paused = ecs::entity::create().add<SimulationPaused>();
    auto active = ecs::entity::create();
    auto nested = ecs::entity::create().child_of(paused);
    auto a = ecs::entity::create().child_of(active).set(SourcePosition{ 1 });
    auto b = ecs::entity::create().child_of(nested).set(SourcePosition{ 2 });
    auto c = ecs::entity::create().child_of(active).set(SourcePosition{ 3 });
    auto d = ecs::entity::create().child_of(nested).set(SourcePosition{ 4 });
    auto e = ecs::entity::create().set(SourcePosition{ 5 });
    int calls = 0;
    auto query = ecs::query().exclude_up<ecs::ChildOf, SimulationPaused>().build_handle();
    query.each([&](SourcePosition &p, ecs::target<ecs::ChildOf> parent) {
        test_uint(active.id(), parent.id());
        p.value += 10;
        ++calls;
    });
    test_int(2, calls);
    test_int(11, a.get<SourcePosition>().value);
    test_int(13, c.get<SourcePosition>().value);
    test_int(2, b.get<SourcePosition>().value);
    // No relation still matches an exclusion, and count() respects row slices.
    auto raw =
        ecs::query().require<SourcePosition>().exclude_up<ecs::ChildOf, SimulationPaused>().build();
    test_uint(3, ecs_query_count(raw));
    paused.remove<SimulationPaused>();
    test_uint(5, ecs_query_count(raw));
    active.add<SimulationPaused>();
    test_uint(3, ecs_query_count(raw));
    test_int(5, e.get<SourcePosition>().value);
    test_int(4, d.get<SourcePosition>().value);
    ecs_query_fini(raw);
}

void source_query_manual_scoped_phase_preserves_order(void) {
    ecs_test_scope scope;
    source_relation();
    auto a = ecs::entity::create(), b = ecs::entity::create();
    ecs::entity::create().relate<SourceOwner>(a).set(SourcePosition{ 1 });
    ecs::entity::create().relate<SourceOwner>(b).set(SourcePosition{ 2 });
    std::vector<int> values;
    auto first = ecs::system("manual first").manual().each([&](const SourcePosition &p) {
        values.push_back(p.value);
    });
    ecs::system("manual after").manual().after(first).each([&](const SourcePosition &p) {
        values.push_back(p.value + 10);
    });
    ecs::run_phase(EcsOnUpdate);
    test_uint(0, values.size());
    ecs_run_phase_target(EcsOnUpdate, ecs::detail::ecs_cpp_relation_id<SourceOwner>(), a.id());
    test_uint(2, values.size());
    test_int(1, values[0]);
    test_int(11, values[1]);
    ecs::disable_system(first);
    ecs::run_system_target<SourceOwner>(first, b);
    test_uint(2, values.size());
    ecs::enable_system(first);
    ecs::run_system_target<SourceOwner>(first, b);
    test_uint(3, values.size());
    test_int(2, values[2]);
}

void source_query_scheduler_tracks_shared_writes(void) {
    const ecs_world_feat_desc_t features{ .worker_threads = 2 };
    ecs_init_w_features(&features);
    source_relation();
    auto root = ecs::entity::create().set(SourceState{ 0 });
    ecs::entity::create().relate<SourceOwner>(root).set(SourcePosition{ 0 });
    std::atomic<int> active{ 0 }, overlaps{ 0 };
    auto work = [&] {
        if (active.fetch_add(1))
            ++overlaps;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        --active;
    };
    ecs::system("source writer").each([&](ecs::source<SourceOwner, SourceState>) { work(); });
    ecs::system("root reader").each([&](const SourceState &) { work(); });
    ecs::run_phase(EcsOnUpdate);
    test_int(0, overlaps.load());
    ecs::fini();
}

struct MoveOnlyState {
    std::unique_ptr<int> value;
    std::deque<int> queue{ 1, 2, 3 };
    MoveOnlyState() = default;
    explicit MoveOnlyState(int n) : value(std::make_unique<int>(n)) {}
    MoveOnlyState(MoveOnlyState &&) = default;
    MoveOnlyState &operator=(MoveOnlyState &&) = default;
    MoveOnlyState(const MoveOnlyState &) = delete;
};
struct MoveOnlyModule {
    static void import(std::unique_ptr<int> value) { ecs::set_resource(MoveOnlyState{ *value }); }
};
void source_query_move_only_resources_and_import(void) {
    ecs_test_scope scope;
    ecs::import<MoveOnlyModule>(std::make_unique<int>(42));
    test_int(42, *ecs::resource<MoveOnlyState>().value);
    MoveOnlyState input{ 7 };
    auto entity = ecs::entity::create().set(std::move(input));
    input.queue.push_back(9); // caller's moved-from object is still alive
    ecs::resource<MoveOnlyState>().queue.push_back(4);
    test_uint(4, ecs::resource<MoveOnlyState>().queue.size());
    entity.add<SourceExtra>();
    test_int(7, *entity.get<MoveOnlyState>().value);
    ecs::remove_resource<MoveOnlyState>();
    test_false(ecs::has_resource<MoveOnlyState>());
}

static int source_observer_calls;
void source_query_observers_respect_target_filters(void) {
    ecs_test_scope scope;
    source_relation();
    auto root = ecs::entity::create().set(SourceState{ 42 }).add<SourceEnabled>();
    auto child = ecs::entity::create().relate<SourceOwner>(root).set(SourcePosition{ 1 });
    source_observer_calls = 0;
    auto observer = ecs::observe<ecs::OnSet>();
    observer.require_target<SourceOwner, SourceEnabled>();
    observer.each([](const SourcePosition &p, ecs::source<SourceOwner, const SourceState> state) {
        test_int(42, state->value);
        test_int(1, p.value);
        ++source_observer_calls;
    });
    child.set(SourcePosition{ 2 });
    test_int(1, source_observer_calls);
    root.remove<SourceEnabled>();
    child.set(SourcePosition{ 3 });
    test_int(1, source_observer_calls);
    root.add<SourceEnabled>();
    root.remove<SourceState>();
    child.set(SourcePosition{ 4 });
    test_int(1, source_observer_calls);
}

void source_query_resource_ids_reset_with_world(void) {
    for (int i = 0; i < 3; ++i) {
        ecs::init();
        test_false(ecs::has_resource<MoveOnlyState>());
        source_relation();
        auto parent = ecs::entity::create().set(SourceState{ i });
        auto entity = ecs::entity::create().relate<SourceOwner>(parent).set(SourcePosition{ i });
        test_int(i, entity.get<SourcePosition>().value);
        test_uint(parent.id(), entity.target<SourceOwner>().id());
        ecs::set_resource(MoveOnlyState{ i });
        test_int(i, *ecs::resource<MoveOnlyState>().value);
        ecs::fini();
    }
}
