#include <siecs_spatial.h>
#include <test.h>

void spatial_component_requirements_and_defaults(void) {
    ecs::init();
    ecs::import<sispatial>();

    ecs::entity position_entity = ecs::entity::create().set(Position{10.0f, 20.0f});
    test_assert(position_entity.has<GlobalPosition>());

    ecs::entity scale_entity = ecs::entity::create().add<Scale>();
    test_assert(scale_entity.has<GlobalScale>());
    test_assert(scale_entity.get<Scale>().x == 1.0f);
    test_assert(scale_entity.get<Scale>().y == 1.0f);
    test_assert(scale_entity.get<GlobalScale>().x == 1.0f);
    test_assert(scale_entity.get<GlobalScale>().y == 1.0f);

    ecs::entity rotation_entity = ecs::entity::create().set(Rotation{3.0f});
    test_assert(rotation_entity.has<GlobalRotation>());

    ecs::fini();
}

void spatial_position_hierarchy(void) {
    ecs::init();
    ecs::import<sispatial>();

    ecs::entity root = ecs::entity::create().set(Position{10.0f, 10.0f});
    ecs::entity child = ecs::entity::create().set(Position{5.0f, 5.0f}).child_of(root);
    ecs::entity grandchild = ecs::entity::create().set(Position{2.0f, 3.0f}).child_of(child);

    ecs::progress();

    test_assert(root.get<GlobalPosition>().x == 10.0f);
    test_assert(root.get<GlobalPosition>().y == 10.0f);
    test_assert(child.get<GlobalPosition>().x == 15.0f);
    test_assert(child.get<GlobalPosition>().y == 15.0f);
    test_assert(grandchild.get<GlobalPosition>().x == 17.0f);
    test_assert(grandchild.get<GlobalPosition>().y == 18.0f);

    ecs::fini();
}

void spatial_rotation_hierarchy(void) {
    ecs::init();
    ecs::import<sispatial>();

    ecs::entity root = ecs::entity::create().set(Rotation{10.0f});
    ecs::entity child = ecs::entity::create().set(Rotation{20.0f}).child_of(root);
    ecs::entity grandchild = ecs::entity::create().set(Rotation{5.0f}).child_of(child);

    ecs::progress();

    test_assert(root.get<GlobalRotation>().value == 10.0f);
    test_assert(child.get<GlobalRotation>().value == 30.0f);
    test_assert(grandchild.get<GlobalRotation>().value == 35.0f);

    ecs::fini();
}

void spatial_scale_hierarchy(void) {
    ecs::init();
    ecs::import<sispatial>();

    ecs::entity root = ecs::entity::create().set(Scale{2.0f, 3.0f});
    ecs::entity child = ecs::entity::create().set(Scale{4.0f, 5.0f}).child_of(root);
    ecs::entity grandchild = ecs::entity::create().set(Scale{0.5f, 2.0f}).child_of(child);

    ecs::progress();

    test_assert(root.get<GlobalScale>().x == 2.0f);
    test_assert(root.get<GlobalScale>().y == 3.0f);
    test_assert(child.get<GlobalScale>().x == 8.0f);
    test_assert(child.get<GlobalScale>().y == 15.0f);
    test_assert(grandchild.get<GlobalScale>().x == 4.0f);
    test_assert(grandchild.get<GlobalScale>().y == 30.0f);

    ecs::fini();
}

void spatial_static_is_not_updated(void) {
    ecs::init();
    ecs::import<sispatial>();

    ecs::entity parent = ecs::entity::create()
        .set(Position{100.0f, 200.0f})
        .add<Static>()
        .set(GlobalPosition{7.0f, 8.0f});
    ecs::entity child = ecs::entity::create().set(Position{1.0f, 2.0f}).child_of(parent);

    ecs::progress();

    test_assert(parent.get<GlobalPosition>().x == 7.0f);
    test_assert(parent.get<GlobalPosition>().y == 8.0f);
    test_assert(child.get<GlobalPosition>().x == 8.0f);
    test_assert(child.get<GlobalPosition>().y == 10.0f);

    ecs::fini();
}

void spatial_cpp_api(void) {
    ecs::init();
    ecs::import<sispatial>();

    Position position{1.0f, 2.0f};
    Rotation rotation{3.0f};
    Scale uniform_scale{4.0f};
    Scale scale{5.0f, 6.0f};

    test_assert(position.x == 1.0f);
    test_assert(position.y == 2.0f);
    test_assert(rotation.value == 3.0f);
    test_assert(uniform_scale.x == 4.0f);
    test_assert(uniform_scale.y == 4.0f);
    test_assert(scale.x == 5.0f);
    test_assert(scale.y == 6.0f);

    ecs::entity entity = ecs::entity::create().set(
        Position{1.0f, 2.0f},
        Rotation{3.0f},
        Scale{4.0f, 5.0f}
    );

    test_assert(entity.has<Position>());
    test_assert(entity.has<GlobalPosition>());
    test_assert(entity.has<Rotation>());
    test_assert(entity.has<GlobalRotation>());
    test_assert(entity.has<Scale>());
    test_assert(entity.has<GlobalScale>());
    test_assert(entity.get<Position>().x == 1.0f);
    test_assert(entity.get<Position>().y == 2.0f);
    test_assert(entity.get<Rotation>().value == 3.0f);
    test_assert(entity.get<Scale>().x == 4.0f);
    test_assert(entity.get<Scale>().y == 5.0f);

    ecs::fini();
}
