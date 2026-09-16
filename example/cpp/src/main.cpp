#include <cassert>
#include <cmath>
#include <cstdio>
#include <siecs_spatial.h>

int main() {
    ecs::init();
    ecs::import<sispatial>();

    constexpr float half_pi = 1.57079632679489661923f;

    ecs::entity root = ecs::entity::create().set(
        Position2d{5.0f, 5.0f},
        Rotation2d{half_pi},
        Scale2d{2.0f, 3.0f}
    );

    ecs::entity child = ecs::entity::create()
        .set(
            Position2d{5.0f, 5.0f},
            Rotation2d{0.0f},
            Scale2d{0.5f, 2.0f}
        )
        .child_of(root);

    ecs::progress();

    const GlobalPosition2d &position = child.get<GlobalPosition2d>();
    const GlobalRotation2d &rotation = child.get<GlobalRotation2d>();
    const GlobalScale2d &scale = child.get<GlobalScale2d>();

    assert(std::fabs(position.x + 10.0f) < 0.0001f);
    assert(std::fabs(position.y - 15.0f) < 0.0001f);
    assert(rotation.value == half_pi);
    assert(scale.x == 1.0f && scale.y == 6.0f);

    std::printf(
        "2d child global: position=(%.1f, %.1f), rotation=%.1f, scale=(%.1f, %.1f)\n",
        position.x,
        position.y,
        rotation.value,
        scale.x,
        scale.y
    );

    ecs::entity root_3d = ecs::entity::create().set(
        Position3d{1.0f, 2.0f, 3.0f},
        Rotation3d{0.0f, 0.0f, half_pi},
        Scale3d{2.0f}
    );
    ecs::entity child_3d = ecs::entity::create()
        .set(Position3d{1.0f, 0.0f, 0.0f})
        .child_of(root_3d);

    ecs::progress();

    const GlobalPosition3d &position_3d = child_3d.get<GlobalPosition3d>();
    assert(std::fabs(position_3d.x - 1.0f) < 0.0001f);
    assert(std::fabs(position_3d.y - 4.0f) < 0.0001f);
    assert(std::fabs(position_3d.z - 3.0f) < 0.0001f);

    std::printf(
        "3d child global: position=(%.1f, %.1f, %.1f)\n",
        position_3d.x,
        position_3d.y,
        position_3d.z
    );

    ecs::fini();
    return 0;
}
