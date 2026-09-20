#include <siecs/cpp/entity.hpp>
#include <siecs/cpp/world.hpp>
#include <sigpu.h>

int main() {
    ecs::init();
    static_cast<void>(ecs::import<sigpu>(sigpu::props_t{
        .title = "SIECS + sigpu",
        .width = 1280,
        .height = 720,
        .samples = 4,
    }));

    ecs::set_resource(Sky{ Color{ 12, 18, 34 } });
    ecs::set_resource(Sun{ -0.45f, -1.0f, 0.35f, Color{ 255, 232, 196 }, 2.5f });
    ecs::set_resource(AmbientLight{ Color{ 155, 180, 255 }, 0.18f });
    ecs::set_resource(Fog{ Color{ 12, 18, 34 }, 24.0f, 65.0f });
    ecs::set_resource(Shadows{ true, 50.0f });
    ecs::set_resource(BloomSettings{ true, 0.85f, 0.7f });

    ecs::entity::create("Camera")
        .set(Position3d{ 0.0f, 3.5f, 14.0f }, Rotation3d{ -0.24f, 0.0f, 0.0f }, Camera{ 58.0f });

    ecs::entity::create("Ground")
        .set(Position3d{ 0.0f, -1.2f, 0.0f }, Cuboid{ 22.0f, 0.25f, 18.0f }, Color{ 30, 41, 64 })
        .add<Static>();

    ecs::entity::create("Orange cuboid")
        .set(
            Position3d{ -3.2f, 0.0f, 0.0f },
            Rotation3d{ 0.2f, -0.45f, 0.0f },
            Cuboid{ 2.2f, 2.2f, 2.2f },
            Color{ 255, 126, 56 }
        );

    ecs::entity::create("Blue cuboid")
        .set(
            Position3d{ 0.0f, 0.4f, -1.2f },
            Rotation3d{ -0.15f, 0.25f, 0.1f },
            Cuboid{ 2.6f, 3.0f, 1.6f },
            Color{ 73, 145, 255 }
        );

    ecs::entity::create("Purple cuboid")
        .set(
            Position3d{ 3.4f, -0.1f, 0.3f },
            Rotation3d{ 0.28f, 0.55f, -0.1f },
            Cuboid{ 1.8f, 1.8f, 1.8f },
            Color{ 188, 82, 255 }
        );

    ecs::entity::create("Emissive cuboid")
        .set(
            Position3d{ 0.0f, 3.0f, 0.0f },
            Rotation3d{ 0.0f, 0.4f, 0.0f },
            Cuboid{ 0.75f, 0.75f, 0.75f },
            Color{ 255, 220, 110 },
            Bloom{ 3.0f }
        );

    ecs::run();
}
