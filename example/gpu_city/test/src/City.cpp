#include "city.hpp"
#include <cmath>
#include <cstdio>
#include <siecs/cpp/query.hpp>
#include <siecs_script/cpp.hpp>
#include <test.h>

namespace {
void check_city(int size, uint32_t expected_shapes) {
    ecs::init({ .worker_threads = 0 });
    ecs::import<sispatial>();
    ECS_COMPONENT_REGISTER(Color, Cuboid, Cylinder, Sphere);
    ecs::ensure("CitySettings")
        .set(
            CityLayout{
                size,
                gpu_city::BlockSpacing,
                gpu_city::StreetWidth,
                gpu_city::RoadY,
                gpu_city::PavementY,
            }
        );
    const auto result = ecs::script::run_file(
        "../city.si",
        { .instruction_budget = gpu_city::ScriptInstructionBudget }
    );
    if (!result)
        std::fprintf(stderr, "%s\n", result.error().message.c_str());
    test_true(bool(result));
    const auto city = ecs::entity::lookup("City");
    test_true(city);
    const auto ground = ecs::entity::lookup("City::GroundPlane");
    test_true(ground);
    test_flt(ground.get<Position3d>().y, -1.5f);
    test_flt(
        ground.get<Scale3d>().x,
        (size - 1) * gpu_city::BlockSpacing + gpu_city::StreetWidth + 70
    );
    ecs_progress();
    uint32_t count = 0, invalid = 0;
    ecs::query<>().each([&](ecs::entity entity,
                            const Color &color,
                            const GlobalPosition3d &p,
                            const GlobalScale3d &scale) {
        ++count;
        const auto prefab = entity.target<ecs::IsA>();
        auto parent = entity.target<ecs::ChildOf>();
        while (parent.id() && parent.id() != city.id())
            parent = parent.target<ecs::ChildOf>();
        invalid += !prefab || !prefab.has<Abstract>() || !entity.has<Static>() ||
                   parent.id() != city.id() || color.a != 255 || !std::isfinite(p.x + p.y + p.z) ||
                   !(scale.x > 0 && scale.y > 0 && scale.z > 0);
    });
    test_uint(count, expected_shapes);
    test_uint(invalid, 0);
    test_uint(ecs_component_lookup("gpu_city::Car"), 0);
    ecs::fini();
}
} // namespace

void City_small_grid(void) { check_city(4, 1693); }

void City_full_grid(void) {
    // Static city output measured from the original C++ generator, excluding traffic lights.
    check_city(64, 495359);
}
