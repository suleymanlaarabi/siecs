#include "render/render_internal.h"

static void camera_corners(float aspect, float far_distance, sigpu_vec3_t corners[8]) {
    sigpu_vec3_t forward = sigpu_vec3_normalize(
        sigpu_vec3_sub(SIGPU_RENDERVIEW->camera.target, SIGPU_RENDERVIEW->camera.position)
    );
    sigpu_vec3_t right =
        sigpu_vec3_normalize(sigpu_vec3_cross((sigpu_vec3_t){ 0.0f, 1.0f, 0.0f }, forward));
    sigpu_vec3_t up = sigpu_vec3_cross(forward, right);
    float tangent = tanf(SIGPU_RENDERVIEW->camera.fov * SIGPU_PI / 360.0f);
    float near_height = tangent * SIGPU_RENDERVIEW->camera.near_plane;
    float near_width = near_height * aspect;
    float far_height = tangent * far_distance;
    float far_width = far_height * aspect;
    sigpu_vec3_t near_center = sigpu_vec3_add(
        SIGPU_RENDERVIEW->camera.position,
        sigpu_vec3_scale(forward, SIGPU_RENDERVIEW->camera.near_plane)
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

void sigpu_shadow_bounds_extend(sigpu_vec3_t center, float radius) {
    sigpu_vec3_t light_position = sigpu_mat4_transform_point(SIGPU_RENDERVIEW->light_view, center);

    if (light_position.x + radius >= SIGPU_RENDERVIEW->light_min_x &&
        light_position.x - radius <= SIGPU_RENDERVIEW->light_max_x &&
        light_position.y + radius >= SIGPU_RENDERVIEW->light_min_y &&
        light_position.y - radius <= SIGPU_RENDERVIEW->light_max_y) {
        SIGPU_RENDERVIEW->shadow_minimum_z =
            fminf(SIGPU_RENDERVIEW->shadow_minimum_z, light_position.z - radius);
        SIGPU_RENDERVIEW->shadow_maximum_z =
            fmaxf(SIGPU_RENDERVIEW->shadow_maximum_z, light_position.z + radius);
    }
}

void sigpu_shadow_bounds_begin(float aspect) {
    float shadow_distance =
        fminf(SIGPU_RENDERVIEW->camera.far_plane, SIGPU_RENDERSETTINGS->shadow_distance);
    sigpu_vec3_t corners[8];
    camera_corners(aspect, shadow_distance, corners);
    sigpu_vec3_t center = { 0.0f, 0.0f, 0.0f };

    for (int index = 0; index < 8; index++) {
        center = sigpu_vec3_add(center, sigpu_vec3_scale(corners[index], 0.125f));
    }

    sigpu_vec3_t up = fabsf(SIGPU_RENDERSETTINGS->sun_direction.y) > 0.99f
                          ? (sigpu_vec3_t){ 1.0f, 0.0f, 0.0f }
                          : (sigpu_vec3_t){ 0.0f, 1.0f, 0.0f };
    SIGPU_RENDERVIEW->light_view = sigpu_mat4_look_at_lh(
        center,
        sigpu_vec3_add(center, SIGPU_RENDERSETTINGS->sun_direction),
        up
    );
    SIGPU_RENDERVIEW->light_min_x = INFINITY;
    SIGPU_RENDERVIEW->light_max_x = -INFINITY;
    SIGPU_RENDERVIEW->light_min_y = INFINITY;
    SIGPU_RENDERVIEW->light_max_y = -INFINITY;
    SIGPU_RENDERVIEW->shadow_minimum_z = INFINITY;
    SIGPU_RENDERVIEW->shadow_maximum_z = -INFINITY;

    for (int index = 0; index < 8; index++) {
        sigpu_vec3_t point =
            sigpu_mat4_transform_point(SIGPU_RENDERVIEW->light_view, corners[index]);
        SIGPU_RENDERVIEW->light_min_x = fminf(SIGPU_RENDERVIEW->light_min_x, point.x);
        SIGPU_RENDERVIEW->light_max_x = fmaxf(SIGPU_RENDERVIEW->light_max_x, point.x);
        SIGPU_RENDERVIEW->light_min_y = fminf(SIGPU_RENDERVIEW->light_min_y, point.y);
        SIGPU_RENDERVIEW->light_max_y = fmaxf(SIGPU_RENDERVIEW->light_max_y, point.y);
        SIGPU_RENDERVIEW->shadow_minimum_z = fminf(SIGPU_RENDERVIEW->shadow_minimum_z, point.z);
        SIGPU_RENDERVIEW->shadow_maximum_z = fmaxf(SIGPU_RENDERVIEW->shadow_maximum_z, point.z);
    }

    SIGPU_RENDERVIEW->light_min_x -= 1.0f;
    SIGPU_RENDERVIEW->light_max_x += 1.0f;
    SIGPU_RENDERVIEW->light_min_y -= 1.0f;
    SIGPU_RENDERVIEW->light_max_y += 1.0f;

    float width = SIGPU_RENDERVIEW->light_max_x - SIGPU_RENDERVIEW->light_min_x;
    float height = SIGPU_RENDERVIEW->light_max_y - SIGPU_RENDERVIEW->light_min_y;
    float center_x = (SIGPU_RENDERVIEW->light_min_x + SIGPU_RENDERVIEW->light_max_x) * 0.5f;
    float center_y = (SIGPU_RENDERVIEW->light_min_y + SIGPU_RENDERVIEW->light_max_y) * 0.5f;
    center_x = roundf(center_x / (width / SIGPU_SHADOW_SIZE)) * (width / SIGPU_SHADOW_SIZE);
    center_y = roundf(center_y / (height / SIGPU_SHADOW_SIZE)) * (height / SIGPU_SHADOW_SIZE);
    SIGPU_RENDERVIEW->light_min_x = center_x - width * 0.5f;
    SIGPU_RENDERVIEW->light_max_x = center_x + width * 0.5f;
    SIGPU_RENDERVIEW->light_min_y = center_y - height * 0.5f;
    SIGPU_RENDERVIEW->light_max_y = center_y + height * 0.5f;

    SIGPU_RENDERVIEW->shadow_center = center;
    SIGPU_RENDERVIEW->shadow_up = up;
}

void sigpu_shadow_bounds_end(void) {
    float depth_shift = SIGPU_RENDERVIEW->shadow_minimum_z - 5.0f;
    sigpu_vec3_t light_eye = sigpu_vec3_add(
        SIGPU_RENDERVIEW->shadow_center,
        sigpu_vec3_scale(SIGPU_RENDERSETTINGS->sun_direction, depth_shift)
    );
    SIGPU_RENDERVIEW->light_view = sigpu_mat4_look_at_lh(
        light_eye,
        sigpu_vec3_add(light_eye, SIGPU_RENDERSETTINGS->sun_direction),
        SIGPU_RENDERVIEW->shadow_up
    );
    SIGPU_RENDERVIEW->light_near = 1.0f;
    SIGPU_RENDERVIEW->light_far =
        SIGPU_RENDERVIEW->shadow_maximum_z - SIGPU_RENDERVIEW->shadow_minimum_z + 10.0f;
    sigpu_mat4_t projection = sigpu_mat4_orthographic_lh(
        SIGPU_RENDERVIEW->light_min_x,
        SIGPU_RENDERVIEW->light_max_x,
        SIGPU_RENDERVIEW->light_min_y,
        SIGPU_RENDERVIEW->light_max_y,
        SIGPU_RENDERVIEW->light_near,
        SIGPU_RENDERVIEW->light_far
    );
    SIGPU_RENDERVIEW->light_view_projection =
        sigpu_mat4_mul(projection, SIGPU_RENDERVIEW->light_view);
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
    sigpu_vec3_t position = sigpu_mat4_transform_point(SIGPU_RENDERVIEW->light_view, center);
    return position.x + radius >= SIGPU_RENDERVIEW->light_min_x &&
           position.x - radius <= SIGPU_RENDERVIEW->light_max_x &&
           position.y + radius >= SIGPU_RENDERVIEW->light_min_y &&
           position.y - radius <= SIGPU_RENDERVIEW->light_max_y &&
           position.z + radius >= SIGPU_RENDERVIEW->light_near &&
           position.z - radius <= SIGPU_RENDERVIEW->light_far;
}

void sigpu_static_shadow_bounds_extend(void) {
    for (Uint32 index = 0; index < SIGPU_STATICRENDERCACHE->static_chunk_count; index++) {
        const sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[index];
        if (chunk->radius > 0.0f)
            sigpu_shadow_bounds_extend(chunk->center, chunk->radius);
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
    for (Uint32 i = 0; i < SIGPU_STATICRENDERCACHE->static_chunk_count; i++) {
        sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[i];
        chunk->shadow_visible =
            chunk->radius > 0.0f && sigpu_shadow_visible(chunk->center, chunk->radius);
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
    view->light_view_projection = sigpu_mat4_identity();
}

bool sigpu_visible_camera(GlobalPosition3d p, float radius, float aspect) {
    return sigpu_camera_visible((sigpu_vec3_t){ p.x, p.y, p.z }, radius, aspect);
}

bool sigpu_visible_with_shadows(GlobalPosition3d p, float radius, float aspect) {
    sigpu_vec3_t center = { p.x, p.y, p.z };
    return sigpu_camera_visible(center, radius, aspect) || sigpu_shadow_visible(center, radius);
}
