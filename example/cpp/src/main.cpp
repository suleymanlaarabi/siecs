#include "siecs.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <siecs_cpp/siecs_cpp.hpp>

struct Position {
    reflected(float x; float y;)
};

struct Velocity : Position {};

struct Time {
    float delta;
};

struct IsKind {};

int main() {
    ecs::world world;

    for (int i = 0; i < 10; i++) {
        world.entity().add<Position>().add<Velocity>();
    }

    auto enemy = world.entity().add<Position>().add<Velocity>().abstract();

    world.entity("enemy").is_a(enemy);

    int count = 0;
    world.query().is_a(enemy).each([&](Position &pos, const Velocity &vel) { count += 1; });

    assert(count == 10);
    while (world.progress()) {
    };
}
