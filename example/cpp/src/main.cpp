#include "siecs.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <siecs_cpp/siecs_cpp.hpp>

struct Position {
    float x, y;
};

struct Velocity {
    float x, y;
};

struct Time {
    float delta;
};

struct physics {
    void import(ecs::world &world) {
        world.component<Position>();
        world.component<Velocity>();

        world.system("Move")
            .phase(EcsOnUpdate)
            .each([](Position &pos, const Velocity &vel, ecs::res<const Time> time) {
                pos.x += vel.x * time->delta;
                pos.y += vel.y * time->delta;

                std::cout << pos.x << "\n";
            });
    }
};

int main() {
    ecs::world world;

    world.set_resource(Time(16));

    world.import<physics>();

    ecs::entity human = world.entity().add<Position>().abstract();

    ecs::entity parent = world.entity().is_a(human);

    world.observe<ecs::OnAdd>().each([](Position &pos) {
        pos.x = 0;
        pos.y = 0;
    });

    world.entity().set(Position{ 0, 0 }).set(Velocity{ 10, 10 }).child_of(parent);

    world.module<physics>().disable();
    world.module<physics>().enable();
    world.progress();
}
