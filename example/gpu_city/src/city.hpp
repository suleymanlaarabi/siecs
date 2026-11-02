#pragma once

#include "traffic.hpp"
#include <cstdint>
#include <siecs/cpp/entity.hpp>
#include <sigpu.h>

struct CityLayout {
    reflected(int32_t size; float spacing; float street_width; float road_y; float pavement_y;)
};

namespace gpu_city {

inline constexpr int CitySize = traffic::Grid;
inline constexpr float BlockSpacing = traffic::Spacing;
inline constexpr float StreetWidth = 5.4f;
inline constexpr float RoadY = -1.3f;
inline constexpr float PavementY = -1.16f;

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

bool build_city(const char *filename = "city.si");
void build_traffic_lights();
ecs::entity car_prefab();
void register_traffic_scene();

} // namespace gpu_city
