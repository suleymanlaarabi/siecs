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

/*
 * Propagation (RFC section 12.5) runs level by level of the ChildOf
 * hierarchy: roots, then depth 1, 2... A level only visits the tables whose
 * local transform changed, unless a level above it changed, in which case
 * the whole level is recomputed (conservative invalidation). Tables that are
 * not visited do not mark their global transform, so a still hierarchy
 * produces no change the renderer sees.
 */
#define SPATIAL_MAX_DEPTH 32

typedef struct {
    ecs_query_id_t changed[SPATIAL_MAX_DEPTH];
    ecs_query_id_t all[SPATIAL_MAX_DEPTH];
} spatial_levels_t;

static spatial_levels_t levels_2d, levels_3d;

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

static ecs_entity_t spatial_2d_transform_parent(ecs_entity_t entity) {
    ecs_entity_t parent = ecs_target(entity, ChildOf);
    while (parent != 0) {
        if (ecs_has(parent, Position2d)) {
            return parent;
        }
        parent = ecs_target(parent, ChildOf);
    }
    return 0;
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
    float px = 0.0f, py = 0.0f, parent_rotation = 0.0f, sx = 1.0f, sy = 1.0f;
    float cos_r = 1.0f, sin_r = 0.0f;
    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t direct_parent = parents[i].entity;
        if (direct_parent != cached_parent) {
            const ecs_entity_t parent = ecs_has(direct_parent, Position2d)
                                            ? direct_parent
                                            : spatial_2d_transform_parent(it->entities[i]);
            const GlobalPosition2d *p = parent ? ecs_try_get(parent, GlobalPosition2d) : NULL;
            const GlobalRotation2d *r = parent ? ecs_try_get(parent, GlobalRotation2d) : NULL;
            const GlobalScale2d *s = parent ? ecs_try_get(parent, GlobalScale2d) : NULL;
            px = p ? p->x : 0.0f;
            py = p ? p->y : 0.0f;
            parent_rotation = r ? r->value : 0.0f;
            sx = s ? s->x : 1.0f;
            sy = s ? s->y : 1.0f;
            cos_r = cosf(parent_rotation);
            sin_r = sinf(parent_rotation);
            cached_parent = direct_parent;
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
    float px = 0.0f, py = 0.0f, pz = 0.0f, sx = 1.0f, sy = 1.0f, sz = 1.0f;
    GlobalOrientation3d parent_orientation = { .x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f };
    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t direct_parent = parents[i].entity;
        if (direct_parent != cached_parent) {
            const ecs_entity_t parent = ecs_has(direct_parent, Position3d)
                                            ? direct_parent
                                            : spatial_3d_transform_parent(it->entities[i]);
            const GlobalPosition3d *p = parent ? ecs_try_get(parent, GlobalPosition3d) : NULL;
            const GlobalOrientation3d *o = parent ? ecs_try_get(parent, GlobalOrientation3d) : NULL;
            const GlobalScale3d *s = parent ? ecs_try_get(parent, GlobalScale3d) : NULL;
            px = p ? p->x : 0.0f;
            py = p ? p->y : 0.0f;
            pz = p ? p->z : 0.0f;
            parent_orientation = o ? *o : (GlobalOrientation3d){ .w = 1.0f };
            sx = s ? s->x : 1.0f;
            sy = s ? s->y : 1.0f;
            sz = s ? s->z : 1.0f;
            cached_parent = direct_parent;
        }
        const Direction3d offset = spatial_3d_rotate_direction(
            &parent_orientation,
            (Direction3d){ position[i].x * sx, position[i].y * sy, position[i].z * sz }
        );
        global_position[i].x = px + offset.x;
        global_position[i].y = py + offset.y;
        global_position[i].z = pz + offset.z;
        const GlobalOrientation3d local = spatial_3d_orientation_from_rotation(&rotation[i]);
        global_orientation[i] = spatial_3d_orientation_multiply(&parent_orientation, &local);
        global_scale[i].x = sx * scale[i].x;
        global_scale[i].y = sy * scale[i].y;
        global_scale[i].z = sz * scale[i].z;
    }
}

/* The queries of one level: changed tables only, or every table. Depth 0
 * holds the entities without ChildOf. */
static void level_queries(spatial_levels_t *levels, uint32_t depth, const ecs_component_t ids[6]) {
    ecs_query_desc_t desc = { 0 };
    for (uint32_t i = 0; i < 3; i++) {
        desc.components[i] = (ecs_access_term_t){ ids[i], EcsIn };
        desc.components[i + 3] = (ecs_access_term_t){ ids[i + 3], EcsInOut };
    }
    desc.relations[0] =
        depth ? (ecs_query_relation_term_t){ (ecs_entity_t)depth, ecs_rid(ChildOf),
                                              EcsRelationDepth }
              : (ecs_query_relation_term_t){ 0, ecs_rid(ChildOf), EcsRelationExcluded };
    levels->all[depth] = ecs_query_init(&desc);
    for (uint32_t i = 0; i < 3; i++) {
        desc.components[i].access |= ECS_TERM_CHANGED;
    }
    levels->changed[depth] = ecs_query_init(&desc);
}

static void propagate_levels(spatial_levels_t *levels, const ecs_component_t ids[6],
                             void (*compute)(ecs_iter_t *)) {
    bool above_changed = false;
    for (uint32_t depth = 0; depth < SPATIAL_MAX_DEPTH; depth++) {
        if (!levels->all[depth]) {
            level_queries(levels, depth, ids);
        }
        /* No entity at this depth: none deeper either. */
        if (depth && !ecs_query_count(levels->all[depth])) {
            return;
        }
        bool level_changed = false;
        ecs_iter_t it = ecs_query_iter(above_changed ? levels->all[depth] : levels->changed[depth]);
        while (ecs_iter_next(&it)) {
            compute(&it);
            level_changed = true;
        }
        if (above_changed) {
            /* The change query records this run too: these changes are done. */
            ecs_iter_t skipped = ecs_query_iter(levels->changed[depth]);
            while (ecs_iter_next(&skipped)) {
            }
        }
        above_changed = above_changed || level_changed;
    }
}

static void spatial_3d_propagation(ecs_iter_t *it) {
    (void)it;
    const ecs_component_t ids[6] = {
        ecs_id(Position3d),       ecs_id(Rotation3d),          ecs_id(Scale3d),
        ecs_id(GlobalPosition3d), ecs_id(GlobalOrientation3d), ecs_id(GlobalScale3d),
    };
    propagate_levels(&levels_3d, ids, spatial_3d_propagate);
}

static void spatial_2d_propagation(ecs_iter_t *it) {
    (void)it;
    const ecs_component_t ids[6] = {
        ecs_id(Position2d),       ecs_id(Rotation2d),       ecs_id(Scale2d),
        ecs_id(GlobalPosition2d), ecs_id(GlobalRotation2d), ecs_id(GlobalScale2d),
    };
    propagate_levels(&levels_2d, ids, spatial_2d_propagate);
}

void spatial_2d_integrate(ecs_iter_t *it) {
    Position2d *restrict position = ecs_field(it, 0);
    const Velocity2d *restrict velocity = ecs_field(it, 1);
    for (uint32_t i = 0; i < it->count; i++) {
        position[i].x += velocity[i].x * it->delta_time;
        position[i].y += velocity[i].y * it->delta_time;
    }
}

void spatial_3d_integrate(ecs_iter_t *it) {
    Position3d *restrict position = ecs_field(it, 0);
    const Velocity3d *restrict velocity = ecs_field(it, 1);
    for (uint32_t i = 0; i < it->count; i++) {
        position[i].x += velocity[i].x * it->delta_time;
        position[i].y += velocity[i].y * it->delta_time;
        position[i].z += velocity[i].z * it->delta_time;
    }
}

static void spatial_fini(void *data) {
    (void)data;
    levels_2d = (spatial_levels_t){ 0 };
    levels_3d = (spatial_levels_t){ 0 };
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
    levels_2d = (spatial_levels_t){ 0 };
    levels_3d = (spatial_levels_t){ 0 };
    ecs_system({ .name = "Spatial2dPropagation", .callback = spatial_2d_propagation,
                 .phase = EcsPostUpdate, .main_thread_only = true, .no_defer = true });
    ecs_system({ .name = "Spatial3dPropagation", .callback = spatial_3d_propagation,
                 .phase = EcsPostUpdate, .main_thread_only = true, .no_defer = true });
    ecs_system({
        .name = "Spatial2dIntegrate",
        .query = { .components = { ecs_inout(Position2d), ecs_in(Velocity2d) } },
        .callback = spatial_2d_integrate,
        .phase = EcsOnUpdate,
    });
    ecs_system({
        .name = "Spatial3dIntegrate",
        .query = { .components = { ecs_inout(Position3d), ecs_in(Velocity3d) } },
        .callback = spatial_3d_integrate,
        .phase = EcsOnUpdate,
    });
    ecs_at_fini({ .callback = spatial_fini });
}
