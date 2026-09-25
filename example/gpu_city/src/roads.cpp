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
    for (float y : { 1.22f, 1.05f, 0.85f }) {
        p = local(x, PavementY, z, yaw, 0.0f, y, 0.15f);
        geometry::box(0.26f, 0.035f, 0.1f, { 18, 20, 24 }, p.x, p.y, p.z, yaw);
    }
}
void section(float x, float z, bool horizontal) {
    geometry::box(
        horizontal ? StreetLength : StreetWidth,
        0.08f,
        horizontal ? StreetWidth : StreetLength,
        Asphalt,
        x,
        RoadY,
        z
    );
    for (float offset : { -4.0f, 0.0f, 4.0f })
        geometry::box(
            horizontal ? 1.45f : 0.09f,
            0.018f,
            horizontal ? 0.09f : 1.45f,
            Yellow,
            x + (horizontal ? offset : 0.0f),
            RoadY + 0.052f,
            z + (horizontal ? 0.0f : offset)
        );
    for (float side : { -1.0f, 1.0f })
        geometry::box(
            horizontal ? StreetLength - 0.3f : 0.065f,
            0.018f,
            horizontal ? 0.065f : StreetLength - 0.3f,
            White,
            x + (horizontal ? 0.0f : side * 2.42f),
            RoadY + 0.052f,
            z + (horizontal ? side * 2.42f : 0.0f)
        );
}

void crossing(float x, float z, bool horizontal) {
    for (int i = 0; i < 8; ++i) {
        const float offset = (i - 3.5f) * 0.55f;
        geometry::box(
            horizontal ? 0.72f : 0.34f,
            0.02f,
            horizontal ? 0.34f : 0.72f,
            White,
            x + (horizontal ? 0.0f : offset),
            RoadY + 0.055f,
            z + (horizontal ? offset : 0.0f)
        );
    }
}

void intersection(float x, float z, int column, int row, bool roundabout) {
    geometry::box(StreetWidth, 0.08f, StreetWidth, Asphalt, x, RoadY, z);
    if (roundabout) {
        geometry::cylinder(3.0f, 0.08f, Asphalt, x, RoadY, z);
        geometry::cylinder(1.24f, 0.18f, Paving, x, RoadY + 0.13f, z);
        geometry::cylinder(1.12f, 0.05f, { 75, 127, 68 }, x, RoadY + 0.245f, z);
        geometry::cylinder(0.43f, 0.54f, { 176, 166, 144 }, x, RoadY + 0.53f, z);
        geometry::sphere(0.32f, { 96, 157, 181 }, x, RoadY + 0.91f, z);
    } else if ((column % 2 == 1) && (row % 2 == 1)) {
        const float crossing_offset = StreetWidth / 2.0f + 0.72f;
        const float light = StreetWidth / 2.0f + 0.55f;
        crossing(x - crossing_offset, z, true);
        crossing(x + crossing_offset, z, true);
        crossing(x, z - crossing_offset, false);
        crossing(x, z + crossing_offset, false);
        const auto node = traffic::junction(column, row);
        traffic_light(x - light, z - light, 3.14159265f, false, node);
        traffic_light(x + light, z - light, 1.57079633f, true, node);
        traffic_light(x - light, z + light, -1.57079633f, true, node);
        traffic_light(x + light, z + light, 0.0f, false, node);
    }
}
} // namespace

void make_road_section(float x, float z, bool horizontal) { section(x, z, horizontal); }

void make_road_intersection(float x, float z, int column, int row, bool roundabout) {
    intersection(x, z, column, row, roundabout);
}

} // namespace gpu_city
