#include <siecs.h>

#include <cassert>

struct Position {
    float x, y;
};

struct Velocity {
    float x, y;
};

int main() {
    ecs::world world;

    ecs::entity entity = world.entity().set(Position{ 1.0f, 2.0f }).set(Velocity{ 3.0f, 4.0f });

    world.system("Move").each([](Position &pos, const Velocity &vel) {
        pos.x += vel.x;
        pos.y += vel.y;
    });

    world.progress();

    const Position *position = static_cast<const Position *>(
        ecs_get_cid(world.c_ptr(), entity.id(), world.component<Position>())
    );

    assert(position != nullptr);
    assert(position->x == 4.0f);
    assert(position->y == 6.0f);
}
