#ifndef SIGPU_INTERNAL_H
#define SIGPU_INTERNAL_H

#include "sigpu.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdint.h>

#define SIGPU_AXIS_CAPACITY 262144
#define SIGPU_ROTATED_CAPACITY 32768
#define SIGPU_SHADOW_SIZE 2048
#define SIGPU_HDR_FORMAT SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT
#define SIGPU_PI 3.14159265358979323846f
#define SIGPU_STATIC_CHUNK_SIZE 64.0f
#define SIGPU_LOD_LOW_MAX_PIXELS 8.0f
#define SIGPU_LOD_MEDIUM_MAX_PIXELS 32.0f
#define SIGPU_SHADER(name) name
typedef enum {
    SIGPU_MESH_CUBE,
    SIGPU_MESH_CYLINDER_LOW,
    SIGPU_MESH_CYLINDER_MEDIUM,
    SIGPU_MESH_CYLINDER_HIGH,
    SIGPU_MESH_SPHERE_LOW,
    SIGPU_MESH_SPHERE_MEDIUM,
    SIGPU_MESH_SPHERE_HIGH,
    SIGPU_MESH_COUNT
} sigpu_mesh_id_t;
typedef enum {
    SIGPU_PRIMITIVE_CUBE,
    SIGPU_PRIMITIVE_CYLINDER,
    SIGPU_PRIMITIVE_SPHERE,
    SIGPU_PRIMITIVE_COUNT
} sigpu_primitive_t;
typedef struct {
    Uint32 first_index, index_count;
    Sint32 vertex_offset;
} sigpu_mesh_t;
typedef struct {
    float x;
    float y;
    float z;
} sigpu_vec3_t;

typedef struct {
    float m[16];
} sigpu_mat4_t;

typedef struct {
    float x;
    float y;
    float z;
    int8_t nx;
    int8_t ny;
    int8_t nz;
    int8_t nw;
} sigpu_mesh_vertex_t;

typedef struct {
    float x;
    float y;
    float z;
    float width;
    float height;
    float depth;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    float bloom;
} sigpu_axis_instance_t;

typedef struct {
    float x;
    float y;
    float z;
    float width;
    float height;
    float depth;
    int16_t qx;
    int16_t qy;
    int16_t qz;
    int16_t qw;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    float bloom;
} sigpu_rotated_instance_t;

typedef struct {
    float x;
    float y;
    float z;
    float scale_x;
    float scale_y;
    float scale_z;
} sigpu_shared_axis_instance_t;

typedef struct {
    float x;
    float y;
    float z;
    float scale_x;
    float scale_y;
    float scale_z;
    int16_t qx;
    int16_t qy;
    int16_t qz;
    int16_t qw;
} sigpu_shared_rotated_instance_t;

typedef struct {
    float size_bloom[4];
    float color[4];
} sigpu_shared_material_t;

typedef struct {
    sigpu_shared_material_t material;
    Uint32 first;
    Uint32 count;
    uint16_t mesh;
    uint8_t lod;
    uint8_t rotated;
} sigpu_shared_batch_t;
typedef struct {
    Uint32 first, count;
    uint16_t mesh;
} sigpu_owned_batch_t;
typedef struct {
    Uint32 first, count;
} sigpu_static_range_t;

typedef struct {
    sigpu_vec3_t center;
    float radius;
    sigpu_static_range_t axis[SIGPU_PRIMITIVE_COUNT];
    sigpu_static_range_t rotated[SIGPU_PRIMITIVE_COUNT];
    float primitive_radius[SIGPU_PRIMITIVE_COUNT];
    sigpu_vec3_t primitive_center[SIGPU_PRIMITIVE_COUNT];
    bool bloom;
    bool camera_visible;
    bool shadow_visible;
} sigpu_static_chunk_t;

typedef struct {
    const sigpu_axis_instance_t *axis[SIGPU_PRIMITIVE_COUNT];
    Uint32 axis_count[SIGPU_PRIMITIVE_COUNT];
    const sigpu_rotated_instance_t *rotated[SIGPU_PRIMITIVE_COUNT];
    Uint32 rotated_count[SIGPU_PRIMITIVE_COUNT];
    const sigpu_static_chunk_t *chunks;
    Uint32 chunk_count;
} sigpu_static_upload_t;

typedef struct {
    sigpu_vec3_t position;
    sigpu_vec3_t target;
    float fov;
    float near_plane;
    float far_plane;
} sigpu_camera_t;

typedef struct {
    sigpu_mat4_t view_projection;
    sigpu_mat4_t light_view_projection;
} sigpu_transform_uniform_t;

typedef struct {
    float camera_position[4];
    float sun_direction_intensity[4];
    float sun_color[4];
    float ambient_color_intensity[4];
    float fog_color[4];
    float fog_parameters[4];
    float shadow_parameters[4];
} sigpu_lighting_uniform_t;

typedef struct {
    SDL_Window *window;
    SDL_GPUDevice *device;
    SDL_GPUCommandBuffer *command_buffer;
    SDL_GPUTexture *swapchain;
    SDL_GPUTextureFormat swapchain_format;
    bool linear_swapchain;

    SDL_GPUGraphicsPipeline *axis_pipeline;
    SDL_GPUGraphicsPipeline *rotated_pipeline;
    SDL_GPUGraphicsPipeline *shared_axis_pipeline;
    SDL_GPUGraphicsPipeline *shared_rotated_pipeline;
    SDL_GPUGraphicsPipeline *axis_shadow_pipeline;
    SDL_GPUGraphicsPipeline *rotated_shadow_pipeline;
    SDL_GPUGraphicsPipeline *shared_axis_shadow_pipeline;
    SDL_GPUGraphicsPipeline *shared_rotated_shadow_pipeline;
    SDL_GPUGraphicsPipeline *bloom_down_pipeline;
    SDL_GPUGraphicsPipeline *bloom_blur_pipeline;
    SDL_GPUGraphicsPipeline *bloom_composite_pipeline;

    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    sigpu_mesh_t meshes[SIGPU_MESH_COUNT];
    SDL_GPUBuffer *axis_buffer;
    SDL_GPUBuffer *rotated_buffer;
    SDL_GPUBuffer *static_axis_buffer[SIGPU_PRIMITIVE_COUNT];
    SDL_GPUBuffer *static_rotated_buffer[SIGPU_PRIMITIVE_COUNT];
    SDL_GPUTransferBuffer *axis_transfer;
    SDL_GPUTransferBuffer *rotated_transfer;

    SDL_GPUTexture *depth_texture;
    SDL_GPUTexture *msaa_texture;
    SDL_GPUTexture *bloom_msaa_texture;
    SDL_GPUTexture *scene_texture;
    SDL_GPUTexture *bloom_texture;
    SDL_GPUTexture *bloom_half;
    SDL_GPUTexture *bloom_half_scratch;
    SDL_GPUTexture *bloom_quarter;
    SDL_GPUTexture *bloom_quarter_scratch;
    SDL_GPUTexture *shadow_texture;
    SDL_GPUSampler *shadow_sampler;
    SDL_GPUSampler *bloom_sampler;

    void *axis_mapped;
    void *rotated_mapped;
    sigpu_shared_batch_t *shared_axis_batches;
    sigpu_shared_batch_t *shared_rotated_batches;
    sigpu_owned_batch_t *owned_axis_batches;
    sigpu_owned_batch_t *owned_rotated_batches;
    sigpu_static_chunk_t *static_chunks;
    Uint32 shared_axis_count;
    Uint32 shared_rotated_count;
    Uint32 owned_axis_count;
    Uint32 owned_rotated_count;
    Uint32 shared_axis_batch_count;
    Uint32 shared_rotated_batch_count;
    Uint32 shared_axis_batch_capacity;
    Uint32 shared_rotated_batch_capacity;
    Uint32 owned_axis_batch_count, owned_rotated_batch_count;
    Uint32 owned_axis_batch_capacity, owned_rotated_batch_capacity;
    Uint32 axis_capacity;
    Uint32 rotated_capacity;
    Uint32 static_chunk_count;
    Uint32 static_shadow_visible_count;
    Uint32 static_camera_visible_count;
    Uint32 draw_calls;
    Uint32 drawn_instances;
    Uint64 frame_start_ns;
    Uint64 acquire_ns;
    Uint64 collect_ns;
    Uint64 cull_ns;
    Uint64 encode_ns;

    Uint32 frame_width;
    Uint32 frame_height;
    Uint32 target_width;
    Uint32 target_height;
    Uint32 bloom_half_width;
    Uint32 bloom_half_height;
    Uint32 bloom_quarter_width;
    Uint32 bloom_quarter_height;
    SDL_GPUSampleCount sample_count;
    SDL_GPUTextureFormat depth_format;

    sigpu_camera_t camera;
    sigpu_vec3_t sun_direction;
    SDL_FColor sun_color;
    SDL_FColor ambient_color;
    SDL_FColor fog_color;
    SDL_FColor sky_linear;
    SDL_FColor sky_srgb;
    float sun_intensity;
    float ambient_intensity;
    float fog_start;
    float fog_end;
    float shadow_distance;
    float bloom_threshold;
    float bloom_intensity;
    bool fog_enabled;
    bool shadows_enabled;
    bool bloom_enabled;
    bool any_bloom;
    bool profile_enabled;

    uint8_t linear_lut[256];
    sigpu_mat4_t view;
    sigpu_mat4_t view_projection;
    sigpu_mat4_t light_view;
    sigpu_mat4_t light_view_projection;
    float light_min_x;
    float light_max_x;
    float light_min_y;
    float light_max_y;
    float light_near;
    float light_far;
    sigpu_vec3_t shadow_center;
    sigpu_vec3_t shadow_up;
    float shadow_minimum_z;
    float shadow_maximum_z;
} sigpu_state_t;

extern sigpu_state_t g_sigpu;

static inline sigpu_vec3_t sigpu_vec3_add(sigpu_vec3_t a, sigpu_vec3_t b) {
    return (sigpu_vec3_t){ a.x + b.x, a.y + b.y, a.z + b.z };
}

static inline sigpu_vec3_t sigpu_vec3_sub(sigpu_vec3_t a, sigpu_vec3_t b) {
    return (sigpu_vec3_t){ a.x - b.x, a.y - b.y, a.z - b.z };
}

static inline sigpu_vec3_t sigpu_vec3_scale(sigpu_vec3_t value, float scale) {
    return (sigpu_vec3_t){ value.x * scale, value.y * scale, value.z * scale };
}

static inline float sigpu_vec3_dot(sigpu_vec3_t a, sigpu_vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline sigpu_vec3_t sigpu_vec3_cross(sigpu_vec3_t a, sigpu_vec3_t b) {
    return (sigpu_vec3_t){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

static inline sigpu_vec3_t sigpu_vec3_normalize(sigpu_vec3_t value) {
    return sigpu_vec3_scale(value, 1.0f / sqrtf(sigpu_vec3_dot(value, value)));
}

static inline sigpu_vec3_t sigpu_mat4_transform_point(sigpu_mat4_t matrix, sigpu_vec3_t point) {
    return (sigpu_vec3_t){
        matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
        matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
        matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14],
    };
}

static inline Uint32 sigpu_primitive_lod(sigpu_vec3_t center, float radius) {
    sigpu_vec3_t view = sigpu_mat4_transform_point(g_sigpu.view, center);
    float depth = fmaxf(view.z, g_sigpu.camera.near_plane);
    float pixels = radius / depth * g_sigpu.frame_height /
                   (2.0f * tanf(g_sigpu.camera.fov * SIGPU_PI / 360.0f));
    return pixels < SIGPU_LOD_LOW_MAX_PIXELS ? 0 : pixels < SIGPU_LOD_MEDIUM_MAX_PIXELS ? 1 : 2;
}

static inline float sigpu_cube_radius(float width, float height, float depth) {
    return 0.5f * sqrtf(width * width + height * height + depth * depth);
}

#ifdef __cplusplus
extern "C" {
#endif

sigpu_mat4_t sigpu_mat4_identity(void);
sigpu_mat4_t sigpu_mat4_mul(sigpu_mat4_t a, sigpu_mat4_t b);
sigpu_mat4_t sigpu_mat4_perspective_lh(float fov, float aspect, float near_plane, float far_plane);
sigpu_mat4_t sigpu_mat4_orthographic_lh(
    float left,
    float right,
    float bottom,
    float top,
    float near_plane,
    float far_plane
);
sigpu_mat4_t sigpu_mat4_look_at_lh(sigpu_vec3_t eye, sigpu_vec3_t target, sigpu_vec3_t up);

void sigpu_pipelines_create(void);
void sigpu_pipelines_destroy(void);
void sigpu_main_pipelines_recreate(void);

void sigpu_resources_create(int samples);
void sigpu_resources_destroy(void);
void sigpu_axis_instances_grow(void);
void sigpu_rotated_instances_grow(void);
void sigpu_static_upload(const sigpu_static_upload_t *upload);
void sigpu_static_shadow_bounds_extend(void);
void sigpu_static_cull(float aspect);
void sigpu_frame_targets_prepare(void);
void sigpu_sample_count_set(int samples);

void sigpu_view_prepare(float aspect);
void sigpu_shadow_bounds_begin(float aspect);
void sigpu_shadow_bounds_extend(sigpu_vec3_t center, float radius);
void sigpu_shadow_bounds_end(void);
bool sigpu_camera_visible(sigpu_vec3_t center, float radius, float aspect);
bool sigpu_shadow_visible(sigpu_vec3_t center, float radius);
void sigpu_passes_draw(void);

#ifdef __cplusplus
}
#endif

#endif
