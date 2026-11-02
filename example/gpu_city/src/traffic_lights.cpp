#include "city.hpp"

#include <cmath>

namespace gpu_city {
namespace {

Position3d local(float x, float y, float z, float yaw, float lx, float ly, float lz) {
    const float c = std::cos(yaw), s = std::sin(yaw);
    return { x + lx * c + lz * s, y + ly, z - lx * s + lz * c };
}

Rgb color_for(uint8_t role, bool lit) {
    constexpr Rgb colors[] = { { 255, 35, 25 }, { 255, 177, 18 }, { 35, 230, 75 } };
    Rgb color = colors[role];
    if (!lit)
        color = { uint8_t(color.r * 0.16f), uint8_t(color.g * 0.16f), uint8_t(color.b * 0.16f) };
    return color;
}

void signal(
    float x,
    float y,
    float z,
    float yaw,
    uint8_t role,
    uint8_t base,
    ecs::entity node,
    uint8_t axis
) {
    const auto p = local(x, y, z, yaw, 0.0f, 0.0f, 0.14f);
    geometry::signal_sphere(
        0.055f,
        color_for(role, role == base),
        p.x,
        p.y,
        p.z,
        SignalLamp{ role, axis, role == base },
        node
    );
}

void traffic_light(float x, float z, float yaw, bool green, ecs::entity node) {
    auto p = local(x, PavementY, z, yaw, 0.0f, 0.035f, 0.0f);
    geometry::cylinder(0.13f, 0.07f, { 35, 38, 43 }, p.x, p.y, p.z, yaw);
    p = local(x, PavementY, z, yaw, 0.0f, 0.37f, 0.0f);
    geometry::cylinder(0.045f, 0.62f, { 75, 79, 84 }, p.x, p.y, p.z, yaw);
    p = local(x, PavementY, z, yaw, 0.0f, 0.9f, 0.0f);
    geometry::box(0.31f, 0.65f, 0.22f, { 24, 27, 32 }, p.x, p.y, p.z, yaw);
    p = local(x, PavementY, z, yaw, 0.0f, 0.9f, 0.12f);
    geometry::box(0.26f, 0.65f, 0.025f, { 8, 10, 13 }, p.x, p.y, p.z, yaw);
    const uint8_t base = green ? SignalGreen : SignalRed;
    const uint8_t axis = green ? 0 : 1;
    signal(x, PavementY + 1.14f, z, yaw, SignalRed, base, node, axis);
    signal(x, PavementY + 0.95f, z, yaw, SignalAmber, base, node, axis);
    signal(x, PavementY + 0.73f, z, yaw, SignalGreen, base, node, axis);
}
} // namespace

void build_traffic_lights() {
    const float half = (CitySize - 1) / 2.0f;
    const float offset = StreetWidth / 2.0f + 0.55f;
    for (int row = 0; row < CitySize; ++row) {
        for (int column = 0; column < CitySize; ++column) {
            const auto node = traffic::junction(column, row);
            if (node.get<traffic::Junction>().roundabout)
                continue;
            const float x = (column - half) * BlockSpacing;
            const float z = (row - half) * BlockSpacing;
            traffic_light(x - offset, z - offset, 3.14159265f, false, node);
            traffic_light(x + offset, z - offset, 1.57079633f, true, node);
            traffic_light(x - offset, z + offset, -1.57079633f, true, node);
            traffic_light(x + offset, z + offset, 0.0f, false, node);
        }
    }
}

} // namespace gpu_city
