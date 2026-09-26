#include "city.hpp"

namespace gpu_city {
namespace {

constexpr float CarLength = 4.3f;
constexpr float CarWidth = 2.185f;
constexpr float BodyWidth = 1.82f;
constexpr float CarScale = 0.85f;
constexpr float WheelRadius = 0.36f;
constexpr float HalfPi = 1.57079633f;

constexpr Rgb Paint{ 52, 116, 177 };
constexpr Rgb DoorPaint{ 57, 123, 184 };
constexpr Rgb Glass{ 44, 83, 105 };
constexpr Rgb Tire{ 24, 27, 30 };
constexpr Rgb Metal{ 153, 161, 165 };

void box(ecs::entity prefab, float w, float h, float d, Rgb color, float x, float y, float z) {
    geometry::box(w, h, d, color, x, y, z, 0.0f, false).child_of(prefab);
}

void wheel(ecs::entity prefab, float x, float z) {
    geometry::cylinder(WheelRadius, 0.16f, Tire, x, WheelRadius, z, 0.0f, false)
        .set(Rotation3d{ 0.0f, 0.0f, HalfPi })
        .child_of(prefab);
}

} // namespace

ecs::entity car_prefab() {
    auto car = ecs::entity::create().set(
        Car{ CarLength * CarScale, CarWidth * CarScale },
        Scale3d{ CarScale, CarScale, CarScale }
    );

    box(car, BodyWidth, 0.58f, 4.08f, Paint, 0.0f, 0.67f, 0.0f);
    box(car, 1.72f, 0.12f, 1.08f, DoorPaint, 0.0f, 0.99f, -1.45f);
    box(car, 1.72f, 0.10f, 0.72f, DoorPaint, 0.0f, 0.98f, 1.62f);

    box(car, 1.52f, 0.53f, 2.02f, Paint, 0.0f, 1.25f, 0.08f);
    box(car, 1.43f, 0.42f, 0.035f, Glass, 0.0f, 1.25f, -0.95f);
    box(car, 1.43f, 0.42f, 0.035f, Glass, 0.0f, 1.25f, 1.11f);

    for (float side : { -1.0f, 1.0f }) {
        const float sx = side * 0.78f;
        for (float window_z : { -0.46f, 0.55f })
            box(car, 0.035f, 0.42f, 0.77f, Glass, sx, 1.25f, window_z);

        for (float door_z : { -0.59f, 0.59f }) {
            box(car, 0.035f, 0.46f, 1.10f, DoorPaint, side * 0.93f, 0.72f, door_z);
            box(car, 0.045f, 0.045f, 0.22f, Metal, side * 0.96f, 0.84f, door_z + 0.23f);
        }
        box(car, 0.18f, 0.10f, 0.20f, Tire, side * 0.90f, 1.08f, -0.65f);

        for (float wheel_z : { -1.32f, 1.32f })
            wheel(car, side * 0.99f, wheel_z);

        box(car, 0.34f, 0.17f, 0.055f, { 240, 225, 172 }, side * 0.60f, 0.77f, -2.08f);
        box(car, 0.33f, 0.17f, 0.055f, { 177, 42, 36 }, side * 0.60f, 0.77f, 2.08f);
    }

    return car.abstract();
}

} // namespace gpu_city
