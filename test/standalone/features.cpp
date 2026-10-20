#include "siecs.h"
#include <cassert>

struct FeatureCppPosition {
    int value;
};

struct FeatureCppResource {
    int value;
};

int main() {
    static_assert(SIECS_HAS_META == 1);

    ecs::init();

    ecs::component<FeatureCppPosition>();
    ecs::entity entity = ecs::singleton<FeatureCppPosition>();
    entity.set(FeatureCppPosition{ 42 });
    assert(entity.get<FeatureCppPosition>().value == 42);

    ecs::resource<FeatureCppResource>().set(FeatureCppResource{ 7 });
    assert(ecs::resource<FeatureCppResource>().get().value == 7);

    ecs::system("FeatureCppSystem").each([] {});
    assert(ecs::ensure("FeatureCppEntity").is_alive());
    ecs::fini();
}
