#include "render3d_internal.h"
#include <SDL3/SDL.h>
#include <float.h>
#include <stddef.h>

/*
 * Picking (RFC section 12.4): the candidates come from the bounds the
 * features extracted (their cells for stable ranges), then each is tested
 * exactly against its shape. Pointer events follow sigpu v1.
 */

typedef struct {
    float x, y, z;
} sipicking_vec3_t;

typedef struct {
    float x, y, z, w;
} sipicking_quat_t;

typedef struct {
    sipicking_vec3_t origin, direction;
} sipicking_ray_t;

typedef struct {
    float distance;
    sipicking_vec3_t point, normal;
} sipicking_hit_t;

static sipicking_vec3_t add(sipicking_vec3_t a, sipicking_vec3_t b) {
    return (sipicking_vec3_t){ a.x + b.x, a.y + b.y, a.z + b.z };
}

static sipicking_vec3_t sub(sipicking_vec3_t a, sipicking_vec3_t b) {
    return (sipicking_vec3_t){ a.x - b.x, a.y - b.y, a.z - b.z };
}

static sipicking_vec3_t scale(sipicking_vec3_t v, float s) {
    return (sipicking_vec3_t){ v.x * s, v.y * s, v.z * s };
}

static sipicking_vec3_t rotate(sipicking_quat_t q, sipicking_vec3_t v) {

    const sipicking_vec3_t u = { q.x, q.y, q.z };
    const sipicking_vec3_t uv = { u.y * v.z - u.z * v.y,
                                  u.z * v.x - u.x * v.z,
                                  u.x * v.y - u.y * v.x };
    const sipicking_vec3_t uuv = { u.y * uv.z - u.z * uv.y,
                                   u.z * uv.x - u.x * uv.z,
                                   u.x * uv.y - u.y * uv.x };
    return add(v, add(scale(uv, 2.0f * q.w), scale(uuv, 2.0f)));
}

static sipicking_vec3_t inverse_rotate(sipicking_quat_t q, sipicking_vec3_t v) {
    q.x = -q.x;
    q.y = -q.y;
    q.z = -q.z;
    return rotate(q, v);
}

static bool sipicking_ray_obb(
    sipicking_ray_t ray,
    sipicking_vec3_t center,
    sipicking_quat_t orientation,
    sipicking_vec3_t half,
    sipicking_hit_t *out
) {
    const sipicking_vec3_t origin = inverse_rotate(orientation, sub(ray.origin, center));
    const sipicking_vec3_t direction = inverse_rotate(orientation, ray.direction);
    const float origins[3] = { origin.x, origin.y, origin.z };
    const float directions[3] = { direction.x, direction.y, direction.z };
    const float extents[3] = { fabsf(half.x), fabsf(half.y), fabsf(half.z) };
    float near_t = -FLT_MAX, far_t = FLT_MAX;
    int near_axis = -1, far_axis = -1;
    float near_sign = 0.0f, far_sign = 0.0f;
    for (int axis = 0; axis < 3; axis++) {
        if (extents[axis] <= 0.0f)
            return false;
        if (fabsf(directions[axis]) < 1e-8f) {
            if (origins[axis] < -extents[axis] || origins[axis] > extents[axis])
                return false;
            continue;
        }
        float t1 = (-extents[axis] - origins[axis]) / directions[axis];
        float t2 = (extents[axis] - origins[axis]) / directions[axis];
        float sign1 = -1.0f, sign2 = 1.0f;
        if (t1 > t2) {
            float t = t1;
            t1 = t2;
            t2 = t;
            float s = sign1;
            sign1 = sign2;
            sign2 = s;
        }
        if (t1 > near_t) {
            near_t = t1;
            near_axis = axis;
            near_sign = sign1;
        }
        if (t2 < far_t) {
            far_t = t2;
            far_axis = axis;
            far_sign = sign2;
        }
        if (near_t > far_t)
            return false;
    }
    if (far_t < 0.0f)
        return false;
    const bool inside = near_t < 0.0f;
    const float distance = inside ? far_t : near_t;
    const int normal_axis = inside ? far_axis : near_axis;
    if (normal_axis < 0 || !isfinite(distance))
        return false;
    sipicking_vec3_t local_normal = { 0, 0, 0 };
    ((float *)&local_normal)[normal_axis] = inside ? far_sign : near_sign;
    out->distance = distance;
    out->point = add(ray.origin, scale(ray.direction, distance));
    out->normal = rotate(orientation, local_normal);
    return true;
}

static float dot(sipicking_vec3_t a, sipicking_vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static sipicking_vec3_t normalize(sipicking_vec3_t v) { return scale(v, 1.0f / sqrtf(dot(v, v))); }
static bool local_ray(
    sipicking_ray_t ray,
    sipicking_vec3_t center,
    sipicking_quat_t orientation,
    sipicking_vec3_t radii,
    sipicking_vec3_t *origin,
    sipicking_vec3_t *direction
) {
    if (fabsf(radii.x) < 1e-8f || fabsf(radii.y) < 1e-8f || fabsf(radii.z) < 1e-8f)
        return false;
    sipicking_vec3_t o = inverse_rotate(orientation, sub(ray.origin, center));
    sipicking_vec3_t d = inverse_rotate(orientation, ray.direction);
    *origin = (sipicking_vec3_t){ o.x / radii.x, o.y / radii.y, o.z / radii.z };
    *direction = (sipicking_vec3_t){ d.x / radii.x, d.y / radii.y, d.z / radii.z };
    return true;
}
static void local_hit(
    sipicking_ray_t ray,
    sipicking_quat_t orientation,
    sipicking_vec3_t radii,
    float t,
    sipicking_vec3_t local_normal,
    sipicking_hit_t *out
) {
    out->distance = t;
    out->point = add(ray.origin, scale(ray.direction, t));
    out->normal = normalize(rotate(
        orientation,
        (sipicking_vec3_t){ local_normal.x / radii.x,
                            local_normal.y / radii.y,
                            local_normal.z / radii.z }
    ));
}
static bool sipicking_ray_sphere(
    sipicking_ray_t ray,
    sipicking_vec3_t center,
    sipicking_quat_t orientation,
    sipicking_vec3_t radii,
    sipicking_hit_t *out
) {
    sipicking_vec3_t o, d;
    if (!local_ray(ray, center, orientation, radii, &o, &d))
        return false;
    float a = dot(d, d), b = dot(o, d), c = dot(o, o) - 1.0f;
    float discriminant = b * b - a * c;
    if (a <= 0.0f || discriminant < 0.0f)
        return false;
    float root = sqrtf(discriminant);
    float t = (-b - root) / a;
    if (t < 0.0f)
        t = (-b + root) / a;
    if (t < 0.0f || !isfinite(t))
        return false;
    local_hit(ray, orientation, radii, t, normalize(add(o, scale(d, t))), out);
    return true;
}
static bool sipicking_ray_cylinder(
    sipicking_ray_t ray,
    sipicking_vec3_t center,
    sipicking_quat_t orientation,
    sipicking_vec3_t radii,
    sipicking_hit_t *out
) {
    sipicking_vec3_t o, d;
    if (!local_ray(ray, center, orientation, radii, &o, &d))
        return false;
    float best = FLT_MAX;
    sipicking_vec3_t normal = { 0 };
    float a = d.x * d.x + d.z * d.z;
    if (a > 1e-12f) {
        float b = o.x * d.x + o.z * d.z;
        float c = o.x * o.x + o.z * o.z - 1.0f;
        float discriminant = b * b - a * c;
        if (discriminant >= 0.0f) {
            float root = sqrtf(discriminant);
            float candidates[2] = { (-b - root) / a, (-b + root) / a };
            for (int i = 0; i < 2; i++) {
                float t = candidates[i], y = o.y + t * d.y;
                if (t >= 0.0f && t < best && y >= -1.0f && y <= 1.0f) {
                    best = t;
                    normal = (sipicking_vec3_t){ o.x + t * d.x, 0, o.z + t * d.z };
                }
            }
        }
    }
    if (fabsf(d.y) > 1e-8f) {
        for (int cap = -1; cap <= 1; cap += 2) {
            float t = (cap - o.y) / d.y;
            float x = o.x + t * d.x, z = o.z + t * d.z;
            if (t >= 0.0f && t < best && x * x + z * z <= 1.0f) {
                best = t;
                normal = (sipicking_vec3_t){ 0, (float)cap, 0 };
            }
        }
    }
    if (best == FLT_MAX || !isfinite(best))
        return false;
    local_hit(ray, orientation, radii, best, normal, out);
    return true;
}

ECS_COMPONENT_DEFINE(PointerEvents);

static ecs_event_t pointer_events[SiPointerEventCount] = {
    UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
    UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
};
static ecs_entity_t hovered;
static ecs_entity_t down_targets[6];
static float down_x[6], down_y[6];
static const float click_distance = 4.0f;

typedef struct {
    ecs_entity_t entity;
    uint32_t mask;
    sipicking_hit_t hit;
    float origin[3];
    float direction[3];
    bool hit_any;
} pick_result_t;

typedef struct {
    sipicking_ray_t ray;
    uint32_t kind;
    sipicking_hit_t hit;
} exact_t;

/* Exact test of a candidate that has PointerEvents. */
static bool exact_hit(ecs_entity_t entity, float *distance, void *user_data) {
    exact_t *exact = user_data;
    const GlobalPosition3d *p = ecs_try_get(entity, GlobalPosition3d);
    const GlobalOrientation3d *o = ecs_try_get(entity, GlobalOrientation3d);
    const GlobalScale3d *s = ecs_try_get(entity, GlobalScale3d);
    if (!ecs_has(entity, PointerEvents) || !p || !o || !s) {
        return false;
    }
    const sipicking_vec3_t center = { p->x, p->y, p->z };
    const sipicking_quat_t q = { o->x, o->y, o->z, o->w };
    sipicking_hit_t hit;
    bool found = false;
    if (exact->kind == Render3dCuboids) {
        const Cuboid *c = ecs_try_get(entity, Cuboid);
        found = c && sipicking_ray_obb(exact->ray, center, q,
                                       (sipicking_vec3_t){ c->width * s->x * 0.5f,
                                                           c->height * s->y * 0.5f,
                                                           c->depth * s->z * 0.5f },
                                       &hit);
    } else if (exact->kind == Render3dCylinders) {
        const Cylinder *c = ecs_try_get(entity, Cylinder);
        found = c && sipicking_ray_cylinder(exact->ray, center, q,
                                            (sipicking_vec3_t){ c->radius * s->x,
                                                                c->height * s->y * 0.5f,
                                                                c->radius * s->z },
                                            &hit);
    } else {
        const Sphere *c = ecs_try_get(entity, Sphere);
        found = c && sipicking_ray_sphere(exact->ray, center, q,
                                          (sipicking_vec3_t){ c->radius * s->x, c->radius * s->y,
                                                              c->radius * s->z },
                                          &hit);
    }
    if (found) {
        *distance = hit.distance;
        exact->hit = hit;
    }
    return found;
}

bool sirender3d_pointer_ray(ecs_entity_t window, float x, float y, float origin[3],
                            float direction[3]) {
    r3d_camera_t camera;
    const WindowState *state =
        window && ecs_is_alive(window) ? ecs_try_get(window, WindowState) : NULL;
    if (!state || state->width <= 0 || state->height <= 0 ||
        !r3d_camera_for_window(window, &camera)) {
        return false;
    }
    /* Logical window coordinates to the camera's viewport, then NDC: the
     * same for any pixel density. */
    const float *v = camera.viewport;
    const bool full = !(v[2] > 0.0f && v[3] > 0.0f);
    const float vx = full ? 0.0f : v[0], vy = full ? 0.0f : v[1];
    const float vw = full ? 1.0f : v[2], vh = full ? 1.0f : v[3];
    const float ndc_x = ((x / (float)state->width - vx) / vw) * 2.0f - 1.0f;
    const float ndc_y = 1.0f - ((y / (float)state->height - vy) / vh) * 2.0f;
    r3d_vec3_t from = camera.position, dir;
    if (camera.orthographic) {
        const float height = camera.fov > 0.0f ? camera.fov : 10.0f;
        from = r3d_add(from, r3d_add(r3d_scale(camera.right, ndc_x * height * camera.aspect * 0.5f),
                                     r3d_scale(camera.up, ndc_y * height * 0.5f)));
        dir = camera.forward;
    } else {
        const float tangent = tanf(camera.fov * R3D_PI / 360.0f);
        dir = r3d_normalize(r3d_add(
            camera.forward, r3d_add(r3d_scale(camera.right, ndc_x * tangent * camera.aspect),
                                    r3d_scale(camera.up, ndc_y * tangent))));
    }
    origin[0] = from.x;
    origin[1] = from.y;
    origin[2] = from.z;
    direction[0] = dir.x;
    direction[1] = dir.y;
    direction[2] = dir.z;
    return true;
}

static pick_result_t pick(ecs_entity_t window, float x, float y) {
    pick_result_t result = { .hit = { .distance = FLT_MAX } };
    if (!sirender3d_pointer_ray(window, x, y, result.origin, result.direction)) {
        return result;
    }
    for (uint32_t kind = 0; kind < 3; kind++) {
        exact_t exact = {
            .ray = { { result.origin[0], result.origin[1], result.origin[2] },
                     { result.direction[0], result.direction[1], result.direction[2] } },
            .kind = kind,
        };
        float distance = 0.0f;
        const ecs_entity_t entity = sigpu_feature_raycast(
            r3d.features[kind], result.origin, result.direction, exact_hit, &exact, &distance);
        if (!entity) {
            continue;
        }
        /* The callback ran on every candidate: test the winner again. */
        exact_hit(entity, &distance, &exact);
        if (!result.hit_any || distance < result.hit.distance ||
            (distance == result.hit.distance && entity < result.entity)) {
            result.entity = entity;
            result.mask = ecs_get(entity, PointerEvents)->mask;
            result.hit = exact.hit;
            result.hit_any = true;
        }
    }
    return result;
}

static void fill_payload(sirender3d_pointer_event_t *payload, ecs_entity_t target,
                         ecs_entity_t related, const pick_result_t *pick,
                         const SiPointerEdge *edge, const PointerState *pointer) {
    memset(payload, 0, sizeof(*payload));
    payload->target = target;
    payload->related_target = related;
    payload->timestamp_ns = edge ? edge->timestamp_ns : SDL_GetTicksNS();
    payload->pointer_id = edge ? edge->pointer_id : pointer->pointer_id;
    payload->buttons = edge ? edge->buttons : pointer->buttons;
    payload->modifiers = ecs_get_resource_read(KeyboardState)->modifiers;
    payload->pointer_type = edge ? edge->pointer_type : pointer->pointer_type;
    payload->button = edge ? edge->button : 0;
    payload->clicks = edge ? edge->clicks : 0;
    payload->x = edge ? edge->x : pointer->x;
    payload->y = edge ? edge->y : pointer->y;
    payload->delta_x = edge ? 0.0f : pointer->delta_x;
    payload->delta_y = edge ? 0.0f : pointer->delta_y;
    payload->wheel_x = edge ? edge->wheel_x : pointer->wheel_x;
    payload->wheel_y = edge ? edge->wheel_y : pointer->wheel_y;
    payload->ray_origin_x = pick->origin[0];
    payload->ray_origin_y = pick->origin[1];
    payload->ray_origin_z = pick->origin[2];
    payload->ray_direction_x = pick->direction[0];
    payload->ray_direction_y = pick->direction[1];
    payload->ray_direction_z = pick->direction[2];
    if (pick->hit_any) {
        payload->point_x = pick->hit.point.x;
        payload->point_y = pick->hit.point.y;
        payload->point_z = pick->hit.point.z;
        payload->normal_x = pick->hit.normal.x;
        payload->normal_y = pick->hit.normal.y;
        payload->normal_z = pick->hit.normal.z;
        payload->distance = pick->hit.distance;
    }
}

static void emit_if(ecs_entity_t target, uint32_t mask, uint32_t wanted, uint32_t event,
                    ecs_entity_t related, const pick_result_t *pick, const SiPointerEdge *edge,
                    const PointerState *pointer) {
    if (!target || !ecs_is_alive(target) || !(mask & wanted)) {
        return;
    }
    sirender3d_pointer_event_t payload;
    fill_payload(&payload, target, related, pick, edge, pointer);
    ecs_observer_trigger(target, pointer_events[event], &payload);
}

static uint32_t target_mask(ecs_entity_t entity) {
    const PointerEvents *events = ecs_is_alive(entity) ? ecs_try_get(entity, PointerEvents) : NULL;
    return events ? events->mask : 0;
}

static void cancel_down(ecs_entity_t window, const SiPointerEdge *edge,
                        const PointerState *pointer) {
    const pick_result_t at = pick(window, edge ? edge->x : pointer->x, edge ? edge->y : pointer->y);
    for (uint32_t button = 1; button < 6; button++) {
        const ecs_entity_t target = down_targets[button];
        if (target) {
            emit_if(target, target_mask(target), SiPointerCancelMask, SiPointerCancel, 0, &at, edge,
                    pointer);
            down_targets[button] = 0;
        }
    }
}

static void pointer_interaction(ecs_iter_t *it) {
    (void)it;
    const PointerState *pointer = ecs_get_resource_read(PointerState);
    const ecs_entity_t window = pointer->window;
    if (hovered && !ecs_is_alive(hovered)) {
        hovered = 0;
    }
    const pick_result_t hover = pick(window, pointer->x, pointer->y);
    const ecs_entity_t next = hover.hit_any ? hover.entity : 0;
    if (hovered != next) {
        if (hovered) {
            emit_if(hovered, target_mask(hovered), SiPointerLeaveMask, SiPointerLeave, next,
                    &hover, NULL, pointer);
        }
        if (next) {
            emit_if(next, hover.mask, SiPointerEnterMask, SiPointerEnter, hovered, &hover, NULL,
                    pointer);
        }
        hovered = next;
    }
    if (pointer->moved && next) {
        emit_if(next, hover.mask, SiPointerMoveMask, SiPointerMove, 0, &hover, NULL, pointer);
    }
    for (uint32_t i = 0; i < pointer->edge_count; i++) {
        const SiPointerEdge *edge = &pointer->edges[i];
        if (edge->kind == SiPointerEdgeCancel) {
            cancel_down(window, edge, pointer);
            continue;
        }
        const pick_result_t hit = pick(window, edge->x, edge->y);
        const ecs_entity_t target = hit.hit_any ? hit.entity : 0;
        if (edge->kind == SiPointerEdgeDown) {
            if (edge->button < 6) {
                down_targets[edge->button] = target;
                down_x[edge->button] = edge->x;
                down_y[edge->button] = edge->y;
            }
            emit_if(target, hit.mask, SiPointerDownMask, SiPointerDown, 0, &hit, edge, pointer);
        } else if (edge->kind == SiPointerEdgeUp) {
            emit_if(target, hit.mask, SiPointerUpMask, SiPointerUp, 0, &hit, edge, pointer);
            if (edge->button < 6) {
                const ecs_entity_t down = down_targets[edge->button];
                const float dx = edge->x - down_x[edge->button];
                const float dy = edge->y - down_y[edge->button];
                if (edge->button == SDL_BUTTON_LEFT && down && ecs_is_alive(down) &&
                    down == target && dx * dx + dy * dy <= click_distance * click_distance) {
                    const uint32_t mask = target_mask(target);
                    emit_if(target, mask, SiClickMask, SiClick, 0, &hit, edge, pointer);
                    emit_if(target, mask, SiPressMask, SiPress, 0, &hit, edge, pointer);
                }
                down_targets[edge->button] = 0;
            }
        } else if (edge->kind == SiPointerEdgeWheel) {
            emit_if(target, hit.mask, SiWheelMask, SiPointerWheel, 0, &hit, edge, pointer);
        }
    }
}

void r3d_picking_init(void) {
    ECS_COMPONENT_REGISTER(PointerEvents);
    for (uint32_t i = 0; i < SiPointerEventCount; i++) {
        ecs_event_register(&pointer_events[i]);
    }
    hovered = 0;
    memset(down_targets, 0, sizeof(down_targets));
    ecs_system({ .name = "Render3dPointerInteraction", .phase = EcsPreRender,
                 .query.resources = { { .id = ecs_id(PointerState), .access = EcsIn },
                                      { .id = ecs_id(KeyboardState), .access = EcsIn } },
                 .callback = pointer_interaction, .main_thread_only = true });
}

ecs_event_t sirender3d_pointer_event_id(uint32_t kind) {
    return kind < SiPointerEventCount ? pointer_events[kind] : 0;
}

const uint32_t *sirender3d_pointer_event_abi(void) {
    static const uint32_t layout[] = {
        sizeof(sirender3d_pointer_event_t),
        offsetof(sirender3d_pointer_event_t, target),
        offsetof(sirender3d_pointer_event_t, related_target),
        offsetof(sirender3d_pointer_event_t, timestamp_ns),
        offsetof(sirender3d_pointer_event_t, pointer_id),
        offsetof(sirender3d_pointer_event_t, buttons),
        offsetof(sirender3d_pointer_event_t, modifiers),
        offsetof(sirender3d_pointer_event_t, pointer_type),
        offsetof(sirender3d_pointer_event_t, button),
        offsetof(sirender3d_pointer_event_t, clicks),
        offsetof(sirender3d_pointer_event_t, x),
        offsetof(sirender3d_pointer_event_t, y),
        offsetof(sirender3d_pointer_event_t, delta_x),
        offsetof(sirender3d_pointer_event_t, delta_y),
        offsetof(sirender3d_pointer_event_t, wheel_x),
        offsetof(sirender3d_pointer_event_t, wheel_y),
        offsetof(sirender3d_pointer_event_t, ray_origin_x),
        offsetof(sirender3d_pointer_event_t, ray_origin_y),
        offsetof(sirender3d_pointer_event_t, ray_origin_z),
        offsetof(sirender3d_pointer_event_t, ray_direction_x),
        offsetof(sirender3d_pointer_event_t, ray_direction_y),
        offsetof(sirender3d_pointer_event_t, ray_direction_z),
        offsetof(sirender3d_pointer_event_t, point_x),
        offsetof(sirender3d_pointer_event_t, point_y),
        offsetof(sirender3d_pointer_event_t, point_z),
        offsetof(sirender3d_pointer_event_t, normal_x),
        offsetof(sirender3d_pointer_event_t, normal_y),
        offsetof(sirender3d_pointer_event_t, normal_z),
        offsetof(sirender3d_pointer_event_t, distance),
    };
    return layout;
}
