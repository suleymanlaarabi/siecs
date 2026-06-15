#include "siecs.h"
#include <cstdint>
#include <iostream>
#include <siecs_cpp/siecs_cpp.hpp>

struct Position {
    float x, y;
};

struct Velocity {
    float x, y;
};

int main() {
    ecs::world world;

    world.entity().set(Position{ 0, 0 }).set(Velocity{ 10, 10 });

    world.system("Move").phase(EcsOnUpdate).each([](Position &pos, const Velocity &vel) {
        pos.x += vel.x;
        pos.y += vel.y;

        std::cout << pos.x << "\n";
    });

    world.progress();
}
