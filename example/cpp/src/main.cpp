#include "siecs/cpp/entity.hpp"
#include "siecs/cpp/query.hpp"
#include "siecs/cpp/system.hpp"
#include "siecs/cpp/world.hpp"
#include <concepts>
#include <cstdint>
#include <cstdio>
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
struct NoIntegrate {};

int main() {
    ecs::init({ .rest = true, .target_fps = 60 });

    ecs::entity::create<Enemy>().abstract();

    ecs::run();

    ecs::fini();
}
