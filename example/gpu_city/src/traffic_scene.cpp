#include "city.hpp"

#include <siecs/cpp/system.hpp>

namespace gpu_city {

void register_traffic_scene() {
    ecs::component<SignalLamp>();
    ecs::relation<ControlsJunction>();
    traffic::register_systems();

    ecs::system("City signal lamps")
        .phase(EcsPreRender)
        .interval(traffic::StepSeconds)
        .with_relation<ControlsJunction>()
        .each([](ecs::entity lamp_entity, SignalLamp &lamp, Color &color, Bloom &bloom) {
            const auto node = lamp_entity.target<ControlsJunction>();
            const auto signal = traffic::light(
                node.get<traffic::Junction>(),
                lamp.axis ? traffic::South : traffic::East
            );
            const bool lit = lamp.role == (signal == traffic::Green   ? SignalGreen
                                           : signal == traffic::Amber ? SignalAmber
                                                                      : SignalRed);
            if (lamp.lit == lit)
                return;

            constexpr Rgb colors[] = { { 255, 35, 25 }, { 255, 177, 18 }, { 35, 230, 75 } };
            lamp.lit = lit;
            const Rgb rgb = colors[lamp.role];
            color = { uint8_t(lit ? rgb.r : rgb.r * 0.16f),
                      uint8_t(lit ? rgb.g : rgb.g * 0.16f),
                      uint8_t(lit ? rgb.b : rgb.b * 0.16f) };
            bloom = Bloom{ lit ? 0.9f : 0.0f };
        });
}

} // namespace gpu_city
