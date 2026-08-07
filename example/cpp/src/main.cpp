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
    SiecsRestImport({});

    ecs::entity::create("Parent").child_of(ecs::entity::create("Child"));
    ecs::query().order_by_depth<ecs::ChildOf>().each([](ecs::entity entity) {
        std::cout << entity.get<Name>().value;
    });

    ecs::run();
}
