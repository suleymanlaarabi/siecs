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

static void spatial_position_2d_propagate(ecs_iter_t *it) {
    const Position2d *restrict local = ecs_field(it, 0);
    GlobalPosition2d *restrict global = ecs_field(it, 1);
    const ecs_relation_target_t *parents = ecs_targets(it, ChildOf);

    if (parents == NULL) {
        memcpy(global, local, sizeof(*global) * it->count);
        return;
    }

    ecs_entity_t cached_parent = 0;
    float parent_x = 0.0f, parent_y = 0.0f;
    float parent_scale_x = 1.0f, parent_scale_y = 1.0f;
    float parent_cos = 1.0f, parent_sin = 0.0f;

    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t parent_entity = parents[i].entity;
        if (parent_entity != cached_parent) {
            const GlobalPosition2d *position = ecs_try_get(parent_entity, GlobalPosition2d);
            const GlobalRotation2d *rotation = ecs_try_get(parent_entity, GlobalRotation2d);
            const GlobalScale2d *scale = ecs_try_get(parent_entity, GlobalScale2d);
            parent_x = position != NULL ? position->x : 0.0f;
            parent_y = position != NULL ? position->y : 0.0f;
            parent_scale_x = scale != NULL ? scale->x : 1.0f;
            parent_scale_y = scale != NULL ? scale->y : 1.0f;
            parent_cos = rotation != NULL ? cosf(rotation->value) : 1.0f;
            parent_sin = rotation != NULL ? sinf(rotation->value) : 0.0f;
            cached_parent = parent_entity;
        }

        const float x = local[i].x * parent_scale_x;
        const float y = local[i].y * parent_scale_y;
        global[i].x = parent_x + parent_cos * x - parent_sin * y;
        global[i].y = parent_y + parent_sin * x + parent_cos * y;
    }
}

static void spatial_rotation_2d_propagate(ecs_iter_t *it) {
    const Rotation2d *restrict local = ecs_field(it, 0);
    GlobalRotation2d *restrict global = ecs_field(it, 1);
    const ecs_relation_target_t *parents = ecs_targets(it, ChildOf);

    if (parents == NULL) {
        memcpy(global, local, sizeof(*global) * it->count);
        return;
    }

    ecs_entity_t cached_parent = 0;
    float parent_value = 0.0f;
    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t parent_entity = parents[i].entity;
        if (parent_entity != cached_parent) {
            const GlobalRotation2d *parent = ecs_try_get(parent_entity, GlobalRotation2d);
            parent_value = parent != NULL ? parent->value : 0.0f;
            cached_parent = parent_entity;
        }
        global[i].value = parent_value + local[i].value;
    }
}

static void spatial_scale_2d_propagate(ecs_iter_t *it) {
    const Scale2d *restrict local = ecs_field(it, 0);
    GlobalScale2d *restrict global = ecs_field(it, 1);
    const ecs_relation_target_t *parents = ecs_targets(it, ChildOf);

    if (parents == NULL) {
        memcpy(global, local, sizeof(*global) * it->count);
        return;
    }

    ecs_entity_t cached_parent = 0;
    float parent_x = 1.0f, parent_y = 1.0f;
    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t parent_entity = parents[i].entity;
        if (parent_entity != cached_parent) {
            const GlobalScale2d *parent = ecs_try_get(parent_entity, GlobalScale2d);
            parent_x = parent != NULL ? parent->x : 1.0f;
            parent_y = parent != NULL ? parent->y : 1.0f;
            cached_parent = parent_entity;
        }
        global[i].x = parent_x * local[i].x;
        global[i].y = parent_y * local[i].y;
    }
}

static void spatial_position_3d_propagate(ecs_iter_t *it) {
    const Position3d *restrict local = ecs_field(it, 0);
    GlobalPosition3d *restrict global = ecs_field(it, 1);
    const ecs_relation_target_t *parents = ecs_targets(it, ChildOf);

    if (parents == NULL) {
        memcpy(global, local, sizeof(*global) * it->count);
        return;
    }

    ecs_entity_t cached_parent = 0;
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float sx = 1.0f, sy = 1.0f, sz = 1.0f;
    float cx = 1.0f, cy = 1.0f, cz = 1.0f;
    float sin_x = 0.0f, sin_y = 0.0f, sin_z = 0.0f;

    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t parent_entity = parents[i].entity;
        if (parent_entity != cached_parent) {
            const GlobalPosition3d *position = ecs_try_get(parent_entity, GlobalPosition3d);
            const GlobalRotation3d *rotation = ecs_try_get(parent_entity, GlobalRotation3d);
            const GlobalScale3d *scale = ecs_try_get(parent_entity, GlobalScale3d);
            px = position != NULL ? position->x : 0.0f;
            py = position != NULL ? position->y : 0.0f;
            pz = position != NULL ? position->z : 0.0f;
            sx = scale != NULL ? scale->x : 1.0f;
            sy = scale != NULL ? scale->y : 1.0f;
            sz = scale != NULL ? scale->z : 1.0f;
            cx = rotation != NULL ? cosf(rotation->x) : 1.0f;
            cy = rotation != NULL ? cosf(rotation->y) : 1.0f;
            cz = rotation != NULL ? cosf(rotation->z) : 1.0f;
            sin_x = rotation != NULL ? sinf(rotation->x) : 0.0f;
            sin_y = rotation != NULL ? sinf(rotation->y) : 0.0f;
            sin_z = rotation != NULL ? sinf(rotation->z) : 0.0f;
            cached_parent = parent_entity;
        }

        const float x = local[i].x * sx;
        const float y = local[i].y * sy;
        const float z = local[i].z * sz;
        const float x1 = x;
        const float y1 = cx * y - sin_x * z;
        const float z1 = sin_x * y + cx * z;
        const float x2 = cy * x1 + sin_y * z1;
        const float y2 = y1;
        const float z2 = -sin_y * x1 + cy * z1;
        global[i].x = px + cz * x2 - sin_z * y2;
        global[i].y = py + sin_z * x2 + cz * y2;
        global[i].z = pz + z2;
    }
}

static void spatial_rotation_3d_propagate(ecs_iter_t *it) {
    const Rotation3d *restrict local = ecs_field(it, 0);
    GlobalRotation3d *restrict global = ecs_field(it, 1);
    const ecs_relation_target_t *parents = ecs_targets(it, ChildOf);

    if (parents == NULL) {
        memcpy(global, local, sizeof(*global) * it->count);
        return;
    }

    ecs_entity_t cached_parent = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t parent_entity = parents[i].entity;
        if (parent_entity != cached_parent) {
            const GlobalRotation3d *parent = ecs_try_get(parent_entity, GlobalRotation3d);
            x = parent != NULL ? parent->x : 0.0f;
            y = parent != NULL ? parent->y : 0.0f;
            z = parent != NULL ? parent->z : 0.0f;
            cached_parent = parent_entity;
        }
        global[i].x = x + local[i].x;
        global[i].y = y + local[i].y;
        global[i].z = z + local[i].z;
    }
}

static void spatial_scale_3d_propagate(ecs_iter_t *it) {
    const Scale3d *restrict local = ecs_field(it, 0);
    GlobalScale3d *restrict global = ecs_field(it, 1);
    const ecs_relation_target_t *parents = ecs_targets(it, ChildOf);

    if (parents == NULL) {
        memcpy(global, local, sizeof(*global) * it->count);
        return;
    }

    ecs_entity_t cached_parent = 0;
    float x = 1.0f, y = 1.0f, z = 1.0f;
    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t parent_entity = parents[i].entity;
        if (parent_entity != cached_parent) {
            const GlobalScale3d *parent = ecs_try_get(parent_entity, GlobalScale3d);
            x = parent != NULL ? parent->x : 1.0f;
            y = parent != NULL ? parent->y : 1.0f;
            z = parent != NULL ? parent->z : 1.0f;
            cached_parent = parent_entity;
        }
        global[i].x = x * local[i].x;
        global[i].y = y * local[i].y;
        global[i].z = z * local[i].z;
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

    ecs_with(Position2d, GlobalPosition2d, GlobalRotation2d, GlobalScale2d);
    ecs_with(Rotation2d, GlobalRotation2d);
    ecs_with(Scale2d, GlobalScale2d);
    ecs_with(Position3d, GlobalPosition3d, GlobalRotation3d, GlobalScale3d);
    ecs_with(Rotation3d, GlobalRotation3d);
    ecs_with(Scale3d, GlobalScale3d);

    const ecs_system_id_t rotation_2d_system = ecs_system(
        {
            .name = "SpatialRotation2dPropagation",
            .query = {
                .components = { ecs_in(Rotation2d), ecs_inout(GlobalRotation2d), ecs_not(Static) },
                .order_by = ecs_order_by_depth(ChildOf),
            },
            .callback = spatial_rotation_2d_propagate,
            .phase = EcsPostUpdate,
        }
    );

    const ecs_system_id_t scale_2d_system = ecs_system(
        {
            .name = "SpatialScale2dPropagation",
            .query = {
                .components = { ecs_in(Scale2d), ecs_inout(GlobalScale2d), ecs_not(Static) },
                .order_by = ecs_order_by_depth(ChildOf),
            },
            .callback = spatial_scale_2d_propagate,
            .phase = EcsPostUpdate,
        }
    );

    ecs_system(
        {
            .name = "SpatialPosition2dPropagation",
            .query = {
                .components = {
                    ecs_in(Position2d),
                    ecs_inout(GlobalPosition2d),
                    ecs_in(GlobalRotation2d),
                    ecs_in(GlobalScale2d),
                    ecs_not(Static),
                },
                .order_by = ecs_order_by_depth(ChildOf),
            },
            .callback = spatial_position_2d_propagate,
            .phase = EcsPostUpdate,
            .after = { rotation_2d_system, scale_2d_system },
        }
    );

    const ecs_system_id_t rotation_3d_system = ecs_system(
        {
            .name = "SpatialRotation3dPropagation",
            .query = {
                .components = {
                    ecs_in(Rotation3d),
                    ecs_inout(GlobalRotation3d),
                    ecs_not(Static),
                },
                .order_by = ecs_order_by_depth(ChildOf),
            },
            .callback = spatial_rotation_3d_propagate,
            .phase = EcsPostUpdate,
        }
    );

    const ecs_system_id_t scale_3d_system = ecs_system(
        {
            .name = "SpatialScale3dPropagation",
            .query = {
                .components = {
                    ecs_in(Scale3d),
                    ecs_inout(GlobalScale3d),
                    ecs_not(Static),
                },
                .order_by = ecs_order_by_depth(ChildOf),
            },
            .callback = spatial_scale_3d_propagate,
            .phase = EcsPostUpdate,
        }
    );

    ecs_system(
        {
            .name = "SpatialPosition3dPropagation",
            .query = {
                .components = {
                    ecs_in(Position3d),
                    ecs_inout(GlobalPosition3d),
                    ecs_in(GlobalRotation3d),
                    ecs_in(GlobalScale3d),
                    ecs_not(Static),
                },
                .order_by = ecs_order_by_depth(ChildOf),
            },
            .callback = spatial_position_3d_propagate,
            .phase = EcsPostUpdate,
            .after = { rotation_3d_system, scale_3d_system },
        }
    );
}
