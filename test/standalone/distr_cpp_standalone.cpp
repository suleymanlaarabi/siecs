#include <siecs.h>

#include <cassert>

struct Position {
    float x, y;
};

struct Velocity {
    float x, y;
};

int main() {
    ecs::init();

    ecs::entity entity = ecs::entity::create().set(Position{ 1.0f, 2.0f }).set(Velocity{ 3.0f, 4.0f });

    ecs::system("Move").each([](Position &pos, const Velocity &vel) {
        pos.x += vel.x;
        pos.y += vel.y;
    });

    ecs::progress();

    const Position *position = static_cast<const Position *>(
        ecs_get_cid(entity.id(), ecs::component<Position>())
    );

    assert(position != nullptr);
    assert(position->x == 4.0f);
    assert(position->y == 6.0f);
    ecs::fini();
}
