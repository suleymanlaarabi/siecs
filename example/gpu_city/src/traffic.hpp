#pragma once

#include <cstdint>
#include <siecs/cpp/world.hpp>
#include <siecs_spatial.h>

namespace gpu_city::traffic {

inline constexpr int Grid = 64;
inline constexpr int Capacity = 800;
inline constexpr float Spacing = 18.0f;
inline constexpr float LaneOffset = 1.2f;
inline constexpr float HalfStreet = 2.7f;
inline constexpr float EdgeLength = Spacing - 2 * HalfStreet;
inline constexpr float VehicleLength = 3.655f;
inline constexpr float StepSeconds = 1.0f / 30.0f;

enum Direction : uint8_t { East, South, West, North };
enum Stage : uint8_t { Road, Intersection, Exit };
enum Light : uint8_t { Green, Amber, Red };

struct Pose {
    float x, z, yaw;
};
struct Junction {
    uint8_t column, row;
    bool roundabout, signal;
    float cycle;
};
struct Lane {
    uint8_t direction;
};
struct Motion {
    float progress = 0, old_progress = 0, speed = 0;
    uint32_t arrival = 0;
    uint8_t incoming = 0, outgoing = 0;
    Stage stage = Road;
    Pose previous{}, current{};
};
struct Clock {
    float accumulator = 0;
    uint32_t tick = 0, random = 0x4a831f53u;
    uint32_t exits = 0, entries = 0;
};
struct OnLane {};
struct FromJunction {};
struct ToJunction {};
struct Crossing {};
struct NextLane {};
struct Waiting {};

void register_components();
void build_network();
ecs::entity junction(int column, int row);
ecs::entity lane_from(ecs::entity junction, int direction);
bool place_car(ecs::entity car, ecs::entity lane, float progress);
void seed_cars(ecs::entity prefab, int count = Capacity);
void step();
void render(float alpha);
Light light(const Junction &junction, int direction);

} // namespace gpu_city::traffic
