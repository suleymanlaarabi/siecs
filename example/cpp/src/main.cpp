#include "siecs/cpp/entity.hpp"
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

void my_scene() {
    ecs::entity::create().relate<Enemy>(ecs::entity::create());
    if (ecs_save("./my_scene")) {
        puts("saved");
    } else {
        puts("error");
    }
}

int main() {
    ecs::init({ .target_fps = 60 });
    ecs::import<sirest>();
    // ecs_load("./my_scene");
    // my_scene();
    ecs::run();
}
