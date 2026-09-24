#include "city.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <siecs/cpp/query.hpp>
#include <siecs/cpp/system.hpp>

namespace gpu_city {
namespace {
void update_lamps() {
    constexpr Rgb colors[] = { { 255, 35, 25 }, { 255, 177, 18 }, { 35, 230, 75 } };
    ecs::query().with_relation<ControlsJunction>().each(
        [&](ecs::entity entity, SignalLamp &lamp, Color &color, Bloom &bloom) {
            const auto node = entity.target<ControlsJunction>();
            const auto signal = traffic::light(node.get<traffic::Junction>(),
                                               lamp.axis ? traffic::South : traffic::East);
            const bool lit = lamp.role == (signal == traffic::Green ? SignalGreen
                                             : signal == traffic::Amber ? SignalAmber : SignalRed);
            if (lit == lamp.lit)
                return;
            lamp.lit = lit;
            const Rgb rgb = colors[lamp.role];
            color = { uint8_t(lit ? rgb.r : rgb.r * 0.16f),
                      uint8_t(lit ? rgb.g : rgb.g * 0.16f),
                      uint8_t(lit ? rgb.b : rgb.b * 0.16f) };
            bloom = Bloom{ lit ? 0.9f : 0.0f };
        });
}
} // namespace

void register_traffic_scene() {
    ecs::component<SignalLamp>();
    ecs::relation<ControlsJunction>();
    ecs::system("City traffic")
        .phase(EcsPreUpdate).immediate().no_defer()
        .each([](ecs::res<traffic::Clock> clock, ecs::res<const DeltaTime> delta) {
            clock->accumulator = std::min(clock->accumulator + delta->value, 0.25f);
            while (clock->accumulator >= traffic::StepSeconds) {
                const auto start = std::chrono::steady_clock::now();
                traffic::step();
                update_lamps();
                clock->accumulator -= traffic::StepSeconds;
                if (std::getenv("SIGPU_PROFILE")) {
                    const double ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - start).count();
                    std::fprintf(stderr, "gpu_city traffic step %.3f ms\n", ms);
                }
            }
            traffic::render(clock->accumulator / traffic::StepSeconds);
        });
}

} // namespace gpu_city
