#include "interaction.h"
#include "../input/input.h"
#include "backend/api.h"
#include "picking.h"
#include <float.h>
#include <math.h>
#include <siecs_spatial.h>
#include <stddef.h>
#include <string.h>

ECS_COMPONENT_DEFINE(PointerEvents);

static ecs_event_t pointer_events[SiPointerEventCount] = {
    UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
    UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
};
static ecs_query_id_t picking_queries[3];
static ecs_entity_t hovered;
static ecs_entity_t down_targets[6];
static float down_x[6], down_y[6];
static const float click_distance = 4.0f;

typedef struct {
    const void *data;
    ptrdiff_t stride;
} field_t;
#define FIELD(it, index)                                                                           \
    ((field_t){ ecs_field((it), (index)), ecs_field_is_shared((it), (index)) ? 0 : 1 })
#define AT(type, field, index) (((const type *)(field).data)[(index) * (field).stride])

typedef struct {
    ecs_entity_t entity;
    uint32_t mask;
    sipicking_hit_t hit;
    sigpu_ray_t ray;
    bool hit_any;
} pick_result_t;

static bool ray_sphere(sigpu_ray_t ray, GlobalPosition3d center, float radius) {
    const float ox = ray.ox - center.x, oy = ray.oy - center.y, oz = ray.oz - center.z;
    const float projection = -(ox * ray.dx + oy * ray.dy + oz * ray.dz);
    const float closest2 = ox * ox + oy * oy + oz * oz - projection * projection;
    return closest2 <= radius * radius &&
           projection + sqrtf(fmaxf(radius * radius - closest2, 0.0f)) >= 0.0f;
}

static pick_result_t pick(float x, float y) {
    pick_result_t result = { .hit = { .distance = FLT_MAX } };
    if (!sigpu_pointer_ray(x, y, &result.ray))
        return result;
    const sipicking_ray_t ray = {
        .origin = { result.ray.ox, result.ray.oy, result.ray.oz },
        .direction = { result.ray.dx, result.ray.dy, result.ray.dz },
    };
    for (uint32_t kind = 0; kind < 3; kind++) {
        for (ecs_iter_t it = ecs_query_iter(picking_queries[kind]); ecs_iter_next(&it);) {
            const field_t positions = FIELD(&it, 0), orientations = FIELD(&it, 1);
            const field_t scales = FIELD(&it, 2), shapes = FIELD(&it, 3), masks = FIELD(&it, 4);
            for (uint32_t i = 0; i < it.count; i++) {
                GlobalPosition3d position = AT(GlobalPosition3d, positions, i);
                GlobalOrientation3d orientation = AT(GlobalOrientation3d, orientations, i);
                GlobalScale3d scale = AT(GlobalScale3d, scales, i);
                sipicking_vec3_t half;
                float radius;
                if (kind == 0) {
                    Cuboid c = AT(Cuboid, shapes, i);
                    half = (sipicking_vec3_t){ c.width * scale.x * 0.5f,
                                               c.height * scale.y * 0.5f,
                                               c.depth * scale.z * 0.5f };
                    radius = sqrtf(half.x * half.x + half.y * half.y + half.z * half.z);
                } else if (kind == 1) {
                    Cylinder c = AT(Cylinder, shapes, i);
                    half = (sipicking_vec3_t){ c.radius * scale.x,
                                               c.height * scale.y * 0.5f,
                                               c.radius * scale.z };
                    radius = hypotf(fmaxf(fabsf(half.x), fabsf(half.z)), fabsf(half.y));
                } else {
                    Sphere c = AT(Sphere, shapes, i);
                    half = (sipicking_vec3_t){ c.radius * scale.x,
                                               c.radius * scale.y,
                                               c.radius * scale.z };
                    radius = fmaxf(fabsf(half.x), fmaxf(fabsf(half.y), fabsf(half.z)));
                }
                if (!ray_sphere(result.ray, position, radius))
                    continue;
                sipicking_vec3_t center = { position.x, position.y, position.z };
                sipicking_quat_t q = { orientation.x, orientation.y, orientation.z, orientation.w };
                sipicking_hit_t hit;
                bool found = kind == 0   ? sipicking_ray_obb(ray, center, q, half, &hit)
                             : kind == 1 ? sipicking_ray_cylinder(ray, center, q, half, &hit)
                                         : sipicking_ray_sphere(ray, center, q, half, &hit);
                if (!found)
                    continue;
                ecs_entity_t entity = it.entities[i];
                if (!result.hit_any || hit.distance < result.hit.distance ||
                    (hit.distance == result.hit.distance && entity < result.entity)) {
                    result.entity = entity;
                    result.mask = AT(PointerEvents, masks, i).mask;
                    result.hit = hit;
                    result.hit_any = true;
                }
            }
        }
    }
    return result;
}

static void fill_payload(
    sigpu_pointer_event_t *payload,
    ecs_entity_t target,
    ecs_entity_t related,
    const pick_result_t *pick,
    const siinput_pointer_edge_t *edge,
    const Pointer *pointer
) {
    const float x = edge ? edge->x : pointer->x;
    const float y = edge ? edge->y : pointer->y;
    const uint64_t timestamp = edge ? edge->timestamp_ns : SDL_GetTicksNS();
    const uint32_t pointer_id = edge ? edge->pointer_id : pointer->pointer_id;
    const uint32_t buttons = edge ? edge->buttons : pointer->buttons;
    memset(payload, 0, sizeof(*payload));
    payload->target = target;
    payload->related_target = related;
    payload->timestamp_ns = timestamp;
    payload->pointer_id = pointer_id;
    payload->buttons = buttons;
    payload->modifiers = sigpu_input_modifiers();
    payload->pointer_type = edge ? edge->pointer_type : pointer->pointer_type;
    payload->button = edge ? edge->button : 0;
    payload->clicks = edge ? edge->clicks : 0;
    payload->x = x;
    payload->y = y;
    payload->delta_x = edge ? 0.0f : pointer->delta_x;
    payload->delta_y = edge ? 0.0f : pointer->delta_y;
    payload->wheel_x = edge ? edge->wheel_x : pointer->wheel_x;
    payload->wheel_y = edge ? edge->wheel_y : pointer->wheel_y;
    payload->ray_origin_x = pick->ray.ox;
    payload->ray_origin_y = pick->ray.oy;
    payload->ray_origin_z = pick->ray.oz;
    payload->ray_direction_x = pick->ray.dx;
    payload->ray_direction_y = pick->ray.dy;
    payload->ray_direction_z = pick->ray.dz;
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

static void emit_if(
    ecs_entity_t target,
    uint32_t mask,
    uint32_t wanted,
    uint32_t event,
    ecs_entity_t related,
    const pick_result_t *pick,
    const siinput_pointer_edge_t *edge,
    const Pointer *pointer
) {
    if (!target || !ecs_is_alive(target) || !(mask & wanted))
        return;
    sigpu_pointer_event_t payload;
    fill_payload(&payload, target, related, pick, edge, pointer);
    ecs_observer_trigger(target, pointer_events[event], &payload);
}

static uint32_t target_mask(ecs_entity_t entity) {
    const PointerEvents *events = ecs_get(entity, PointerEvents);
    return events ? events->mask : 0;
}

static void cancel_down(const siinput_pointer_edge_t *edge, const Pointer *pointer) {
    pick_result_t pick_result = pick(edge ? edge->x : pointer->x, edge ? edge->y : pointer->y);
    for (uint32_t button = 1; button < 6; button++) {
        ecs_entity_t target = down_targets[button];
        if (!target)
            continue;
        emit_if(
            target,
            target_mask(target),
            SiPointerCancelMask,
            SiPointerCancel,
            0,
            &pick_result,
            edge,
            pointer
        );
        down_targets[button] = 0;
    }
}

static void pointer_interaction(ecs_iter_t *it) {
    const Pointer *pointer = ecs_get_resource_read(Pointer);
    if (hovered && !ecs_is_alive(hovered))
        hovered = 0;
    const pick_result_t hover = pick(pointer->x, pointer->y);
    const ecs_entity_t next = hover.hit_any ? hover.entity : 0;
    if (hovered != next) {
        if (hovered)
            emit_if(
                hovered,
                target_mask(hovered),
                SiPointerLeaveMask,
                SiPointerLeave,
                next,
                &hover,
                NULL,
                pointer
            );
        if (next)
            emit_if(
                next,
                hover.mask,
                SiPointerEnterMask,
                SiPointerEnter,
                hovered,
                &hover,
                NULL,
                pointer
            );
        hovered = next;
    }
    if (sigpu_input_had_motion() && next)
        emit_if(next, hover.mask, SiPointerMoveMask, SiPointerMove, 0, &hover, NULL, pointer);

    uint32_t count;
    const siinput_pointer_edge_t *edges = sigpu_input_pointer_edges(&count);
    for (uint32_t i = 0; i < count; i++) {
        const siinput_pointer_edge_t *edge = &edges[i];
        if (edge->kind == SiInputEdgeCancel) {
            cancel_down(edge, pointer);
            continue;
        }
        const pick_result_t hit = pick(edge->x, edge->y);
        const ecs_entity_t target = hit.hit_any ? hit.entity : 0;
        if (edge->kind == SiInputEdgeDown) {
            if (edge->button < 6) {
                down_targets[edge->button] = target;
                down_x[edge->button] = edge->x;
                down_y[edge->button] = edge->y;
            }
            if (target)
                emit_if(target, hit.mask, SiPointerDownMask, SiPointerDown, 0, &hit, edge, pointer);
        } else if (edge->kind == SiInputEdgeUp) {
            if (target)
                emit_if(target, hit.mask, SiPointerUpMask, SiPointerUp, 0, &hit, edge, pointer);
            if (edge->button < 6) {
                const ecs_entity_t down = down_targets[edge->button];
                const float dx = edge->x - down_x[edge->button],
                            dy = edge->y - down_y[edge->button];
                if (edge->button == SDL_BUTTON_LEFT && down && ecs_is_alive(down) &&
                    down == target && dx * dx + dy * dy <= click_distance * click_distance) {
                    const uint32_t mask = target_mask(target);
                    emit_if(target, mask, SiClickMask, SiClick, 0, &hit, edge, pointer);
                    emit_if(target, mask, SiPressMask, SiPress, 0, &hit, edge, pointer);
                }
                down_targets[edge->button] = 0;
            }
        } else if (edge->kind == SiInputEdgeWheel && target) {
            emit_if(target, hit.mask, SiWheelMask, SiPointerWheel, 0, &hit, edge, pointer);
        }
    }
}

void sigpu_interaction_init(ecs_system_id_t after) {
    ECS_MODULE_IMPORT(sispatial, { 0 });
    ECS_COMPONENT_REGISTER(PointerEvents);
    for (uint32_t i = 0; i < SiPointerEventCount; i++)
        ecs_event_register(&pointer_events[i]);
    uint16_t shapes[3] = { ecs_id(Cuboid), ecs_id(Cylinder), ecs_id(Sphere) };
    for (uint32_t kind = 0; kind < 3; kind++) {
        picking_queries[kind] = ecs_query_init(&(ecs_query_desc_t){
            .components = {
                { .id = ecs_id(GlobalPosition3d), .access = EcsIn },
                { .id = ecs_id(GlobalOrientation3d), .access = EcsIn },
                { .id = ecs_id(GlobalScale3d), .access = EcsIn },
                { .id = shapes[kind], .access = EcsIn },
                { .id = ecs_id(PointerEvents), .access = EcsIn },
            } });
    }
    ecs_system(
        { .name = "PointerInteraction",
          .phase = EcsPreRender,
          .after = { after },
          .callback = pointer_interaction,
          .main_thread_only = true }
    );
}

uint16_t sigpu_interaction_component_id(const char *name) {
    return strcmp(name, "PointerEvents") == 0 ? ecs_id(PointerEvents) : 0;
}
ecs_event_t sigpu_pointer_event_id(uint32_t kind) {
    return kind < SiPointerEventCount ? pointer_events[kind] : 0;
}
const uint32_t *sigpu_pointer_event_abi(void) {
    static const uint32_t layout[] = {
        sizeof(sigpu_pointer_event_t),
        offsetof(sigpu_pointer_event_t, target),
        offsetof(sigpu_pointer_event_t, related_target),
        offsetof(sigpu_pointer_event_t, timestamp_ns),
        offsetof(sigpu_pointer_event_t, pointer_id),
        offsetof(sigpu_pointer_event_t, buttons),
        offsetof(sigpu_pointer_event_t, modifiers),
        offsetof(sigpu_pointer_event_t, pointer_type),
        offsetof(sigpu_pointer_event_t, button),
        offsetof(sigpu_pointer_event_t, clicks),
        offsetof(sigpu_pointer_event_t, x),
        offsetof(sigpu_pointer_event_t, y),
        offsetof(sigpu_pointer_event_t, delta_x),
        offsetof(sigpu_pointer_event_t, delta_y),
        offsetof(sigpu_pointer_event_t, wheel_x),
        offsetof(sigpu_pointer_event_t, wheel_y),
        offsetof(sigpu_pointer_event_t, ray_origin_x),
        offsetof(sigpu_pointer_event_t, ray_origin_y),
        offsetof(sigpu_pointer_event_t, ray_origin_z),
        offsetof(sigpu_pointer_event_t, ray_direction_x),
        offsetof(sigpu_pointer_event_t, ray_direction_y),
        offsetof(sigpu_pointer_event_t, ray_direction_z),
        offsetof(sigpu_pointer_event_t, point_x),
        offsetof(sigpu_pointer_event_t, point_y),
        offsetof(sigpu_pointer_event_t, point_z),
        offsetof(sigpu_pointer_event_t, normal_x),
        offsetof(sigpu_pointer_event_t, normal_y),
        offsetof(sigpu_pointer_event_t, normal_z),
        offsetof(sigpu_pointer_event_t, distance),
    };
    return layout;
}
