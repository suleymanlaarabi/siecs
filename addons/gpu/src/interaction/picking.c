#include "picking.h"
#include "backend/backend.h"
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
bool sipicking_ray_sphere(
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
bool sipicking_ray_cylinder(
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

bool sigpu_pointer_ray(float window_x, float window_y, sigpu_ray_t *out) {
    int logical_width, logical_height;
    if (!out || !SIGPU_GPUCONTEXT->window || !SIGPU_FRAMECONTEXT->frame_width ||
        !SIGPU_FRAMECONTEXT->frame_height ||
        !SDL_GetWindowSize(SIGPU_GPUCONTEXT->window, &logical_width, &logical_height) ||
        logical_width <= 0 || logical_height <= 0) {
        return false;
    }

    /* Events use logical coordinates; convert through the current framebuffer
     * dimensions so DPI/resize changes use exactly the rendered aspect. */
    const float pixel_x = window_x * (float)SIGPU_FRAMECONTEXT->frame_width / (float)logical_width;
    const float pixel_y =
        window_y * (float)SIGPU_FRAMECONTEXT->frame_height / (float)logical_height;
    const float ndc_x = pixel_x * 2.0f / (float)SIGPU_FRAMECONTEXT->frame_width - 1.0f;
    const float ndc_y = 1.0f - pixel_y * 2.0f / (float)SIGPU_FRAMECONTEXT->frame_height;
    const sigpu_vec3_t forward = sigpu_vec3_normalize(
        sigpu_vec3_sub(SIGPU_RENDERVIEW->camera.target, SIGPU_RENDERVIEW->camera.position)
    );
    const sigpu_vec3_t right =
        sigpu_vec3_normalize(sigpu_vec3_cross((sigpu_vec3_t){ 0.0f, 1.0f, 0.0f }, forward));
    const sigpu_vec3_t up = sigpu_vec3_cross(forward, right);
    const float tangent = tanf(SIGPU_RENDERVIEW->camera.fov * SIGPU_PI / 360.0f);
    const float aspect =
        (float)SIGPU_FRAMECONTEXT->frame_width / (float)SIGPU_FRAMECONTEXT->frame_height;
    const sigpu_vec3_t direction = sigpu_vec3_normalize(sigpu_vec3_add(
        forward,
        sigpu_vec3_add(
            sigpu_vec3_scale(right, ndc_x * tangent * aspect),
            sigpu_vec3_scale(up, ndc_y * tangent)
        )
    ));
    *out = (sigpu_ray_t){
        SIGPU_RENDERVIEW->camera.position.x,
        SIGPU_RENDERVIEW->camera.position.y,
        SIGPU_RENDERVIEW->camera.position.z,
        direction.x,
        direction.y,
        direction.z,
    };
    return true;
}
