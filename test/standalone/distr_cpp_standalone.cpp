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

    assert(ecs_component_lookup("Position") == ecs::component<Position>().id());
    assert(ecs_relation_lookup("ChildOf") == ecs_rid(ChildOf));
    assert(ecs_component_lookup("ChildOf") == 0);
    assert(ecs_component_lookup(nullptr) == 0);

    ecs::entity entity = ecs::spawn().set(Velocity{ 3.0f, 4.0f });

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

    auto first = ecs::ensure("SceneA::Player");
    auto second = ecs::ensure("SceneB::Player");
    auto scene = ecs::entity::lookup("SceneA");
    assert(first.id() != second.id());
    assert(ecs::entity::lookup(scene, "Player").id() == first.id());
    assert(first.path() == "::SceneA::Player");
    auto replacement = ecs::ensure(scene, "Replacement");
    assert(replacement.set_name("Player"));
    assert(!first.is_alive());
    assert(second.is_alive());
    assert(ecs::entity::lookup("SceneA::Player").id() == replacement.id());
    ecs::fini();
}
