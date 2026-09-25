#include "render/render_internal.h"

static void camera_corners(float aspect, float near_distance, float far_distance, sigpu_vec3_t corners[8]) {
    sigpu_vec3_t forward = sigpu_vec3_normalize(
        sigpu_vec3_sub(SIGPU_RENDERVIEW->camera.target, SIGPU_RENDERVIEW->camera.position)
    );
    sigpu_vec3_t right =
        sigpu_vec3_normalize(sigpu_vec3_cross((sigpu_vec3_t){ 0.0f, 1.0f, 0.0f }, forward));
    sigpu_vec3_t up = sigpu_vec3_cross(forward, right);
    float tangent = tanf(SIGPU_RENDERVIEW->camera.fov * SIGPU_PI / 360.0f);
    float near_height = tangent * near_distance;
    float near_width = near_height * aspect;
    float far_height = tangent * far_distance;
    float far_width = far_height * aspect;
    sigpu_vec3_t near_center = sigpu_vec3_add(
        SIGPU_RENDERVIEW->camera.position,
        sigpu_vec3_scale(forward, near_distance)
    );
    sigpu_vec3_t far_center =
        sigpu_vec3_add(SIGPU_RENDERVIEW->camera.position, sigpu_vec3_scale(forward, far_distance));

    corners[0] = sigpu_vec3_add(
        sigpu_vec3_add(near_center, sigpu_vec3_scale(right, -near_width)),
        sigpu_vec3_scale(up, -near_height)
    );
    corners[1] = sigpu_vec3_add(
        sigpu_vec3_add(near_center, sigpu_vec3_scale(right, near_width)),
        sigpu_vec3_scale(up, -near_height)
    );
    corners[2] = sigpu_vec3_add(
        sigpu_vec3_add(near_center, sigpu_vec3_scale(right, near_width)),
        sigpu_vec3_scale(up, near_height)
    );
    corners[3] = sigpu_vec3_add(
        sigpu_vec3_add(near_center, sigpu_vec3_scale(right, -near_width)),
        sigpu_vec3_scale(up, near_height)
    );
    corners[4] = sigpu_vec3_add(
        sigpu_vec3_add(far_center, sigpu_vec3_scale(right, -far_width)),
        sigpu_vec3_scale(up, -far_height)
    );
    corners[5] = sigpu_vec3_add(
        sigpu_vec3_add(far_center, sigpu_vec3_scale(right, far_width)),
        sigpu_vec3_scale(up, -far_height)
    );
    corners[6] = sigpu_vec3_add(
        sigpu_vec3_add(far_center, sigpu_vec3_scale(right, far_width)),
        sigpu_vec3_scale(up, far_height)
    );
    corners[7] = sigpu_vec3_add(
        sigpu_vec3_add(far_center, sigpu_vec3_scale(right, -far_width)),
        sigpu_vec3_scale(up, far_height)
    );
}

void sigpu_shadow_bounds_extend(sigpu_vec3_t center, float radius, bool is_static) {
    RenderView *view = SIGPU_RENDERVIEW;
    for (Uint32 i = 0; i < view->cascade_count; i++) {
        if (!is_static && i != 0)
            break;
        sigpu_shadow_cascade_t *cascade = &view->cascades[i];
        sigpu_vec3_t p = sigpu_mat4_transform_point(cascade->view, center);
        if (p.x + radius < cascade->min_x || p.x - radius > cascade->max_x ||
            p.y + radius < cascade->min_y || p.y - radius > cascade->max_y)
            continue;
        cascade->minimum_z = fminf(cascade->minimum_z, p.z - radius);
        cascade->maximum_z = fmaxf(cascade->maximum_z, p.z + radius);
    }
}

void sigpu_shadow_bounds_begin(float aspect) {
    RenderView *view = SIGPU_RENDERVIEW;
    float distance = fminf(view->camera.far_plane, SIGPU_RENDERSETTINGS->shadow_distance);
    float ends[SIGPU_SHADOW_CASCADES] = { fminf(distance, 120.0f),
                                           fminf(distance, 300.0f), distance };
    view->cascade_count = distance > view->camera.near_plane ? 1 : 0;
    if (distance > 120.0f) view->cascade_count++;
    if (distance > 300.0f) view->cascade_count++;
    sigpu_vec3_t up = fabsf(SIGPU_RENDERSETTINGS->sun_direction.y) > 0.99f
                          ? (sigpu_vec3_t){ 1.0f, 0.0f, 0.0f }
                          : (sigpu_vec3_t){ 0.0f, 1.0f, 0.0f };
    sigpu_mat4_t orientation = sigpu_mat4_look_at_lh(
        (sigpu_vec3_t){ 0.0f, 0.0f, 0.0f }, SIGPU_RENDERSETTINGS->sun_direction, up
    );
    float tangent = tanf(view->camera.fov * SIGPU_PI / 360.0f);
    for (Uint32 i = 0; i < view->cascade_count; i++) {
        sigpu_shadow_cascade_t *cascade = &view->cascades[i];
        float near = i ? ends[i - 1] : view->camera.near_plane;
        float far = ends[i];
        cascade->split_near = near;
        cascade->split_far = far;
        float overlap = i ? fminf(10.0f, (far - near) * 0.1f) : 0.0f;
        sigpu_vec3_t corners[8];
        camera_corners(aspect, fmaxf(view->camera.near_plane, near - overlap), far, corners);
        sigpu_vec3_t center = { 0 };
        for (int c = 0; c < 8; c++)
            center = sigpu_vec3_add(center, sigpu_vec3_scale(corners[c], 0.125f));
        float lateral = tangent * far * sqrtf(1.0f + aspect * aspect);
        float longitudinal = (far - fmaxf(view->camera.near_plane, near - overlap)) * 0.5f;
        float half_extent = sqrtf(lateral * lateral + longitudinal * longitudinal);
        half_extent /= 1.0f - 8.0f / SIGPU_SHADOW_SIZE;
        float texel = 2.0f * half_extent / SIGPU_SHADOW_SIZE;
        sigpu_vec3_t light_center = sigpu_mat4_transform_point(orientation, center);
        float dx = roundf(light_center.x / texel) * texel - light_center.x;
        float dy = roundf(light_center.y / texel) * texel - light_center.y;
        sigpu_vec3_t right = { orientation.m[0], orientation.m[4], orientation.m[8] };
        sigpu_vec3_t light_up = { orientation.m[1], orientation.m[5], orientation.m[9] };
        center = sigpu_vec3_add(center, sigpu_vec3_add(sigpu_vec3_scale(right, dx), sigpu_vec3_scale(light_up, dy)));
        cascade->center = center;
        cascade->up = up;
        cascade->view = sigpu_mat4_look_at_lh(center, sigpu_vec3_add(center, SIGPU_RENDERSETTINGS->sun_direction), up);
        cascade->min_x = cascade->min_y = -half_extent;
        cascade->max_x = cascade->max_y = half_extent;
        cascade->minimum_z = INFINITY;
        cascade->maximum_z = -INFINITY;
    }
}

void sigpu_shadow_bounds_end(void) {
    RenderView *view = SIGPU_RENDERVIEW;
    for (Uint32 i = 0; i < view->cascade_count; i++) {
        sigpu_shadow_cascade_t *cascade = &view->cascades[i];
        if (!isfinite(cascade->minimum_z)) {
            cascade->minimum_z = 0.0f;
            cascade->maximum_z = 1.0f;
        }
        float shift = cascade->minimum_z - 5.0f;
        sigpu_vec3_t eye = sigpu_vec3_add(cascade->center, sigpu_vec3_scale(SIGPU_RENDERSETTINGS->sun_direction, shift));
        cascade->view = sigpu_mat4_look_at_lh(eye, sigpu_vec3_add(eye, SIGPU_RENDERSETTINGS->sun_direction), cascade->up);
        cascade->near_plane = 1.0f;
        cascade->far_plane = cascade->maximum_z - cascade->minimum_z + 10.0f;
        sigpu_mat4_t projection = sigpu_mat4_orthographic_lh(
            cascade->min_x, cascade->max_x, cascade->min_y, cascade->max_y,
            cascade->near_plane, cascade->far_plane
        );
        cascade->view_projection = sigpu_mat4_mul(projection, cascade->view);
    }
}

bool sigpu_camera_visible(sigpu_vec3_t center, float radius, float aspect) {
    const RenderView *view = ecs_get_resource_read(RenderView);
    for (Uint32 i = 0; i < 6; i++) {
        const float *plane = view->frustum_planes[i];
        if (plane[0] * center.x + plane[1] * center.y + plane[2] * center.z + plane[3] < -radius)
            return false;
    }
    return true;
}

bool sigpu_shadow_visible(sigpu_vec3_t center, float radius) {
    if (!SIGPU_RENDERVIEW->cascade_count) return false;
    const sigpu_shadow_cascade_t *cascade = &SIGPU_RENDERVIEW->cascades[0];
    sigpu_vec3_t p = sigpu_mat4_transform_point(cascade->view, center);
    return p.x + radius >= cascade->min_x && p.x - radius <= cascade->max_x &&
           p.y + radius >= cascade->min_y && p.y - radius <= cascade->max_y &&
           p.z + radius >= cascade->near_plane && p.z - radius <= cascade->far_plane;
}

void sigpu_static_shadow_bounds_extend(void) {
    for (Uint32 index = 0; index < SIGPU_STATICRENDERCACHE->static_chunk_count; index++) {
        const sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[index];
        if (chunk->radius > 0.0f)
            sigpu_shadow_bounds_extend(chunk->center, chunk->radius, true);
    }
}

void sigpu_static_cull(float aspect) {
    SIGPU_STATICRENDERCACHE->static_camera_visible_count = 0;

    for (Uint32 index = 0; index < SIGPU_STATICRENDERCACHE->static_chunk_count; index++) {
        sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[index];
        chunk->camera_visible =
            chunk->radius > 0.0f && sigpu_camera_visible(chunk->center, chunk->radius, aspect);
        SIGPU_STATICRENDERCACHE->static_camera_visible_count += chunk->camera_visible;
        SIGPU_RENDERQUEUE->any_bloom =
            SIGPU_RENDERQUEUE->any_bloom || (chunk->camera_visible && chunk->bloom);
    }
}

static void
camera_plane(float out[4], const sigpu_mat4_t *matrix, float x, float y, float z, float bias) {
    out[0] = x * matrix->m[0] + y * matrix->m[1] + z * matrix->m[2];
    out[1] = x * matrix->m[4] + y * matrix->m[5] + z * matrix->m[6];
    out[2] = x * matrix->m[8] + y * matrix->m[9] + z * matrix->m[10];
    out[3] = x * matrix->m[12] + y * matrix->m[13] + z * matrix->m[14] + bias;
    float length = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    for (int i = 0; i < 4; i++)
        out[i] /= length;
}

void sigpu_static_shadow_cull(void) {
    SIGPU_STATICRENDERCACHE->static_shadow_visible_count = 0;
    SDL_memset(SIGPU_STATICRENDERCACHE->shadow_visible_count, 0, sizeof(SIGPU_STATICRENDERCACHE->shadow_visible_count));
    for (Uint32 i = 0; i < SIGPU_STATICRENDERCACHE->static_chunk_count; i++) {
        sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[i];
        chunk->shadow_mask = 0;
        for (Uint32 c = 0; c < SIGPU_RENDERVIEW->cascade_count; c++) {
            const sigpu_shadow_cascade_t *cascade = &SIGPU_RENDERVIEW->cascades[c];
            sigpu_vec3_t p = sigpu_mat4_transform_point(cascade->view, chunk->center);
            bool visible = chunk->radius > 0.0f &&
                p.x + chunk->radius >= cascade->min_x && p.x - chunk->radius <= cascade->max_x &&
                p.y + chunk->radius >= cascade->min_y && p.y - chunk->radius <= cascade->max_y &&
                p.z + chunk->radius >= cascade->near_plane && p.z - chunk->radius <= cascade->far_plane;
            if (visible) {
                chunk->shadow_mask |= 1u << c;
                SIGPU_STATICRENDERCACHE->shadow_visible_count[c]++;
            }
        }
        chunk->shadow_visible = chunk->shadow_mask != 0;
        SIGPU_STATICRENDERCACHE->static_shadow_visible_count += chunk->shadow_visible;
    }
}

void sigpu_view_prepare(float aspect) {
    RenderView *view = SIGPU_RENDERVIEW;
    view->view = sigpu_mat4_look_at_lh(
        view->camera.position,
        view->camera.target,
        (sigpu_vec3_t){ 0.0f, 1.0f, 0.0f }
    );
    view->projection = sigpu_mat4_perspective_lh(
        view->camera.fov,
        aspect,
        view->camera.near_plane,
        view->camera.far_plane
    );
    view->view_projection = sigpu_mat4_mul(view->projection, view->view);
    view->near_plane = view->camera.near_plane;
    view->far_plane = view->camera.far_plane;
    float slope_y = tanf(view->camera.fov * SIGPU_PI / 360.0f);
    float slope_x = slope_y * aspect;
    view->lod_scale = SIGPU_FRAMECONTEXT->frame_height / (2.0f * slope_y);
    camera_plane(view->frustum_planes[0], &view->view, 0, 0, 1, -view->near_plane);
    camera_plane(view->frustum_planes[1], &view->view, 0, 0, -1, view->far_plane);
    camera_plane(view->frustum_planes[2], &view->view, 1, 0, slope_x, 0);
    camera_plane(view->frustum_planes[3], &view->view, -1, 0, slope_x, 0);
    camera_plane(view->frustum_planes[4], &view->view, 0, 1, slope_y, 0);
    camera_plane(view->frustum_planes[5], &view->view, 0, -1, slope_y, 0);
}
