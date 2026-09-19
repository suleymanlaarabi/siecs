#include "siecs_spatial.h"
#include <math.h>
#include <stdint.h>
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
ECS_COMPONENT_DEFINE(GlobalOrientation3d);
ECS_CTOR(Scale3d, { 1.0f, 1.0f, 1.0f });
ECS_COMPONENT_DEFINE(Scale3d, .ops = { .ctor = ecs_ctor_id(Scale3d) });
ECS_CTOR(GlobalScale3d, { 1.0f, 1.0f, 1.0f });
ECS_COMPONENT_DEFINE(GlobalScale3d, .ops = { .ctor = ecs_ctor_id(GlobalScale3d) });

ECS_TAG_DEFINE(Static);
ECS_MODULE_DEFINE(sispatial);

static inline GlobalOrientation3d spatial_3d_orientation_from_rotation(const Rotation3d *rotation) {
    const float half_pitch = rotation->pitch * 0.5f;
    const float half_yaw = rotation->yaw * 0.5f;
    const float half_roll = rotation->roll * 0.5f;
    const float sin_pitch = sinf(half_pitch);
    const float cos_pitch = cosf(half_pitch);
    const float sin_yaw = sinf(half_yaw);
    const float cos_yaw = cosf(half_yaw);
    const float sin_roll = sinf(half_roll);
    const float cos_roll = cosf(half_roll);

    return (GlobalOrientation3d){
        .x = sin_pitch * cos_yaw * cos_roll - cos_pitch * sin_yaw * sin_roll,
        .y = cos_pitch * sin_yaw * cos_roll + sin_pitch * cos_yaw * sin_roll,
        .z = cos_pitch * cos_yaw * sin_roll - sin_pitch * sin_yaw * cos_roll,
        .w = cos_pitch * cos_yaw * cos_roll + sin_pitch * sin_yaw * sin_roll,
    };
}

static inline GlobalOrientation3d
spatial_3d_orientation_multiply(const GlobalOrientation3d *left, const GlobalOrientation3d *right) {
    return (GlobalOrientation3d){
        .x = left->w * right->x + left->x * right->w + left->y * right->z - left->z * right->y,
        .y = left->w * right->y - left->x * right->z + left->y * right->w + left->z * right->x,
        .z = left->w * right->z + left->x * right->y - left->y * right->x + left->z * right->w,
        .w = left->w * right->w - left->x * right->x - left->y * right->y - left->z * right->z,
    };
}

static inline Direction3d
spatial_3d_rotate_direction(const GlobalOrientation3d *orientation, Direction3d direction) {
    const Direction3d q = { orientation->x, orientation->y, orientation->z };
    const Direction3d cross = {
        q.y * direction.z - q.z * direction.y,
        q.z * direction.x - q.x * direction.z,
        q.x * direction.y - q.y * direction.x,
    };
    const Direction3d double_cross = {
        q.y * cross.z - q.z * cross.y,
        q.z * cross.x - q.x * cross.z,
        q.x * cross.y - q.y * cross.x,
    };

    return (Direction3d){
        .x = direction.x + 2.0f * (orientation->w * cross.x + double_cross.x),
        .y = direction.y + 2.0f * (orientation->w * cross.y + double_cross.y),
        .z = direction.z + 2.0f * (orientation->w * cross.z + double_cross.z),
    };
}

Direction3d sispatial_forward_3d(const GlobalOrientation3d *orientation) {
    return spatial_3d_rotate_direction(orientation, (Direction3d){ 0.0f, 0.0f, -1.0f });
}

static inline void spatial_2d_compute_static(
    ecs_entity_t entity,
    const Position2d *restrict position,
    const Rotation2d *restrict rotation,
    const Scale2d *restrict scale
) {
    GlobalPosition2d *restrict global_position = ecs_get(entity, GlobalPosition2d);
    GlobalRotation2d *restrict global_rotation = ecs_get(entity, GlobalRotation2d);
    GlobalScale2d *restrict global_scale = ecs_get(entity, GlobalScale2d);

    const ecs_entity_t parent = ecs_target(entity, ChildOf);

    if (parent == 0) {
        global_position->x = position->x;
        global_position->y = position->y;

        global_rotation->value = rotation->value;

        global_scale->x = scale->x;
        global_scale->y = scale->y;
        return;
    }

    const GlobalPosition2d *parent_position = ecs_try_get(parent, GlobalPosition2d);
    const GlobalRotation2d *parent_rotation = ecs_try_get(parent, GlobalRotation2d);
    const GlobalScale2d *parent_scale = ecs_try_get(parent, GlobalScale2d);

    const float px = parent_position != NULL ? parent_position->x : 0.0f;
    const float py = parent_position != NULL ? parent_position->y : 0.0f;

    const float pr = parent_rotation != NULL ? parent_rotation->value : 0.0f;

    const float sx = parent_scale != NULL ? parent_scale->x : 1.0f;
    const float sy = parent_scale != NULL ? parent_scale->y : 1.0f;

    const float x = position->x * sx;
    const float y = position->y * sy;

    const float cos_r = cosf(pr);
    const float sin_r = sinf(pr);

    global_position->x = px + cos_r * x - sin_r * y;
    global_position->y = py + sin_r * x + cos_r * y;

    global_rotation->value = pr + rotation->value;

    global_scale->x = sx * scale->x;
    global_scale->y = sy * scale->y;
}

static ecs_entity_t spatial_3d_transform_parent(ecs_entity_t entity) {
    ecs_entity_t parent = ecs_target(entity, ChildOf);

    while (parent != 0) {
        if (ecs_has(parent, Position3d)) {
            return parent;
        }

        parent = ecs_target(parent, ChildOf);
    }

    return 0;
}

static inline void spatial_3d_compute_static(
    ecs_entity_t entity,
    const Position3d *restrict position,
    const Rotation3d *restrict rotation,
    const Scale3d *restrict scale
) {
    GlobalPosition3d *restrict global_position = ecs_get(entity, GlobalPosition3d);
    GlobalOrientation3d *restrict global_orientation = ecs_get(entity, GlobalOrientation3d);
    GlobalScale3d *restrict global_scale = ecs_get(entity, GlobalScale3d);

    const ecs_entity_t parent = spatial_3d_transform_parent(entity);

    if (parent == 0) {
        global_position->x = position->x;
        global_position->y = position->y;
        global_position->z = position->z;

        *global_orientation = spatial_3d_orientation_from_rotation(rotation);

        global_scale->x = scale->x;
        global_scale->y = scale->y;
        global_scale->z = scale->z;
        return;
    }

    const GlobalPosition3d *parent_position = ecs_try_get(parent, GlobalPosition3d);
    const GlobalOrientation3d *parent_orientation = ecs_try_get(parent, GlobalOrientation3d);
    const GlobalScale3d *parent_scale = ecs_try_get(parent, GlobalScale3d);

    const float px = parent_position != NULL ? parent_position->x : 0.0f;
    const float py = parent_position != NULL ? parent_position->y : 0.0f;
    const float pz = parent_position != NULL ? parent_position->z : 0.0f;

    const GlobalOrientation3d identity = { .x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f };
    const GlobalOrientation3d *orientation =
        parent_orientation != NULL ? parent_orientation : &identity;

    const float sx = parent_scale != NULL ? parent_scale->x : 1.0f;
    const float sy = parent_scale != NULL ? parent_scale->y : 1.0f;
    const float sz = parent_scale != NULL ? parent_scale->z : 1.0f;

    const float x = position->x * sx;
    const float y = position->y * sy;
    const float z = position->z * sz;

    const Direction3d offset = spatial_3d_rotate_direction(orientation, (Direction3d){ x, y, z });

    global_position->x = px + offset.x;
    global_position->y = py + offset.y;
    global_position->z = pz + offset.z;

    const GlobalOrientation3d local_orientation = spatial_3d_orientation_from_rotation(rotation);
    *global_orientation = spatial_3d_orientation_multiply(orientation, &local_orientation);

    global_scale->x = sx * scale->x;
    global_scale->y = sy * scale->y;
    global_scale->z = sz * scale->z;
}

static void spatial_2d_static_propagate_subtree(ecs_entity_t entity);
static void spatial_3d_static_propagate_subtree(ecs_entity_t entity);

static void spatial_2d_static_propagate_children(ecs_entity_t entity) {
    const ecs_relation_sources_t children = ecs_relation_sources(entity, ecs_rid(ChildOf));

    for (uint32_t i = 0; i < children.count; i++) {
        const ecs_entity_t child = children.entities[i];
        if (ecs_has(child, Static) && ecs_has(child, Position2d)) {
            spatial_2d_static_propagate_subtree(child);
        }
    }
}

static void spatial_3d_static_propagate_children(ecs_entity_t entity) {
    const ecs_relation_sources_t children = ecs_relation_sources(entity, ecs_rid(ChildOf));

    for (uint32_t i = 0; i < children.count; i++) {
        const ecs_entity_t child = children.entities[i];

        if (!ecs_has(child, Position3d)) {
            spatial_3d_static_propagate_children(child);
        } else if (ecs_has(child, Static)) {
            spatial_3d_static_propagate_subtree(child);
        }
    }
}

static void spatial_2d_static_propagate_subtree(ecs_entity_t entity) {
    spatial_2d_compute_static(
        entity,
        ecs_get(entity, Position2d),
        ecs_get(entity, Rotation2d),
        ecs_get(entity, Scale2d)
    );
    spatial_2d_static_propagate_children(entity);
}

static void spatial_3d_static_propagate_subtree(ecs_entity_t entity) {
    spatial_3d_compute_static(
        entity,
        ecs_get(entity, Position3d),
        ecs_get(entity, Rotation3d),
        ecs_get(entity, Scale3d)
    );
    spatial_3d_static_propagate_children(entity);
}

static void spatial_2d_static_on_set(ecs_observer_event_t *event) {
    const ecs_component_t component = event->component;

    if (component != ecs_id(Position2d) && component != ecs_id(Rotation2d) &&
        component != ecs_id(Scale2d)) {
        return;
    }

    const ecs_entity_t entity = event->entity;

    const Position2d *position = ecs_get(entity, Position2d);
    const Rotation2d *rotation = ecs_get(entity, Rotation2d);
    const Scale2d *scale = ecs_get(entity, Scale2d);

    /*
     * EcsOnSet is emitted before the incoming value is copied into component
     * storage. Use trigger_data for the component currently being set.
     */
    if (component == ecs_id(Position2d)) {
        position = (const Position2d *)event->trigger_data;
    } else if (component == ecs_id(Rotation2d)) {
        rotation = (const Rotation2d *)event->trigger_data;
    } else {
        scale = (const Scale2d *)event->trigger_data;
    }

    spatial_2d_compute_static(entity, position, rotation, scale);
    spatial_2d_static_propagate_children(entity);
}

static void spatial_3d_static_on_set(ecs_observer_event_t *event) {
    const ecs_component_t component = event->component;

    if (component != ecs_id(Position3d) && component != ecs_id(Rotation3d) &&
        component != ecs_id(Scale3d)) {
        return;
    }

    const ecs_entity_t entity = event->entity;

    const Position3d *position = ecs_get(entity, Position3d);
    const Rotation3d *rotation = ecs_get(entity, Rotation3d);
    const Scale3d *scale = ecs_get(entity, Scale3d);

    if (component == ecs_id(Position3d)) {
        position = (const Position3d *)event->trigger_data;
    } else if (component == ecs_id(Rotation3d)) {
        rotation = (const Rotation3d *)event->trigger_data;
    } else {
        scale = (const Scale3d *)event->trigger_data;
    }

    spatial_3d_compute_static(entity, position, rotation, scale);
    spatial_3d_static_propagate_children(entity);
}

static void spatial_2d_static_on_add(ecs_observer_event_t *event) {
    if (event->component == ecs_id(Position2d)) {
        spatial_2d_static_propagate_subtree(event->entity);
    }
}

static void spatial_3d_static_on_add(ecs_observer_event_t *event) {
    if (event->component == ecs_id(Position3d)) {
        spatial_3d_static_propagate_subtree(event->entity);
    }
}

static void spatial_2d_static_on_relation_set(ecs_observer_event_t *event) {
    const ecs_relation_event_t *relation = event->trigger_data;
    if (relation->relation == ecs_rid(ChildOf)) {
        spatial_2d_static_propagate_subtree(event->entity);
    }
}

static void spatial_3d_static_on_relation_set(ecs_observer_event_t *event) {
    const ecs_relation_event_t *relation = event->trigger_data;
    if (relation->relation == ecs_rid(ChildOf)) {
        spatial_3d_static_propagate_subtree(event->entity);
    }
}

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

    float px = 0.0f;
    float py = 0.0f;

    float parent_rotation = 0.0f;

    float sx = 1.0f;
    float sy = 1.0f;

    float cos_r = 1.0f;
    float sin_r = 0.0f;

    const uint32_t count = it->count;

    for (uint32_t i = 0; i < count; i++) {
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
    GlobalOrientation3d *restrict global_orientation = ecs_field(it, 4);
    GlobalScale3d *restrict global_scale = ecs_field(it, 5);

    const ecs_relation_target_t *parents = ecs_targets(it, ChildOf);

    if (parents == NULL) {
        memcpy(global_position, position, sizeof(*global_position) * it->count);
        for (uint32_t i = 0; i < it->count; i++) {
            global_orientation[i] = spatial_3d_orientation_from_rotation(&rotation[i]);
        }
        memcpy(global_scale, scale, sizeof(*global_scale) * it->count);
        return;
    }

    ecs_entity_t cached_parent = 0;

    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;

    GlobalOrientation3d parent_orientation = { .x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f };

    float sx = 1.0f;
    float sy = 1.0f;
    float sz = 1.0f;

    const uint32_t count = it->count;

    for (uint32_t i = 0; i < count; i++) {
        const ecs_entity_t parent_entity = parents[i].entity;

        if (parent_entity != cached_parent) {
            const GlobalPosition3d *parent_position = ecs_try_get(parent_entity, GlobalPosition3d);

            const GlobalOrientation3d *parent_global_orientation =
                ecs_try_get(parent_entity, GlobalOrientation3d);

            const GlobalScale3d *parent_scale = ecs_try_get(parent_entity, GlobalScale3d);

            px = parent_position != NULL ? parent_position->x : 0.0f;
            py = parent_position != NULL ? parent_position->y : 0.0f;
            pz = parent_position != NULL ? parent_position->z : 0.0f;

            parent_orientation = parent_global_orientation != NULL ? *parent_global_orientation
                                                                   : (GlobalOrientation3d){
                                                                         .x = 0.0f,
                                                                         .y = 0.0f,
                                                                         .z = 0.0f,
                                                                         .w = 1.0f,
                                                                     };

            sx = parent_scale != NULL ? parent_scale->x : 1.0f;
            sy = parent_scale != NULL ? parent_scale->y : 1.0f;
            sz = parent_scale != NULL ? parent_scale->z : 1.0f;

            cached_parent = parent_entity;
        }

        const float x = position[i].x * sx;
        const float y = position[i].y * sy;
        const float z = position[i].z * sz;

        const Direction3d offset =
            spatial_3d_rotate_direction(&parent_orientation, (Direction3d){ x, y, z });

        global_position[i].x = px + offset.x;

        global_position[i].y = py + offset.y;

        global_position[i].z = pz + offset.z;

        const GlobalOrientation3d local_orientation =
            spatial_3d_orientation_from_rotation(&rotation[i]);
        global_orientation[i] =
            spatial_3d_orientation_multiply(&parent_orientation, &local_orientation);

        global_scale[i].x = sx * scale[i].x;

        global_scale[i].y = sy * scale[i].y;

        global_scale[i].z = sz * scale[i].z;
    }
}

void spatial_2d_integrate(ecs_iter_t *it) {
    Position2d *restrict position = ecs_field(it, 0);
    const Velocity2d *restrict velocity = ecs_field(it, 1);

    const uint32_t count = it->count;
    const float delta_time = it->delta_time;

    for (uint32_t i = 0; i < count; i++) {
        position[i].x += velocity[i].x * delta_time;
        position[i].y += velocity[i].y * delta_time;
    }
}

void spatial_3d_integrate(ecs_iter_t *it) {
    Position3d *restrict position = ecs_field(it, 0);
    const Velocity3d *restrict velocity = ecs_field(it, 1);

    const uint32_t count = it->count;
    const float delta_time = it->delta_time;

    for (uint32_t i = 0; i < count; i++) {
        position[i].x += velocity[i].x * delta_time;
        position[i].y += velocity[i].y * delta_time;
        position[i].z += velocity[i].z * delta_time;
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
        GlobalOrientation3d,
        Scale3d,
        GlobalScale3d,

        Static
    );

    ecs_with(Position2d, Rotation2d, Scale2d, GlobalPosition2d, GlobalRotation2d, GlobalScale2d);

    ecs_with(Position3d, Rotation3d, Scale3d, GlobalPosition3d, GlobalOrientation3d, GlobalScale3d);

    /*
     * Static transforms are excluded from the per-frame propagation systems.
     * Recompute them only when Position / Rotation / Scale receives OnSet.
     *
     * Position2d/3d is enough as a transform marker because ecs_with()
     * guarantees the other transform components.
     *
     * SIECS observers are indexed by event + matching table, not by the
     * component which caused EcsOnSet. Therefore one observer per transform
     * type plus the early component check in the callback is cheaper than
     * registering three observers.
     */
    ecs_observer(
        {
            .on = EcsOnSet,
            .query = {
                .components = {
                    ecs_filter(Position2d),
                    ecs_filter(Static),
                    ecs_in_optional(Abstract),
                },
            },
            .callback = spatial_2d_static_on_set,
        }
    );

    ecs_observer(
        {
            .on = EcsOnSet,
            .query = {
                .components = {
                    ecs_filter(Position3d),
                    ecs_filter(Static),
                    ecs_in_optional(Abstract),
                },
            },
            .callback = spatial_3d_static_on_set,
        }
    );

    ecs_observer(
        {
            .on = EcsOnAdd,
            .query = {
                .components = {
                    ecs_filter(Position2d),
                    ecs_filter(Static),
                    ecs_in_optional(Abstract),
                },
            },
            .callback = spatial_2d_static_on_add,
        }
    );

    ecs_observer(
        {
            .on = EcsOnAdd,
            .query = {
                .components = {
                    ecs_filter(Position3d),
                    ecs_filter(Static),
                    ecs_in_optional(Abstract),
                },
            },
            .callback = spatial_3d_static_on_add,
        }
    );

    ecs_observer(
        {
            .on = EcsOnRelationSet,
            .query = {
                .components = {
                    ecs_filter(Position2d),
                    ecs_filter(Static),
                    ecs_in_optional(Abstract),
                },
            },
            .callback = spatial_2d_static_on_relation_set,
        }
    );

    ecs_observer(
        {
            .on = EcsOnRelationSet,
            .query = {
                .components = {
                    ecs_filter(Position3d),
                    ecs_filter(Static),
                    ecs_in_optional(Abstract),
                },
            },
            .callback = spatial_3d_static_on_relation_set,
        }
    );

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
                    ecs_inout(GlobalOrientation3d),
                    ecs_inout(GlobalScale3d),
                    ecs_not(Static),
                },
                .order_by = ecs_order_by_depth(ChildOf),
            },
            .callback = spatial_3d_propagate,
            .phase = EcsPostUpdate,
        }
    );

    ecs_system(
        {
            .name = "Spatial2dIntegrate",
            .query = {
                .components = {
                    ecs_inout(Position2d),
                    ecs_in(Velocity2d),
                },
            },
            .callback = spatial_2d_integrate,
            .phase = EcsOnUpdate,
        }
    );

    ecs_system(
        {
            .name = "Spatial3dIntegrate",
            .query = {
                .components = {
                    ecs_inout(Position3d),
                    ecs_in(Velocity3d),
                },
            },
            .callback = spatial_3d_integrate,
            .phase = EcsOnUpdate,
        }
    );
}
