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
    ecs::init({ .rest = false, .target_fps = 60 });
    ecs::entity::create("Parent").child_of(ecs::entity::create("Child"));
    ecs::query().cascade<ecs::ChildOf>().each([](ecs::entity entity) {
        std::cout << entity.get<Name>().value;
    });
}
