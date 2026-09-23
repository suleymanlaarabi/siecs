#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <siecs/cpp/entity.hpp>
#include <siecs/cpp/resource.hpp>
#include <siecs/cpp/system.hpp>
#include <siecs/cpp/world.hpp>
#include <siecs_spatial.h>
#include <sigpu.h>

struct FreeCameraController {};

static void report_frames(ecs_iter_t *) {
    using clock = std::chrono::steady_clock;
    static auto last = clock::now();
    static uint32_t frames = 0;
    if (++frames == 30) {
        auto now = clock::now();
        double seconds = std::chrono::duration<double>(now - last).count();
        std::printf("city: %.1f frames/s over 30 frames\n", frames / seconds);
        std::fflush(stdout);
        last = now;
        frames = 0;
    }
}

int main() {
    ecs::init();
    static_cast<void>(ecs::import<sigpu>(sigpu::props_t{
        .title = "sigpu city: 1,000,000 static primitives",
        .width = 1280,
        .height = 720,
        .samples = 1 }));
    ecs::set_resource(CameraClip{ 0.5f, 4000.0f });
    ecs::set_resource(Shadows{ false, 100.0f });
    ecs::set_resource(Fog{ Color{ 20, 27, 40 }, 500.0f, 3500.0f });
    ecs::component<FreeCameraController>();
    ecs::system("CityFreeCamera")
        .phase(EcsPreUpdate)
        .each([](const FreeCameraController &,
                 Position3d &position,
                 Rotation3d &rotation,
                 ecs::res<const Keyboard> keyboard,
                 ecs::res<const DeltaTime> delta) {
            constexpr float move_speed = 35.0f;
            constexpr float rotation_speed = 1.5f;
            const float forward = static_cast<float>(keyboard->down(Key::S)) -
                                  static_cast<float>(keyboard->down(Key::W));
            const float strafe = static_cast<float>(keyboard->down(Key::A)) -
                                 static_cast<float>(keyboard->down(Key::D));
            const float vertical = static_cast<float>(keyboard->down(Key::E)) -
                                   static_cast<float>(keyboard->down(Key::Q));
            const float yaw = rotation.yaw;
            const float sin_yaw = std::sin(yaw);
            const float cos_yaw = std::cos(yaw);

            position.x += (forward * sin_yaw + strafe * cos_yaw) * move_speed * delta->value;
            position.y += vertical * move_speed * delta->value;
            position.z += (forward * cos_yaw - strafe * sin_yaw) * move_speed * delta->value;
            rotation.yaw += (static_cast<float>(keyboard->down(Key::Right)) -
                             static_cast<float>(keyboard->down(Key::Left))) *
                            rotation_speed * delta->value;
            rotation.pitch -= (static_cast<float>(keyboard->down(Key::Down)) -
                               static_cast<float>(keyboard->down(Key::Up))) *
                              rotation_speed * delta->value;
        });
    ecs::entity::create("City camera")
        .add<FreeCameraController>()
        .set(Position3d{ 0.0f, 100.0f, 300.0f }, Rotation3d{ -0.15f, 0.0f, 0.0f }, Camera{ 65.0f });

    // The default is exactly 500k cuboids, 250k cylinders, 250k spheres.
    // Override the count for quick local runs without changing the scene.
    uint32_t count = 1000000;
    if (const char *value = std::getenv("SIGPU_CITY_OBJECTS")) {
        unsigned long requested = std::strtoul(value, nullptr, 10);
        if (requested > 0 && requested <= 1000000)
            count = static_cast<uint32_t>(requested);
    }
    auto start = std::chrono::steady_clock::now();
    for (uint32_t i = 0; i < count; i++) {
        float x = (static_cast<float>(i % 1000) - 500.0f) * 3.0f;
        float z = -static_cast<float>(i / 1000) * 3.0f;
        float height = 2.0f + static_cast<float>((i * 17u) % 13u);
        auto entity = ecs::entity::create();
        if (i % 4u < 2u) {
            entity.set(
                Position3d{ x, height * 0.5f, z },
                Cuboid{ 2.0f, height, 2.0f },
                Color{ uint8_t(80 + i % 70u), uint8_t(95 + i % 60u), uint8_t(115 + i % 75u) }
            );
        } else if (i % 4u == 2u) {
            entity.set(
                Position3d{ x, height * 0.5f, z },
                Cylinder{ 0.75f, height },
                Color{ 120, 170, 145 }
            );
        } else {
            entity.set(Position3d{ x, height + 0.5f, z }, Sphere{ 0.9f }, Color{ 175, 125, 95 });
        }
        entity.add<Static>();
    }
    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("city: %u static primitives created in %.3f s\n", count, elapsed);
    ecs_system_desc_t report = {};
    report.name = "CityFrameReport";
    report.phase = EcsPostRender;
    report.callback = report_frames;
    report.main_thread_only = true;
    ecs_system_init(&report);
    ecs::run();
}
