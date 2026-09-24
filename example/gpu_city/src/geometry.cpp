#include "city.hpp"

#include <bit>
#include <cmath>
#include <cstdio>

namespace gpu_city {
namespace {
struct ShapeModel {};

ecs::entity shape(int kind, float a, float b, float c, Rgb color) {
    char name[96];
    std::snprintf(name, sizeof name, "City shape %d %08x %08x %08x %u %u %u",
                  kind, std::bit_cast<uint32_t>(a), std::bit_cast<uint32_t>(b),
                  std::bit_cast<uint32_t>(c), color.r, color.g, color.b);
    if (auto found = ecs::entity::lookup(name))
        return found.target<ShapeModel>();
    auto prefab = ecs::entity::create();
    if (kind == 0)
        prefab.set(Cuboid{ a, b, c });
    else if (kind == 1)
        prefab.set(Cylinder{ a, b });
    else
        prefab.set(Sphere{ a });
    prefab.set(Color{ color.r, color.g, color.b }).abstract();
    ecs::entity::create(name).relate<ShapeModel>(prefab);
    return prefab;
}

ecs::entity place(ecs::entity prefab, float x, float y, float z, float yaw, bool is_static) {
    auto instance = ecs::entity::create().is_a(prefab)
        .set(Position3d{ x, y, z }, Rotation3d{ 0, yaw, 0 });
    if (is_static)
        instance.add<Static>();
    return instance;
}
} // namespace

uint8_t shade(uint8_t value, float factor) {
    return uint8_t(std::lround(value * factor));
}

namespace geometry {
ecs::entity box(float width, float height, float depth, Rgb color, float x, float y, float z,
                float yaw, bool is_static) {
    return place(shape(0, width, height, depth, color), x, y, z, yaw, is_static);
}

ecs::entity cylinder(float radius, float height, Rgb color, float x, float y, float z,
                     float yaw, bool is_static) {
    return place(shape(1, radius, height, 0, color), x, y, z, yaw, is_static);
}

ecs::entity sphere(float radius, Rgb color, float x, float y, float z) {
    return place(shape(2, radius, 0, 0, color), x, y, z, 0, true);
}

ecs::entity signal_sphere(float radius, Rgb color, float x, float y, float z, SignalLamp lamp,
                          ecs::entity junction) {
    const bool lit = lamp.lit;
    return place(shape(2, radius, 0, 0, color), x, y, z, 0, false)
        .set(Color{ color.r, color.g, color.b }, Bloom{ lit ? 0.9f : 0.0f }, lamp)
        .relate<ControlsJunction>(junction);
}
} // namespace geometry
} // namespace gpu_city
