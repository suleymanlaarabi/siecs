#include "siecs.h"
#include <cstdio>
#include <siecs_cpp/siecs_cpp.hpp>

struct Player {};
struct Disabled {};

struct Position {
    float x, y;
};

struct Velocity {
    float x, y;
};

int main(int argc, char *argv[]) {
    ecs::world world;

    world.entity().add<Position>().add<Player>().add<Disabled>().set(Velocity{ 10, 10 });

    return 0;
}
