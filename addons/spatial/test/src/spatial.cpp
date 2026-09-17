#include <siecs_spatial.h>
#include <test.h>

void spatial_component_requirements_and_defaults(void) {
    ecs::init();
    ecs::import<sispatial>();

    ecs::entity position_entity = ecs::entity::create().set(Position2d{ 10.0f, 20.0f });
    test_assert(position_entity.has<GlobalPosition2d>());

    ecs::entity scale_entity = ecs::entity::create().add<Scale2d>();
    test_assert(scale_entity.has<GlobalScale2d>());
    test_assert(scale_entity.get<Scale2d>().x == 1.0f);
    test_assert(scale_entity.get<Scale2d>().y == 1.0f);
    test_assert(scale_entity.get<GlobalScale2d>().x == 1.0f);
    test_assert(scale_entity.get<GlobalScale2d>().y == 1.0f);

    ecs::entity rotation_entity = ecs::entity::create().set(Rotation2d{ 3.0f });
    test_assert(rotation_entity.has<GlobalRotation2d>());

    ecs::fini();
}

void spatial_position_hierarchy(void) {
    ecs::init();
    ecs::import<sispatial>();

    ecs::entity root = ecs::entity::create().set(Position2d{ 10.0f, 10.0f });
    ecs::entity child = ecs::entity::create().set(Position2d{ 5.0f, 5.0f }).child_of(root);
    ecs::entity grandchild = ecs::entity::create().set(Position2d{ 2.0f, 3.0f }).child_of(child);

    ecs::progress();

    test_assert(root.get<GlobalPosition2d>().x == 10.0f);
    test_assert(root.get<GlobalPosition2d>().y == 10.0f);
    test_assert(child.get<GlobalPosition2d>().x == 15.0f);
    test_assert(child.get<GlobalPosition2d>().y == 15.0f);
    test_assert(grandchild.get<GlobalPosition2d>().x == 17.0f);
    test_assert(grandchild.get<GlobalPosition2d>().y == 18.0f);

    ecs::fini();
}

void spatial_rotation_hierarchy(void) {
    ecs::init();
    ecs::import<sispatial>();

    ecs::entity root = ecs::entity::create().set(Rotation2d{ 10.0f });
    ecs::entity child = ecs::entity::create().set(Rotation2d{ 20.0f }).child_of(root);
    ecs::entity grandchild = ecs::entity::create().set(Rotation2d{ 5.0f }).child_of(child);

    ecs::progress();

    test_assert(root.get<GlobalRotation2d>().value == 10.0f);
    test_assert(child.get<GlobalRotation2d>().value == 30.0f);
    test_assert(grandchild.get<GlobalRotation2d>().value == 35.0f);

    ecs::fini();
}

void spatial_scale_hierarchy(void) {
    ecs::init();
    ecs::import<sispatial>();

    ecs::entity root = ecs::entity::create().set(Scale2d{ 2.0f, 3.0f });
    ecs::entity child = ecs::entity::create().set(Scale2d{ 4.0f, 5.0f }).child_of(root);
    ecs::entity grandchild = ecs::entity::create().set(Scale2d{ 0.5f, 2.0f }).child_of(child);

    ecs::progress();

    test_assert(root.get<GlobalScale2d>().x == 2.0f);
    test_assert(root.get<GlobalScale2d>().y == 3.0f);
    test_assert(child.get<GlobalScale2d>().x == 8.0f);
    test_assert(child.get<GlobalScale2d>().y == 15.0f);
    test_assert(grandchild.get<GlobalScale2d>().x == 4.0f);
    test_assert(grandchild.get<GlobalScale2d>().y == 30.0f);

    ecs::fini();
}

void spatial_static_is_not_updated(void) {
    ecs::init();
    ecs::import<sispatial>();

    ecs::entity parent = ecs::entity::create()
                             .set(Position2d{ 100.0f, 200.0f })
                             .add<Static>()
                             .set(GlobalPosition2d{ 7.0f, 8.0f });
    ecs::entity child = ecs::entity::create().set(Position2d{ 1.0f, 2.0f }).child_of(parent);

    ecs::progress();

    test_assert(parent.get<GlobalPosition2d>().x == 7.0f);
    test_assert(parent.get<GlobalPosition2d>().y == 8.0f);
    test_assert(child.get<GlobalPosition2d>().x == 8.0f);
    test_assert(child.get<GlobalPosition2d>().y == 10.0f);

    ecs::fini();
}

void spatial_cpp_api(void) {
    ecs::init();
    ecs::import<sispatial>();

    Position2d position{ 1.0f, 2.0f };
    Rotation2d rotation{ 3.0f };
    Scale2d uniform_scale{ 4.0f };
    Scale2d scale{ 5.0f, 6.0f };

    test_assert(position.x == 1.0f);
    test_assert(position.y == 2.0f);
    test_assert(rotation.value == 3.0f);
    test_assert(uniform_scale.x == 4.0f);
    test_assert(uniform_scale.y == 4.0f);
    test_assert(scale.x == 5.0f);
    test_assert(scale.y == 6.0f);

    ecs::entity entity =
        ecs::entity::create().set(
            Position2d{ 1.0f, 2.0f },
            Rotation2d{ 3.0f },
            Scale2d{ 4.0f, 5.0f }
        );

    test_assert(entity.has<Position2d>());
    test_assert(entity.has<GlobalPosition2d>());
    test_assert(entity.has<Rotation2d>());
    test_assert(entity.has<GlobalRotation2d>());
    test_assert(entity.has<Scale2d>());
    test_assert(entity.has<GlobalScale2d>());
    test_assert(entity.get<Position2d>().x == 1.0f);
    test_assert(entity.get<Position2d>().y == 2.0f);
    test_assert(entity.get<Rotation2d>().value == 3.0f);
    test_assert(entity.get<Scale2d>().x == 4.0f);
    test_assert(entity.get<Scale2d>().y == 5.0f);

    ecs::fini();
}
