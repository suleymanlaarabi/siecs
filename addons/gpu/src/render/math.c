#include "backend/backend.h"

sigpu_mat4_t sigpu_mat4_identity(void) {
    sigpu_mat4_t result = { 0 };
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

sigpu_mat4_t sigpu_mat4_mul(sigpu_mat4_t a, sigpu_mat4_t b) {
    sigpu_mat4_t result = { 0 };

    for (int column = 0; column < 4; column++) {
        for (int row = 0; row < 4; row++) {
            result.m[column * 4 + row] =
                a.m[row] * b.m[column * 4] + a.m[4 + row] * b.m[column * 4 + 1] +
                a.m[8 + row] * b.m[column * 4 + 2] + a.m[12 + row] * b.m[column * 4 + 3];
        }
    }

    return result;
}

sigpu_mat4_t sigpu_mat4_perspective_lh(float fov, float aspect, float near_plane, float far_plane) {
    sigpu_mat4_t result = { 0 };
    float y_scale = 1.0f / tanf(fov * SIGPU_PI / 360.0f);
    float x_scale = y_scale / aspect;

    result.m[0] = x_scale;
    result.m[5] = y_scale;
    result.m[10] = far_plane / (far_plane - near_plane);
    result.m[11] = 1.0f;
    result.m[14] = -(near_plane * far_plane) / (far_plane - near_plane);
    return result;
}

sigpu_mat4_t sigpu_mat4_orthographic_lh(
    float left,
    float right,
    float bottom,
    float top,
    float near_plane,
    float far_plane
) {
    sigpu_mat4_t result = { 0 };

    result.m[0] = 2.0f / (right - left);
    result.m[5] = 2.0f / (top - bottom);
    result.m[10] = 1.0f / (far_plane - near_plane);
    result.m[12] = -(right + left) / (right - left);
    result.m[13] = -(top + bottom) / (top - bottom);
    result.m[14] = -near_plane / (far_plane - near_plane);
    result.m[15] = 1.0f;
    return result;
}

sigpu_mat4_t sigpu_mat4_look_at_lh(sigpu_vec3_t eye, sigpu_vec3_t target, sigpu_vec3_t up) {
    sigpu_vec3_t forward = sigpu_vec3_normalize(sigpu_vec3_sub(target, eye));
    sigpu_vec3_t right = sigpu_vec3_normalize(sigpu_vec3_cross(up, forward));
    sigpu_vec3_t camera_up = sigpu_vec3_cross(forward, right);
    sigpu_mat4_t result = { 0 };

    result.m[0] = right.x;
    result.m[1] = camera_up.x;
    result.m[2] = forward.x;
    result.m[4] = right.y;
    result.m[5] = camera_up.y;
    result.m[6] = forward.y;
    result.m[8] = right.z;
    result.m[9] = camera_up.z;
    result.m[10] = forward.z;
    result.m[12] = -sigpu_vec3_dot(right, eye);
    result.m[13] = -sigpu_vec3_dot(camera_up, eye);
    result.m[14] = -sigpu_vec3_dot(forward, eye);
    result.m[15] = 1.0f;
    return result;
}
