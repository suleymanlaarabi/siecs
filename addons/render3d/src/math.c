#include "render3d_internal.h"

r3d_mat4_t r3d_identity(void) {
    r3d_mat4_t result = { 0 };
    result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
    return result;
}

r3d_mat4_t r3d_mul(r3d_mat4_t a, r3d_mat4_t b) {
    r3d_mat4_t result = { 0 };
    for (int column = 0; column < 4; column++) {
        for (int row = 0; row < 4; row++) {
            result.m[column * 4 + row] =
                a.m[row] * b.m[column * 4] + a.m[4 + row] * b.m[column * 4 + 1] +
                a.m[8 + row] * b.m[column * 4 + 2] + a.m[12 + row] * b.m[column * 4 + 3];
        }
    }
    return result;
}

/* Left-handed, depth 0..1, as SDL GPU expects. */
r3d_mat4_t r3d_perspective(float fov, float aspect, float near_plane, float far_plane) {
    r3d_mat4_t result = { 0 };
    const float y_scale = 1.0f / tanf(fov * R3D_PI / 360.0f);
    result.m[0] = y_scale / aspect;
    result.m[5] = y_scale;
    result.m[10] = far_plane / (far_plane - near_plane);
    result.m[11] = 1.0f;
    result.m[14] = -(near_plane * far_plane) / (far_plane - near_plane);
    return result;
}

r3d_mat4_t r3d_orthographic(float left, float right, float bottom, float top, float near_plane,
                            float far_plane) {
    r3d_mat4_t result = { 0 };
    result.m[0] = 2.0f / (right - left);
    result.m[5] = 2.0f / (top - bottom);
    result.m[10] = 1.0f / (far_plane - near_plane);
    result.m[12] = -(right + left) / (right - left);
    result.m[13] = -(top + bottom) / (top - bottom);
    result.m[14] = -near_plane / (far_plane - near_plane);
    result.m[15] = 1.0f;
    return result;
}

r3d_mat4_t r3d_look_at(r3d_vec3_t eye, r3d_vec3_t target, r3d_vec3_t up) {
    const r3d_vec3_t forward = r3d_normalize(r3d_sub(target, eye));
    const r3d_vec3_t right = r3d_normalize(r3d_cross(up, forward));
    const r3d_vec3_t camera_up = r3d_cross(forward, right);
    r3d_mat4_t result = { 0 };
    result.m[0] = right.x;
    result.m[1] = camera_up.x;
    result.m[2] = forward.x;
    result.m[4] = right.y;
    result.m[5] = camera_up.y;
    result.m[6] = forward.y;
    result.m[8] = right.z;
    result.m[9] = camera_up.z;
    result.m[10] = forward.z;
    result.m[12] = -r3d_dot(right, eye);
    result.m[13] = -r3d_dot(camera_up, eye);
    result.m[14] = -r3d_dot(forward, eye);
    result.m[15] = 1.0f;
    return result;
}

float r3d_linear(uint8_t srgb) {
    const float value = srgb / 255.0f;
    return value <= 0.04045f ? value / 12.92f : powf((value + 0.055f) / 1.055f, 2.4f);
}

static int16_t snorm(float value) {
    const float clamped = value < -1.0f ? -1.0f : value > 1.0f ? 1.0f : value;
    return (int16_t)lroundf(clamped * 32767.0f);
}

uint32_t r3d_pack_rotation(const GlobalOrientation3d *q, uint32_t out[2]) {
    const float length = sqrtf(q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w);
    const float inv = length > 0.0f ? 1.0f / length : 0.0f;
    const uint16_t x = (uint16_t)snorm(q->x * inv), y = (uint16_t)snorm(q->y * inv);
    const uint16_t z = (uint16_t)snorm(q->z * inv), w = (uint16_t)snorm(length > 0 ? q->w * inv : 1);
    out[0] = (uint32_t)x | (uint32_t)y << 16;
    out[1] = (uint32_t)z | (uint32_t)w << 16;
    return out[0];
}
