#include "siecs.h"
#include "siecs/cpp/query.hpp"
#include "siecs/cpp/resource.hpp"
#include "siecs/cpp/system.hpp"
#include "siecs_spatial.h"
#include <concepts>
#include <cstdint>
#include <execution>
#include <siecs/cpp/entity.hpp>
#include <siecs/cpp/world.hpp>
#include <sirender3d.h>

int main() {
    ecs::init();

    ecs::import<siwindow>();
    ecs::ensure("Window").set(Window{
        .slot = "main",
        .title = "SIECS + sigpu",
        .width = 1280,
        .height = 720,
    });
    ecs::import<sigpu>();
    ecs::import<sirender3d>();
    ecs::ensure("Device").set(GpuDevice{});
    auto surface = ecs::ensure("Surface").set(GpuSurface{ .window_slot = "main" });

    ecs::ensure("Sun").set(DirectionalLight{ -0.45f, -1.0f, 0.35f, Color{ 255, 232, 196 }, 1.5f });
    ecs::ensure("Camera")
        .set(Position3d{ 0.0f, 3.5f, 14.0f }, Rotation3d{ -0.24f, 0.0f, 0.0f }, Camera3d{ 58.0f })
        .set(
            Sky{ Color{ 12, 18, 34 } },
            AmbientLight{ Color{ 155, 180, 255 }, 0.24f },
            Fog{ Color{ 12, 18, 34 }, 24.0f, 65.0f },
            Shadows{ true, 50.0f }
        )
        .set(BloomSettings{ true, 0.85f, 0.7f }, Multisampling{ 4 })
        .relate<RendersTo>(surface);

    ecs::ensure("Ground")
        .set(Position3d{ 0.0f, -1.2f, 0.0f }, Cuboid{ 22.0f, 0.25f, 18.0f }, Color{ 30, 41, 64 })
        .add<Static>();

    ecs::ensure("Static cylinder")
        .set(
            Position3d{ -7.0f, -0.075f, -2.0f },
            Cylinder{ 0.65f, 2.0f },
            Color{ 95, 205, 175 },
            PointerEvents{ SiPointerEnterMask | SiClickMask }
        )
        .add<Static>();
    ecs::ensure("Static sphere")
        .set(
            Position3d{ 7.0f, -0.075f, -2.0f },
            Sphere{ 1.0f },
            Color{ 245, 155, 95 },
            PointerEvents{ SiPointerEnterMask | SiClickMask }
        )
        .add<Static>();

    ecs::ensure("Orange cuboid")
        .set(
            Position3d{ -3.2f, 0.0f, 0.0f },
            Rotation3d{ 0.2f, -0.45f, 0.0f },
            Cuboid{ 2.2f, 2.2f, 2.2f },
            Color{ 255, 126, 56 },
            PointerEvents{ SiPointerEnterMask | SiClickMask }
        );

    ecs::ensure("Rotated blooming cylinder")
        .set(
            Position3d{ -7.0f, 0.0f, 3.0f },
            Rotation3d{ 0.4f, 0.2f, 0.1f },
            Cylinder{ 0.6f, 1.7f },
            Color{ 90, 235, 255 },
            Bloom{ 0.25f },
            PointerEvents{ SiPointerEnterMask | SiClickMask }
        );
    ecs::ensure("Rotated blooming ellipsoid")
        .set(
            Position3d{ 7.0f, -0.15f, 3.0f },
            Rotation3d{ 0.25f, 0.45f, 0.0f },
            Scale3d{ 1.1f, 0.95f, 1.0f },
            Sphere{ 0.9f },
            Color{ 255, 90, 180 },
            Bloom{ 0.25f },
            PointerEvents{ SiPointerEnterMask | SiClickMask }
        );

    ecs::ensure("Blue cuboid")
        .set(
            Position3d{ 0.0f, 0.4f, -1.2f },
            Rotation3d{ -0.15f, 0.25f, 0.1f },
            Cuboid{ 2.6f, 3.0f, 1.6f },
            Color{ 73, 145, 255 }
        );

    ecs::ensure("Purple cuboid")
        .set(
            Position3d{ 3.4f, -0.1f, 0.3f },
            Rotation3d{ 0.28f, 0.55f, -0.1f },
            Cuboid{ 1.8f, 1.8f, 1.8f },
            Color{ 188, 82, 255 }
        );

    ecs::ensure("Emissive cuboid")
        .set(
            Position3d{ 0.0f, 3.0f, 0.0f },
            Rotation3d{ 0.0f, 0.4f, 0.0f },
            Cuboid{ 0.75f, 0.75f, 0.75f },
            Color{ 255, 220, 110 },
            Bloom{ 3.0f }
        );

    ecs::run();
}
