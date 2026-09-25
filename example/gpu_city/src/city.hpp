#pragma once

#include "traffic.hpp"
#include <cstdint>
#include <siecs/cpp/entity.hpp>
#include <sigpu.h>

namespace gpu_city {

inline constexpr int CitySize = traffic::Grid;
inline constexpr float BlockSpacing = traffic::Spacing;
inline constexpr float StreetWidth = 5.4f;
inline constexpr float StreetLength = BlockSpacing - StreetWidth;
inline constexpr float RoadY = -1.3f;
inline constexpr float PavementY = -1.16f;
inline constexpr float GroundSize = (CitySize - 1) * BlockSpacing + StreetWidth + 70.0f;

struct Rgb {
    uint8_t r, g, b;
};

enum SignalColor : uint8_t { SignalRed, SignalAmber, SignalGreen };

struct SignalLamp {
    uint8_t role, axis;
    bool lit;
};

struct Car {
    float length, width;
};

struct ControlsJunction {};

inline constexpr Rgb Asphalt{ 43, 47, 52 };
inline constexpr Rgb Paving{ 151, 153, 149 };
inline constexpr Rgb White{ 233, 230, 216 };
inline constexpr Rgb Yellow{ 237, 205, 111 };
inline constexpr Rgb Facades[] = {
    { 155, 133, 108 }, { 178, 163, 136 }, { 130, 145, 154 }, { 184, 145, 121 }, { 147, 154, 129 },
};

uint8_t shade(uint8_t value, float factor);

namespace geometry {
ecs::entity
box(float width,
    float height,
    float depth,
    Rgb color,
    float x,
    float y,
    float z,
    float yaw = 0,
    bool is_static = true);

ecs::entity cylinder(
    float radius,
    float height,
    Rgb color,
    float x,
    float y,
    float z,
    float yaw = 0,
    bool is_static = true
);

ecs::entity sphere(float radius, Rgb color, float x, float y, float z);

ecs::entity signal_sphere(
    float radius,
    Rgb color,
    float x,
    float y,
    float z,
    SignalLamp lamp,
    ecs::entity junction
);

} // namespace geometry

void make_building(float x, float z, int floors, Rgb facade, float yaw);
void make_park(float x, float z);
void make_road_section(float x, float z, bool horizontal);
void make_road_intersection(float x, float z, int column, int row, bool roundabout);
void build_city();
ecs::entity car_prefab();
void register_traffic_scene();

} // namespace gpu_city
