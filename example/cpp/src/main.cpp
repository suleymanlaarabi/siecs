#include "siecs/cpp/world.hpp"
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <siecs.h>
#include <siecs_rest.h>

#include <cassert>

struct Position {
    reflected(float x, y;)
};

struct Velocity {
    reflected(float x, y;)
};

struct Gravity {
    float value;
};

struct Enemy {};
struct Voiture {};
struct NoIntegrate {};


int main() {
    ecs::init({ .target_fps = 60 });
    ecs::import<sirest>();


    ecs::run();
}
