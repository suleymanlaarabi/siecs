#include "siecs_spatial.h"

#include <math.h>
#include <string.h>

ECS_COMPONENT_DEFINE(Position2d);
ECS_COMPONENT_DEFINE(GlobalPosition2d);
ECS_COMPONENT_DEFINE(Velocity2d);
ECS_COMPONENT_DEFINE(Rotation2d);
ECS_COMPONENT_DEFINE(GlobalRotation2d);
ECS_CTOR(Scale2d, { 1.0f, 1.0f });
ECS_COMPONENT_DEFINE(Scale2d, .ops = { .ctor = ecs_ctor_id(Scale2d) });
ECS_CTOR(GlobalScale2d, { 1.0f, 1.0f });
ECS_COMPONENT_DEFINE(GlobalScale2d, .ops = { .ctor = ecs_ctor_id(GlobalScale2d) });

ECS_COMPONENT_DEFINE(Position3d);
ECS_COMPONENT_DEFINE(GlobalPosition3d);
ECS_COMPONENT_DEFINE(Velocity3d);
ECS_COMPONENT_DEFINE(Rotation3d);
ECS_COMPONENT_DEFINE(GlobalRotation3d);
ECS_CTOR(Scale3d, { 1.0f, 1.0f, 1.0f });
ECS_COMPONENT_DEFINE(Scale3d, .ops = { .ctor = ecs_ctor_id(Scale3d) });
ECS_CTOR(GlobalScale3d, { 1.0f, 1.0f, 1.0f });
ECS_COMPONENT_DEFINE(GlobalScale3d, .ops = { .ctor = ecs_ctor_id(GlobalScale3d) });

ECS_TAG_DEFINE(Static);
ECS_MODULE_DEFINE(sispatial);

static void spatial_2d_propagate(ecs_iter_t *it) {
    const Position2d *restrict position = ecs_field(it, 0);
    const Rotation2d *restrict rotation = ecs_field(it, 1);
    const Scale2d *restrict scale = ecs_field(it, 2);
    GlobalPosition2d *restrict global_position = ecs_field(it, 3);
    GlobalRotation2d *restrict global_rotation = ecs_field(it, 4);
    GlobalScale2d *restrict global_scale = ecs_field(it, 5);

    const ecs_relation_target_t *parents = ecs_targets(it, ChildOf);

    if (parents == NULL) {
        memcpy(global_position, position, sizeof(*global_position) * it->count);
        memcpy(global_rotation, rotation, sizeof(*global_rotation) * it->count);
        memcpy(global_scale, scale, sizeof(*global_scale) * it->count);
        return;
    }

    ecs_entity_t cached_parent = 0;
    float px = 0.0f, py = 0.0f;
    float parent_rotation = 0.0f;
    float sx = 1.0f, sy = 1.0f;
    float cos_r = 1.0f, sin_r = 0.0f;

    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t parent_entity = parents[i].entity;

        if (parent_entity != cached_parent) {
            const GlobalPosition2d *parent_position = ecs_try_get(parent_entity, GlobalPosition2d);
            const GlobalRotation2d *parent_rotation_component =
                ecs_try_get(parent_entity, GlobalRotation2d);
            const GlobalScale2d *parent_scale = ecs_try_get(parent_entity, GlobalScale2d);

            px = parent_position != NULL ? parent_position->x : 0.0f;
            py = parent_position != NULL ? parent_position->y : 0.0f;
            parent_rotation =
                parent_rotation_component != NULL ? parent_rotation_component->value : 0.0f;
            sx = parent_scale != NULL ? parent_scale->x : 1.0f;
            sy = parent_scale != NULL ? parent_scale->y : 1.0f;

            cos_r = cosf(parent_rotation);
            sin_r = sinf(parent_rotation);
            cached_parent = parent_entity;
        }

        const float x = position[i].x * sx;
        const float y = position[i].y * sy;

        global_position[i].x = px + cos_r * x - sin_r * y;
        global_position[i].y = py + sin_r * x + cos_r * y;

        global_rotation[i].value = parent_rotation + rotation[i].value;

        global_scale[i].x = sx * scale[i].x;
        global_scale[i].y = sy * scale[i].y;
    }
}

static void spatial_3d_propagate(ecs_iter_t *it) {
    const Position3d *restrict position = ecs_field(it, 0);
    const Rotation3d *restrict rotation = ecs_field(it, 1);
    const Scale3d *restrict scale = ecs_field(it, 2);
    GlobalPosition3d *restrict global_position = ecs_field(it, 3);
    GlobalRotation3d *restrict global_rotation = ecs_field(it, 4);
    GlobalScale3d *restrict global_scale = ecs_field(it, 5);

    const ecs_relation_target_t *parents = ecs_targets(it, ChildOf);

    if (parents == NULL) {
        memcpy(global_position, position, sizeof(*global_position) * it->count);
        memcpy(global_rotation, rotation, sizeof(*global_rotation) * it->count);
        memcpy(global_scale, scale, sizeof(*global_scale) * it->count);
        return;
    }

    ecs_entity_t cached_parent = 0;
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    float sx = 1.0f, sy = 1.0f, sz = 1.0f;
    float cx = 1.0f, cy = 1.0f, cz = 1.0f;
    float sin_x = 0.0f, sin_y = 0.0f, sin_z = 0.0f;

    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t parent_entity = parents[i].entity;

        if (parent_entity != cached_parent) {
            const GlobalPosition3d *parent_position = ecs_try_get(parent_entity, GlobalPosition3d);
            const GlobalRotation3d *parent_rotation = ecs_try_get(parent_entity, GlobalRotation3d);
            const GlobalScale3d *parent_scale = ecs_try_get(parent_entity, GlobalScale3d);

            px = parent_position != NULL ? parent_position->x : 0.0f;
            py = parent_position != NULL ? parent_position->y : 0.0f;
            pz = parent_position != NULL ? parent_position->z : 0.0f;

            rx = parent_rotation != NULL ? parent_rotation->x : 0.0f;
            ry = parent_rotation != NULL ? parent_rotation->y : 0.0f;
            rz = parent_rotation != NULL ? parent_rotation->z : 0.0f;

            sx = parent_scale != NULL ? parent_scale->x : 1.0f;
            sy = parent_scale != NULL ? parent_scale->y : 1.0f;
            sz = parent_scale != NULL ? parent_scale->z : 1.0f;

            cx = cosf(rx);
            cy = cosf(ry);
            cz = cosf(rz);
            sin_x = sinf(rx);
            sin_y = sinf(ry);
            sin_z = sinf(rz);

            cached_parent = parent_entity;
        }

        const float x = position[i].x * sx;
        const float y = position[i].y * sy;
        const float z = position[i].z * sz;

        const float x1 = x;
        const float y1 = cx * y - sin_x * z;
        const float z1 = sin_x * y + cx * z;

        const float x2 = cy * x1 + sin_y * z1;
        const float y2 = y1;
        const float z2 = -sin_y * x1 + cy * z1;

        global_position[i].x = px + cz * x2 - sin_z * y2;
        global_position[i].y = py + sin_z * x2 + cz * y2;
        global_position[i].z = pz + z2;

        global_rotation[i].x = rx + rotation[i].x;
        global_rotation[i].y = ry + rotation[i].y;
        global_rotation[i].z = rz + rotation[i].z;

        global_scale[i].x = sx * scale[i].x;
        global_scale[i].y = sy * scale[i].y;
        global_scale[i].z = sz * scale[i].z;
    }
}

void sispatial_import(const sispatial_props_t *props) {
    (void)props;

    ECS_COMPONENT_REGISTER(
        Position2d,
        Velocity2d,
        GlobalPosition2d,
        Rotation2d,
        GlobalRotation2d,
        Scale2d,
        GlobalScale2d,
        Position3d,
        GlobalPosition3d,
        Velocity3d,
        Rotation3d,
        GlobalRotation3d,
        Scale3d,
        GlobalScale3d,
        Static
    );

    ecs_with(Position2d, Rotation2d, Scale2d, GlobalPosition2d, GlobalRotation2d, GlobalScale2d);
    ecs_with(Position3d, Rotation3d, Scale3d, GlobalPosition3d, GlobalRotation3d, GlobalScale3d);

    ecs_system(
        {
            .name = "Spatial2dPropagation",
            .query = {
                .components = {
                    ecs_in(Position2d),
                    ecs_in(Rotation2d),
                    ecs_in(Scale2d),
                    ecs_inout(GlobalPosition2d),
                    ecs_inout(GlobalRotation2d),
                    ecs_inout(GlobalScale2d),
                    ecs_not(Static),
                },
                .order_by = ecs_order_by_depth(ChildOf),
            },
            .callback = spatial_2d_propagate,
            .phase = EcsPostUpdate,
        }
    );

    ecs_system(
        {
            .name = "Spatial3dPropagation",
            .query = {
                .components = {
                    ecs_in(Position3d),
                    ecs_in(Rotation3d),
                    ecs_in(Scale3d),
                    ecs_inout(GlobalPosition3d),
                    ecs_inout(GlobalRotation3d),
                    ecs_inout(GlobalScale3d),
                    ecs_not(Static),
                },
                .order_by = ecs_order_by_depth(ChildOf),
            },
            .callback = spatial_3d_propagate,
            .phase = EcsPostUpdate,
        }
    );
}
