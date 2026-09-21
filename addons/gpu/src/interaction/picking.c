#include "picking.h"
#include <float.h>
#include <math.h>

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

bool sipicking_ray_obb(
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
