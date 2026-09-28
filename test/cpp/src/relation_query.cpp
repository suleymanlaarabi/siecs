#include <siecs.h>
#include <test.h>
#include <type_traits>

struct RqDense {};
struct RqDepth {};
struct RqGroup {};
struct RqPosition { int x; };
struct RqVelocity { int x; };
struct RqMass { int x; };

static_assert(sizeof(ecs::target<RqDense>) == sizeof(ecs::entity));
static_assert(std::is_trivially_copyable_v<ecs::target<RqDense>>);

static void rq_dense() {
    ecs::relation<RqDense>({ .storage = EcsRelationDense, .on_delete_target = EcsRemoveRelation });
}
static void rq_depth() {
    ecs::relation<RqDepth>({
        .storage = EcsRelationByDepth, .on_delete_target = EcsRemoveRelation, .acyclic = true
    });
}
static void rq_group() {
    ecs::relation<RqGroup>({ .storage = EcsRelationByTarget, .on_delete_target = EcsRemoveRelation });
}

void relation_query_target_dense_is_inferred(void) {
    ecs_test_scope scope;
    rq_dense();
    auto x = ecs::entity::create(), y = ecs::entity::create();
    auto a = ecs::entity::create().relate<RqDense>(x);
    auto b = ecs::entity::create().relate<RqDense>(y);
    int calls = 0;
    ecs::query().each([&](ecs::entity e, ecs::target<RqDense> target) {
        if (e.id() == a.id()) test_uint(target.id(), x.id());
        else if (e.id() == b.id()) test_uint(target.id(), y.id());
        else test_assert(false);
        calls++;
    });
    test_int(calls, 2);
}

void relation_query_target_bydepth_is_inferred(void) {
    ecs_test_scope scope;
    rq_depth();
    auto root = ecs::entity::create();
    auto a = ecs::entity::create().relate<RqDepth>(root);
    auto b = ecs::entity::create().relate<RqDepth>(a);
    int calls = 0;
    ecs::query().order_by_depth<RqDepth>().each([&](ecs::entity e, ecs::target<RqDepth> parent) {
        if (e.id() == a.id()) test_uint(parent.id(), root.id());
        else if (e.id() == b.id()) test_uint(parent.id(), a.id());
        else test_assert(false);
        calls++;
    });
    test_int(calls, 2);
    calls = 0;
    ecs::query().at_depth<RqDepth>(2).each([&](ecs::entity e, ecs::target<RqDepth> parent) {
        test_uint(e.id(), b.id());
        test_uint(parent.id(), a.id());
        calls++;
    });
    test_int(calls, 1);
}

void relation_query_target_bytarget_is_inferred(void) {
    ecs_test_scope scope;
    rq_group();
    auto red = ecs::entity::create(), blue = ecs::entity::create();
    auto a = ecs::entity::create().relate<RqGroup>(red);
    auto b = ecs::entity::create().relate<RqGroup>(red);
    (void)ecs::entity::create().relate<RqGroup>(blue);
    int calls = 0;
    ecs::query().where<RqGroup>(red).each([&](ecs::entity e, ecs::target<RqGroup> group) {
        test_true(e.id() == a.id() || e.id() == b.id());
        test_uint(group.id(), red.id());
        calls++;
    });
    test_int(calls, 2);
}

void relation_query_target_isa_is_inferred(void) {
    ecs_test_scope scope;
    auto base = ecs::entity::create();
    auto instance = ecs::entity::create().is_a(base);
    int calls = 0;
    ecs::query().each([&](ecs::entity e, ecs::target<ecs::IsA> actual) {
        test_uint(e.id(), instance.id());
        test_uint(actual.id(), base.id());
        calls++;
    });
    test_int(calls, 1);
}

void relation_query_target_childof_is_inferred(void) {
    ecs_test_scope scope;
    auto parent = ecs::entity::create();
    auto child = ecs::entity::create().child_of(parent);
    int calls = 0;
    ecs::query().each([&](ecs::entity e, ecs::target<ecs::ChildOf> actual) {
        test_uint(e.id(), child.id());
        test_uint(actual.id(), parent.id());
        calls++;
    });
    test_int(calls, 1);
}

void relation_query_target_does_not_shift_component_fields(void) {
    ecs_test_scope scope;
    rq_dense(); rq_group();
    auto parent = ecs::entity::create(), group = ecs::entity::create();
    (void)ecs::entity::create().set(RqPosition{1}).set(RqVelocity{2})
        .relate<RqDense>(parent).relate<RqGroup>(group);
    int calls = 0;
    ecs::query().each([&](RqPosition &p, ecs::target<RqDense> actual_parent,
                          const RqVelocity &v, ecs::target<RqGroup> actual_group,
                          ecs::optional<RqMass> mass) {
        test_int(p.x, 1); test_int(v.x, 2); test_assert(!mass);
        test_uint(actual_parent.id(), parent.id()); test_uint(actual_group.id(), group.id());
        calls++;
    });
    test_int(calls, 1);
}

void relation_query_target_with_explicit_required_is_deduplicated(void) {
    ecs_test_scope scope;
    rq_dense();
    auto parent = ecs::entity::create();
    (void)ecs::entity::create().relate<RqDense>(parent);
    int calls = 0;
    ecs::query().where<RqDense>().each([&](ecs::target<RqDense> actual) {
        test_uint(actual.id(), parent.id()); calls++;
    });
    test_int(calls, 1);
}

void relation_query_target_with_explicit_target_filter_is_deduplicated(void) {
    relation_query_target_bytarget_is_inferred();
}

void relation_query_target_with_explicit_depth_filter_is_deduplicated(void) {
    relation_query_target_bydepth_is_inferred();
}

void relation_query_target_query_handle_rebuilds_signature(void) {
    ecs_test_scope scope;
    rq_dense();
    auto parent = ecs::entity::create();
    (void)ecs::entity::create().set(RqPosition{1}).relate<RqDense>(parent);
    (void)ecs::entity::create().set(RqPosition{2});
    auto q = ecs::query().build_handle();
    int all = 0, related = 0;
    q.each([&](RqPosition &) { all++; });
    q.each([&](RqPosition &, ecs::target<RqDense> actual) {
        test_uint(actual.id(), parent.id()); related++;
    });
    test_int(all, 2); test_int(related, 1);
    all = 0;
    q.each([&](RqPosition &) { all++; });
    test_int(all, 2);
}

void relation_query_target_system_infers_relation(void) {
    ecs_test_scope scope;
    rq_dense();
    auto parent = ecs::entity::create();
    auto child = ecs::entity::create().relate<RqDense>(parent);
    int calls = 0;
    ecs::system("RelationTargetSystem").each([&](ecs::entity e, ecs::target<RqDense> actual) {
        test_uint(e.id(), child.id()); test_uint(actual.id(), parent.id()); calls++;
    });
    ecs::progress();
    test_int(calls, 1);
}
