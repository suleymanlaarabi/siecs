#ifndef SIRENDER3D_INTERNAL_H
#define SIRENDER3D_INTERNAL_H

#include <math.h>
#include <sirender3d.h>
#include <stdlib.h>
#include <string.h>

#define R3D_PI 3.14159265358979323846f
#define R3D_SHADOW_SIZE 2048u
#define R3D_CASCADES 3u
#define R3D_PACK "sirender3d://render3d.gpushader"

typedef struct {
    float x, y, z;
} r3d_vec3_t;

typedef struct {
    float m[16];
} r3d_mat4_t;

static inline r3d_vec3_t r3d_add(r3d_vec3_t a, r3d_vec3_t b) {
    return (r3d_vec3_t){ a.x + b.x, a.y + b.y, a.z + b.z };
}
static inline r3d_vec3_t r3d_sub(r3d_vec3_t a, r3d_vec3_t b) {
    return (r3d_vec3_t){ a.x - b.x, a.y - b.y, a.z - b.z };
}
static inline r3d_vec3_t r3d_scale(r3d_vec3_t v, float s) {
    return (r3d_vec3_t){ v.x * s, v.y * s, v.z * s };
}
static inline float r3d_dot(r3d_vec3_t a, r3d_vec3_t b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline r3d_vec3_t r3d_cross(r3d_vec3_t a, r3d_vec3_t b) {
    return (r3d_vec3_t){ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
static inline r3d_vec3_t r3d_normalize(r3d_vec3_t v) {
    const float length = sqrtf(r3d_dot(v, v));
    return length > 0.0f ? r3d_scale(v, 1.0f / length) : v;
}
static inline r3d_vec3_t r3d_transform_point(const r3d_mat4_t *m, r3d_vec3_t p) {
    return (r3d_vec3_t){
        m->m[0] * p.x + m->m[4] * p.y + m->m[8] * p.z + m->m[12],
        m->m[1] * p.x + m->m[5] * p.y + m->m[9] * p.z + m->m[13],
        m->m[2] * p.x + m->m[6] * p.y + m->m[10] * p.z + m->m[14],
    };
}

r3d_mat4_t r3d_identity(void);
r3d_mat4_t r3d_mul(r3d_mat4_t a, r3d_mat4_t b);
r3d_mat4_t r3d_perspective(float fov, float aspect, float near_plane, float far_plane);
r3d_mat4_t r3d_orthographic(float left, float right, float bottom, float top, float near_plane,
                            float far_plane);
r3d_mat4_t r3d_look_at(r3d_vec3_t eye, r3d_vec3_t target, r3d_vec3_t up);
float r3d_linear(uint8_t srgb);

/* Uniform block of the forward programs, the shader's View3d. */
typedef struct {
    float view_projection[16];
    float light_view_projection[3][16];
    float camera_position[4];
    float camera_direction[4];
    float sun_direction_intensity[4];
    float sun_color[4];
    float ambient_color_intensity[4];
    float fog_color[4];
    float fog_parameters[4];
    float shadow_parameters[4];
    float shadow_splits[4];
    float shadow_texel[3][4];
} r3d_view_uniform_t;

_Static_assert(sizeof(r3d_view_uniform_t) == 448, "View3d layout");

/* Instances, as the shader reads them. */
typedef struct {
    float position[3];
    float scale[3];
    uint32_t rotation[2];
    uint32_t color;
    float bloom;
} r3d_primitive_t;

typedef struct {
    float position[3];
    float scale[3];
    uint32_t rotation[2];
} r3d_mesh_t;

/* Internal instance components, registered for the features. */
ECS_COMPONENT_DECLARE(Render3dPrimitiveInstance, {
    float position[3];
    float scale[3];
    uint32_t rotation[2];
    uint32_t color;
    float bloom;
});
ECS_COMPONENT_DECLARE(Render3dMeshInstance, {
    float position[3];
    float scale[3];
    uint32_t rotation[2];
});

typedef struct {
    ecs_entity_t shader;
    ecs_entity_t primitive_program;
    ecs_entity_t mesh_program;
    ecs_entity_t sky_program;
    ecs_entity_t bloom_down_program;
    ecs_entity_t bloom_blur_program;
    ecs_entity_t composite_program;
    ecs_entity_t composite_plain_program;
    ecs_entity_t linear_sampler;
    ecs_entity_t shadow_sampler;
    ecs_entity_t geometries[7];
    ecs_entity_t features[4];
    ecs_entity_t default_material;
} r3d_state_t;

extern r3d_state_t r3d;

void r3d_primitives_init(void);
void r3d_views_init(void);
void r3d_views_fini(void);
void r3d_picking_init(void);
void r3d_picking_fini(void);
uint32_t r3d_pack_rotation(const GlobalOrientation3d *q, uint32_t out[2]);

/* The camera rendering to a surface (first in order) and its matrices. */
typedef struct {
    ecs_entity_t camera;
    ecs_entity_t surface;
    r3d_vec3_t position;
    r3d_vec3_t forward;
    r3d_vec3_t right;
    r3d_vec3_t up;
    float fov;
    float aspect;
    float near_plane;
    float far_plane;
    bool orthographic;
    float viewport[4];
} r3d_camera_t;

bool r3d_camera_for_window(ecs_entity_t window, r3d_camera_t *out);

#endif
