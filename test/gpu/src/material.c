#include "util.h"

#define RED 0xff0000ffu
#define GREEN 0xff00ff00u
#define BLUE 0xffff0000u
#define BLACK 0xff000000u


static ecs_entity_t material_program(scene_t *s) {
    ecs_entity_t shader = ecs_get(s->program, GpuProgram)->shader;
    ecs_entity_t textured = ecs_new();
    ecs_set(textured, GpuProgram, { .shader = shader, .vertex = "quad_vertex",
                                    .fragment = "quad_material", .phase = s->phase });
    test_true(sigpu_program_params(textured, ecs_id(QuadMaterial)));
    return textured;
}

void material_isa_override(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    material_program(&s);
    ecs_entity_t white = gpu_test_color_texture(0xffffffffu);
    ecs_entity_t base = ecs_new();
    ecs_set(base, QuadMaterial, { .tint = { 255, 0, 0, 255 }, .flag = true, .albedo = white });
    ecs_entity_t variant = ecs_new();
    ecs_is_a(variant, base);
    ecs_entity_t overridden = ecs_new();
    ecs_is_a(overridden, base);
    ecs_set(overridden, QuadMaterial, { .tint = { 0, 255, 0, 255 }, .flag = true,
                                        .albedo = white });
    ecs_relate(gpu_test_quad(-1, -1, 1, 2, 0), DrawMaterial, variant);
    ecs_relate(gpu_test_quad(0, -1, 1, 2, 0), DrawMaterial, overridden);
    gpu_test_frames(3);
    test_uint(GpuReady, gpu_test_state(variant)->status);
    test_uint(GpuReady, gpu_test_state(overridden)->status);
    test_uint(RED, gpu_test_pixel(s.target, 2, 8));
    test_uint(GREEN, gpu_test_pixel(s.target, 13, 8));
    ecs_fini();
}

void material_dead_texture_waits(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    material_program(&s);
    ecs_entity_t texture = gpu_test_color_texture(0xffffffffu);
    ecs_entity_t material = ecs_new();
    ecs_set(material, QuadMaterial, { .tint = { 255, 0, 0, 255 }, .flag = true,
                                      .albedo = texture });
    ecs_relate(gpu_test_quad(-1, -1, 2, 2, 0), DrawMaterial, material);
    gpu_test_frames(3);
    test_uint(GpuReady, gpu_test_state(material)->status);
    ecs_kill(texture);
    gpu_test_frames(2);
    test_uint(GpuWaiting, gpu_test_state(material)->status);
    test_assert(gpu_test_stats()->batches_skipped > 0);
    test_uint(BLACK, gpu_test_pixel(s.target, 8, 8));
    ecs_fini();
}

void material_packing(void) {
    gpu_test_start(0);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    ecs_entity_t program = material_program(&s);
    ecs_entity_t white = gpu_test_color_texture(0xffffffffu);
    ecs_entity_t material = ecs_new();
    ecs_set(material, QuadMaterial, { .tint = { 255, 51, 0, 255 }, .flag = true,
                                      .albedo = white });
    gpu_test_frames(3);
    test_uint(GpuReady, gpu_test_state(material)->status);
    const gpu_params_t *params = gpu_params_for_program(program);
    test_uint(32, params->stride);
    struct {
        float tint[4];
        uint32_t flag;
        float unused;
        uint32_t padding[2];
    } packed;
    memset(&packed, 0xcd, sizeof(packed));
    test_true(gpu_test_read_raw(params->buffer, 0, &packed, sizeof(packed)));
    test_flt(1.0f, packed.tint[0]);
    test_flt(0.2f, packed.tint[1]);
    test_flt(0.0f, packed.tint[2]);
    test_uint(1, packed.flag);
    /* Fields without a CPU counterpart and the padding are zero. */
    test_flt(0.0f, packed.unused);
    test_uint(0, packed.padding[0]);
    ecs_fini();
}

ECS_COMPONENT(BadMaterial, { float tint; ecs_entity_t albedo; });

void material_mismatch_fails(void) {
    gpu_test_start(0);
    ECS_COMPONENT_REGISTER(BadMaterial);
    scene_t s = gpu_test_scene(GpuOrderBatched, false, 0);
    ecs_entity_t shader = ecs_get(s.program, GpuProgram)->shader;
    ecs_entity_t textured = ecs_new();
    ecs_set(textured, GpuProgram, { .shader = shader, .vertex = "quad_vertex",
                                    .fragment = "quad_material", .phase = s.phase });
    test_true(sigpu_program_params(textured, ecs_id(BadMaterial)));
    ecs_entity_t material = ecs_new();
    ecs_set(material, BadMaterial, { .tint = 1.0f, .albedo = gpu_test_color_texture(0) });
    gpu_test_frames(3);
    test_uint(GpuFailed, gpu_test_state(material)->status);
    test_uint(GpuErrorInterface, gpu_test_state(material)->error);
    test_assert(strstr(gpu_test_state(material)->message, "tint") != NULL);
    ecs_fini();
}

