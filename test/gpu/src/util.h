#ifndef SIGPU_TEST_UTIL_H
#define SIGPU_TEST_UTIL_H

#include "sigpu_internal.h"
#include <bake_test.h>

#define TEST_PACK "test.gpushader"

/* A quad in normalized device coordinates, drawn by the test feature. */
ECS_COMPONENT_DECLARE(Quad, {
    float offset[2];
    float size[2];
    float color[4];
});

/* Same layout as the shader's QuadInstance. */
ECS_COMPONENT_DECLARE(QuadInstance, {
    float offset[2];
    float size[2];
    float color[4];
});

/* Parameters of the quad_material program; tint is packed as float4 and
 * flag as uint, unused stays zero. */
ECS_COMPONENT_DECLARE(QuadMaterial, {
    uint8_t tint[4];
    bool flag;
    ecs_entity_t albedo;
    ecs_entity_t albedo_sampler;
});

ECS_TAG_DECLARE(QuadLayer);

/* Layer key of a layered quad. */
ECS_COMPONENT_DECLARE(QuadOrder, { uint32_t key; });

/* World with sigpu imported and the test pack registered as TEST_PACK.
 * The device is a real SDL GPU device on the offscreen video driver. */
void gpu_test_start(uint32_t budget);
const unsigned char *gpu_test_pack(size_t *size);

ecs_entity_t gpu_test_device(uint8_t frames_in_flight);
ecs_entity_t gpu_test_buffer(uint32_t size);
ecs_entity_t gpu_test_texture(uint32_t width, uint32_t height);
ecs_entity_t gpu_test_shader(const char *path);
ecs_entity_t gpu_test_compute(ecs_entity_t shader, const char *entry);
void gpu_test_frames(uint32_t count);

const GpuResourceState *gpu_test_state(ecs_entity_t resource);
const GpuStats *gpu_test_stats(void);
const GpuSession *gpu_test_session(void);

ecs_entity_t gpu_test_graphics(ecs_entity_t shader, const char *vertex, const char *fragment);
ecs_entity_t gpu_test_target(uint32_t width, uint32_t height);
ecs_entity_t gpu_test_depth(uint32_t width, uint32_t height);
/* Corners (0,0) to (1,1) as two triangles, attribute "corner". */
ecs_entity_t gpu_test_quad_geometry(void);
ecs_entity_t gpu_test_sampler(void);
/* A 1x1 texture of one color. */
ecs_entity_t gpu_test_color_texture(uint32_t rgba);
/* Feature "test::quads" drawing Quad rows; layered features order by
 * QuadOrder. */
ecs_entity_t gpu_test_quad_feature(const char *name, ecs_entity_t geometry, ecs_entity_t program,
                                   uint8_t ordering, bool culling);
ecs_entity_t gpu_test_quad(float x, float y, float w, float h, uint32_t rgba);
/* A graph with one render pass clearing target to clear and drawing phase. */
ecs_entity_t gpu_test_draw_graph(ecs_entity_t target, ecs_entity_t phase, ecs_entity_t view,
                                 ecs_entity_t *pass);
/* Fills the pass with the view tint uniform the quad programs read. */
void gpu_test_tint(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data);

typedef struct {
    ecs_entity_t target, phase, program, geometry, feature, graph, pass;
} scene_t;

/* A device, a 16x16 target, a quad feature drawn by quad_plain in phase
 * "opaque", and a graph drawing that phase into the target. */
scene_t gpu_test_scene(uint8_t ordering, bool culling, ecs_entity_t view);
/* An orthographic view of the [-1, 1] square shifted by shift_x. */
ecs_entity_t gpu_test_view(float shift_x);

/* Downloads through SDL and waits for the GPU. */
bool gpu_test_read_buffer(ecs_entity_t buffer, uint32_t offset, void *out, uint32_t size);
bool gpu_test_read_texture(ecs_entity_t texture, uint32_t width, uint32_t height, void *out);
bool gpu_test_read_raw(SDL_GPUBuffer *buffer, uint32_t offset, void *out, uint32_t size);
/* RGBA8 pixel as 0xAABBGGRR. */
uint32_t gpu_test_pixel(ecs_entity_t texture, uint32_t x, uint32_t y);

#endif
