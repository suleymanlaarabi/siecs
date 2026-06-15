#include <siecs_cpp/siecs_cpp.hpp>

static bool moved = false;

struct Position {
    float x;
    float y;
};

struct Velocity {
    float x;
    float y;
};

int main() {
    ecs::world world;

    auto entity = world.entity();
    entity.set(Position{ .x = 1.0f, .y = 2.0f });
    entity.set(Velocity{ .x = 3.0f, .y = 4.0f });

    world.system("Move").each([](Position &position, const Velocity &velocity) {
        position.x += velocity.x;
        position.y += velocity.y;
        moved = position.x == 4.0f && position.y == 6.0f;
    });

    world.progress();

    return moved ? 0 : 1;
}
