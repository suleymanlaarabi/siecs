#include "siecs.h"
#include <iostream>
#include <siecs_cpp/siecs_cpp.hpp>

struct Player {};
struct Disabled {};

struct Position {
    float x, y;
};

struct Velocity {
    float x, y;
};

int main() {
    ecs::world world;

    world.entity().add<Position>().add<Player>().add<Disabled>().set(Velocity{ 10, 10 });
    world.entity().set(Position{ 10, 10 }).set(Velocity{ 10, 10 });

    world.query().each([](Position &pos, const Velocity &vel) {
        pos.x += vel.x;
        pos.y += vel.y;
    });

    return 0;
}
