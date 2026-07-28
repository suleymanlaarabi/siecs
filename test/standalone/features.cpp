#include "siecs.h"
#include <cassert>

#ifndef EXPECT_NAMES
#error "EXPECT_NAMES must be defined"
#endif
#ifndef EXPECT_META
#error "EXPECT_META must be defined"
#endif
#ifndef EXPECT_REST
#error "EXPECT_REST must be defined"
#endif

struct FeatureCppPosition {
    int value;
};

struct FeatureCppResource {
    int value;
};

int main() {
    static_assert(SIECS_HAS_NAMES == EXPECT_NAMES);
    static_assert(SIECS_HAS_META == EXPECT_META);
    static_assert(SIECS_HAS_REST == EXPECT_REST);

    ecs::init();

    ecs::component<FeatureCppPosition>();
    ecs::entity entity = ecs::entity::create<FeatureCppPosition>();
    entity.set(FeatureCppPosition{ 42 });
    assert(entity.get<FeatureCppPosition>().value == 42);

    ecs::set_resource(FeatureCppResource{ 7 });
    assert(ecs::resource<FeatureCppResource>().value == 7);

    ecs::system("FeatureCppSystem").each([] {});
    assert(ecs::entity::create("FeatureCppEntity").is_alive());
    ecs::fini();
}
