#include "siecs.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
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

int main() {
    ecs::world world;

    for (int i = 0; i < 10; i++) {
        world.entity().add<Position>().add<Velocity>();
    }

    auto enemy = world.entity().add<Position>().add<Velocity>().abstract();

    world.entity("enemy").is_a(enemy);

    int count = 0;
    world.query().each([&](Position &pos, const Velocity &vel) { count += 1; });

    assert(count == 10);
    while (world.progress()) {
    };
}
