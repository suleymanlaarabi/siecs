#include "city.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <siecs/cpp/resource.hpp>
#include <siecs/cpp/system.hpp>
#include <siecs/cpp/world.hpp>

namespace {

struct FreeCameraController {};

void report_frames(ecs_iter_t *) {
    using clock = std::chrono::steady_clock;
    static auto last = clock::now();
    static uint32_t frames = 0;
    if (++frames == 30) {
        const auto now = clock::now();
        const double seconds = std::chrono::duration<double>(now - last).count();
        std::printf("city: %.1f frames/s over 30 frames\n", frames / seconds);
        std::fflush(stdout);
        last = now;
        frames = 0;
    }
}

} // namespace

int main() {
    ecs::init({ .target_fps = 120, .worker_threads = 0 });
    static_cast<void>(ecs::import<sigpu>(sigpu::props_t{
        .title = "SIECS GPU City",
        .width = 1280,
        .height = 800,
        .samples = 1,
    }));
    ecs::set_resource(AmbientLight{ Color{ 180, 195, 230 }, 0.45f });
    ecs::set_resource(Shadows{ true, 400.0f });
    ecs::set_resource(CameraClip{ 0.5f, 4000.0f });
    ecs::set_resource(Multisampling{ 2 });

    ecs::component<FreeCameraController>();
    ecs::component<gpu_city::Car>();
    gpu_city::traffic::register_components();
    gpu_city::register_traffic_scene();

    ecs::system("Camera controller")
        .each([](const FreeCameraController &,
                 Position3d &position,
                 Rotation3d &rotation,
                 ecs::res<const Keyboard> keyboard,
                 ecs::res<const DeltaTime> delta) {
            constexpr float move_speed = 8.0f;
            constexpr float rotation_speed = 1.5f;
            const float forward = float(keyboard->down(Key::S)) - float(keyboard->down(Key::W));
            const float strafe = float(keyboard->down(Key::A)) - float(keyboard->down(Key::D));
            const float vertical = float(keyboard->down(Key::E)) - float(keyboard->down(Key::Q));
            const float sin_yaw = std::sin(rotation.yaw), cos_yaw = std::cos(rotation.yaw);
            const float distance = move_speed * delta->value;
            position.x += (forward * sin_yaw + strafe * cos_yaw) * distance;
            position.y += vertical * distance;
            position.z += (forward * cos_yaw - strafe * sin_yaw) * distance;
            rotation.yaw += (float(keyboard->down(Key::Right)) - float(keyboard->down(Key::Left))) *
                            rotation_speed * delta->value;
            rotation.pitch -= (float(keyboard->down(Key::Down)) - float(keyboard->down(Key::Up))) *
                              rotation_speed * delta->value;
        });

    const bool aerial = std::getenv("GPU_CITY_AERIAL") != nullptr;
    ecs::entity::create("Camera").add<FreeCameraController>().set(
        aerial ? Position3d{ 9.0f, 70.0f, 35.0f } : Position3d{ 17.0f, 23.0f, 43.0f },
        aerial ? Rotation3d{ -1.45f, 0.0f, 0.0f } : Rotation3d{ -0.4f, 0.35f, 0.0f },
        Camera{ 70.0f }
    );

    const auto start = std::chrono::steady_clock::now();
    gpu_city::traffic::build_network();
    gpu_city::build_city();
    gpu_city::traffic::seed_cars(gpu_city::car_prefab());
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf(
        "city: %dx%d blocks generated in %.2f s\n",
        gpu_city::CitySize,
        gpu_city::CitySize,
        seconds
    );

    ecs_system_desc_t report = {};
    report.name = "City frame report";
    report.phase = EcsPostRender;
    report.callback = report_frames;
    report.main_thread_only = true;
    ecs_system_init(&report);
    ecs::run();
}
