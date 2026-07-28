#include "siecs/cpp/entity.hpp"
#include "siecs/cpp/query.hpp"
#include "siecs/cpp/resource.hpp"
#include "siecs/cpp/system.hpp"
#include "siecs/cpp/world.hpp"
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <iostream>
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
    ecs::init({ .rest = true, .target_fps = 60 });

    ecs::entity::create<Voiture>().abstract();
    ecs::entity::create<Enemy>().is_a<Voiture>().abstract();

    ecs::entity entity = ecs::entity::create().is_a<Enemy>();

    auto sys = ecs::system().each([]() {});

    ecs::system().after(sys);

    if (entity.is<Enemy>() && entity.is<Voiture>()) {
        puts("ok");
    }

    ecs::entity::create();
}
