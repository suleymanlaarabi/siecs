#include "city.hpp"

#include <cmath>

namespace gpu_city {
namespace {

class Window {
  public:
    static void
    make(float x, float y, float z, float yaw = 0.0f, float width = 0.82f, float height = 1.05f) {
        constexpr float frame = 0.1f;
        geometry::box(width, height, 0.1f, { 205, 194, 172 }, x, y, z, yaw);
        geometry::box(
            width - frame * 2,
            height - frame * 2,
            0.035f,
            { 63, 145, 174 },
            x,
            y,
            z + (yaw == 0.0f ? 0.06f : -0.06f),
            yaw
        );
        geometry::box(
            0.055f,
            height - frame * 2,
            0.04f,
            { 205, 194, 172 },
            x,
            y,
            z + (yaw == 0.0f ? 0.09f : -0.09f),
            yaw
        );
        geometry::box(
            width - frame * 2,
            0.055f,
            0.04f,
            { 205, 194, 172 },
            x,
            y,
            z + (yaw == 0.0f ? 0.09f : -0.09f),
            yaw
        );
    }
};

class Door {
  public:
    static void
    make(float x, float y, float z, float yaw = 0.0f, float width = 1.2f, float height = 2.1f) {
        const float facing = yaw == 0.0f ? 1.0f : -1.0f;
        geometry::box(width + 0.2f, height + 0.14f, 0.16f, { 211, 194, 163 }, x, y, z, yaw);
        geometry::box(width, height, 0.12f, { 150, 75, 0 }, x, y, z + facing * 0.07f, yaw);
        geometry::box(
            width * 0.72f,
            height * 0.3f,
            0.035f,
            { 112, 57, 26 },
            x,
            y + height * 0.24f,
            z + facing * 0.14f,
            yaw
        );
        geometry::box(
            0.1f,
            0.16f,
            0.1f,
            { 0, 0, 0 },
            x + (yaw == 0.0f ? 1.0f : -1.0f) * width * 0.36f,
            y,
            z + facing * 0.14f,
            yaw
        );
    }
};

class Building {
  public:
    static void make(float x, float z, int floors, Rgb facade, float yaw) {
        constexpr float width = 4.2f;
        constexpr float depth = 2.6f;
        constexpr float floor_height = 2.55f;
        const float height = floors * floor_height;
        const float front = depth / 2.0f + 0.06f;
        auto local = [x, z, yaw](float lx, float ly, float lz) {
            const float c = std::cos(yaw), s = std::sin(yaw);
            return Position3d{ x + lx * c + lz * s, ly, z - lx * s + lz * c };
        };
        auto box = [&](float w, float h, float d, Rgb color, float lx, float ly, float lz) {
            auto p = local(lx, ly, lz);
            geometry::box(w, h, d, color, p.x, p.y + PavementY, p.z, yaw);
        };

        box(width, height, depth, facade, 0.0f, height / 2.0f, 0.0f);
        for (int floor = 0; floor < floors; ++floor) {
            const float floor_bottom = floor * floor_height;
            const float window_y = floor_bottom + 1.38f;
            if (floor == 0) {
                auto door = local(0.0f, 1.05f, front);
                Door::make(door.x, door.y + PavementY, door.z, yaw);
                for (float wx : { -1.42f, 1.42f }) {
                    auto p = local(wx, window_y, front);
                    Window::make(p.x, p.y + PavementY, p.z, yaw, 0.78f, 1.05f);
                }
            } else {
                for (float wx : { -1.15f, 1.15f }) {
                    auto p = local(wx, window_y, front);
                    Window::make(p.x, p.y + PavementY, p.z, yaw);
                }
            }
            box(width + 0.12f,
                0.12f,
                depth + 0.12f,
                { shade(facade.r, 0.8f), shade(facade.g, 0.8f), shade(facade.b, 0.8f) },
                0.0f,
                floor_bottom + floor_height,
                0.0f);
        }
        box(width + 0.36f,
            0.24f,
            depth + 0.36f,
            { shade(facade.r, 0.7f), shade(facade.g, 0.7f), shade(facade.b, 0.7f) },
            0.0f,
            height + 0.12f,
            0.0f);
    }
};

class Park {
    static void tree(float x, float z, float y) {
        geometry::cylinder(0.14f, 1.35f, { 104, 68, 39 }, x, y + 0.72f, z);
        geometry::sphere(0.68f, { 47, 119, 61 }, x, y + 1.72f, z);
        geometry::sphere(0.43f, { 58, 137, 69 }, x - 0.36f, y + 1.53f, z + 0.08f);
        geometry::sphere(0.44f, { 52, 128, 64 }, x + 0.35f, y + 1.55f, z - 0.06f);
    }

  public:
    static void make(float x, float z) {
        const float y = PavementY;
        geometry::box(10.2f, 0.08f, 10.2f, { 74, 126, 69 }, x, y + 0.04f, z);
        geometry::box(1.05f, 0.035f, 9.9f, { 181, 172, 151 }, x, y + 0.095f, z);
        geometry::box(9.9f, 0.035f, 0.9f, { 181, 172, 151 }, x, y + 0.1f, z);
        for (float dx : { -2.12f, 2.12f })
            for (float dz : { -2.15f, 2.15f })
                tree(x + dx, z + dz, y);
        geometry::box(0.9f, 0.08f, 0.3f, { 132, 83, 43 }, x, y + 0.26f, z + 2.1f);
        geometry::box(0.9f, 0.34f, 0.08f, { 132, 83, 43 }, x, y + 0.4f, z + 1.98f);
        for (float dx : { -0.32f, 0.32f })
            geometry::box(0.08f, 0.26f, 0.22f, { 55, 59, 58 }, x + dx, y + 0.13f, z + 2.1f);
    }
};

} // namespace

void make_building(float x, float z, int floors, Rgb facade, float yaw) {
    Building::make(x, z, floors, facade, yaw);
}

void make_park(float x, float z) {
    Park::make(x, z);
}

} // namespace gpu_city
