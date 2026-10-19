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

    ecs::component<Velocity>().with<Position>();

    ecs::entity entity = ecs::entity::create().set(Velocity{ 3.0f, 4.0f });

    ecs::system("Move").each([](Position &pos, const Velocity &vel) {
        pos.x += vel.x;
        pos.y += vel.y;
    });

    ecs::progress();

    const Position *position =
        static_cast<const Position *>(ecs_get_cid(entity.id(), ecs::component<Position>()));

    assert(position != nullptr);
    assert(position->x == 3.0f);
    assert(position->y == 4.0f);

    auto first = ecs::entity::create("SceneA::Player");
    auto second = ecs::entity::create("SceneB::Player");
    auto scene = ecs::entity::lookup("SceneA");
    assert(first.id() != second.id());
    assert(ecs::entity::lookup(scene, "Player").id() == first.id());
    assert(first.path() == "::SceneA::Player");
    auto replacement = ecs::entity::create(scene, "Replacement");
    assert(replacement.set_name("Player"));
    assert(!first.is_alive());
    assert(second.is_alive());
    assert(ecs::entity::lookup("SceneA::Player").id() == replacement.id());
    ecs::fini();
}
