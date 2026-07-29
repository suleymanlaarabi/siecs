#include "siecs/cpp/entity.hpp"
#include "siecs/cpp/query.hpp"
#include "siecs/cpp/resource.hpp"
#include "siecs/cpp/system.hpp"
#include "siecs/cpp/world.hpp"
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <print>
#include <siecs.h>

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
    ecs::init({ .rest = false, .target_fps = 60 });

    std::println("{}", ecs::entity::lookup("Enemy").id());

    ecs::entity::create<Enemy>();
    ecs::entity::create("Enemy");

    std::println("{}", ecs::entity::lookup("Enemy").id());
}
