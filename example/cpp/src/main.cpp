#include <concepts>
#include <cstdint>
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

struct Enemy;
struct NoIntegrate;

template <typename... T> struct With {};

template <typename... T> struct Without {};

template <typename T> struct IsA {};

template <typename With, typename Without = Without<>> struct Query {};

int main() {
    ecs::init();

    ecs::entity::create<Enemy>();

    ecs::entity::create().is_a<Enemy>();

    ecs::system().require<Position>();

    ecs::system().require<Enemy>().each([](Position &pos, const Velocity &vel) { pos.x += vel.x; });

    ecs::system().exclude<Enemy>().each([](Velocity &vitesse, const Gravity &gravity) {
        vitesse.y += gravity.value;
    });

    ecs::entity::create().add<Position, Velocity>();

    while (ecs::progress()) {
    };
}
