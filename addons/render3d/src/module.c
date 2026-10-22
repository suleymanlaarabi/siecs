#include "render3d_internal.h"
#include "shaders/render3d_pack.generated.h"
#include <SDL3/SDL.h>

ECS_MODULE_DEFINE(sirender3d);
ECS_RELATION_DEFINE(RendersTo, { .storage = EcsRelationDense,
                                 .on_delete_target = EcsRemoveRelation });
ECS_COMPONENT_DEFINE(Color, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Cuboid, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Sphere, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Cylinder, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Bloom, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Camera3d);
ECS_COMPONENT_DEFINE(Sky);
ECS_COMPONENT_DEFINE(AmbientLight);
ECS_COMPONENT_DEFINE(Fog);
ECS_COMPONENT_DEFINE(Shadows);
ECS_COMPONENT_DEFINE(BloomSettings);
ECS_COMPONENT_DEFINE(Multisampling);
ECS_COMPONENT_DEFINE(DirectionalLight);
ECS_COMPONENT_DEFINE(StandardMaterial, .inheritance = EcsInheritShared);
ECS_COMPONENT_DEFINE(Render3dPrimitiveInstance);
ECS_COMPONENT_DEFINE(Render3dMeshInstance);

ecs_entity_t Render3dOpaque;
ecs_entity_t Render3dShadow;
r3d_state_t r3d;

_Static_assert(sizeof(Render3dPrimitiveInstance) == sizeof(r3d_primitive_t), "instance layout");
_Static_assert(sizeof(Render3dMeshInstance) == sizeof(r3d_mesh_t), "instance layout");

static ecs_entity_t program(const char *name, const char *vertex, const char *fragment,
                            ecs_entity_t phase, GpuRaster raster) {
    GpuProgram desc = { .shader = r3d.shader, .phase = phase, .raster = raster };
    SDL_strlcpy(desc.vertex, vertex, sizeof(desc.vertex));
    SDL_strlcpy(desc.fragment, fragment, sizeof(desc.fragment));
    const ecs_entity_t entity = ecs_new_named(0, name);
    ecs_set_cid(entity, ecs_id(GpuProgram), &desc);
    return entity;
}

/* Opaque geometry with a depth-only shadow variant, as in sigpu v1. */
static ecs_entity_t lit_program(const char *name, const char *vertex, const char *fragment,
                                const char *shadow) {
    const GpuRaster opaque = { .cull = GpuFaceBack, .depth_compare = GpuCompareLessEqual,
                               .depth_write = true };
    GpuProgram desc = { .shader = r3d.shader, .phase = Render3dOpaque, .raster = opaque };
    SDL_strlcpy(desc.vertex, vertex, sizeof(desc.vertex));
    SDL_strlcpy(desc.fragment, fragment, sizeof(desc.fragment));
    desc.variants[0] = (GpuProgramVariant){
        .phase = Render3dShadow,
        .raster = { .cull = GpuFaceBack, .depth_compare = GpuCompareLessEqual,
                    .depth_write = true, .depth_bias = 1.25f, .slope_bias = 1.75f },
    };
    SDL_strlcpy(desc.variants[0].vertex, shadow, sizeof(desc.variants[0].vertex));
    SDL_strlcpy(desc.variants[0].fragment, "depth_only", sizeof(desc.variants[0].fragment));
    const ecs_entity_t entity = ecs_new_named(0, name);
    ecs_set_cid(entity, ecs_id(GpuProgram), &desc);
    return entity;
}

static void fini_render3d(void *data) {
    (void)data;
    r3d_views_fini();
    r3d = (r3d_state_t){ 0 };
    Render3dOpaque = Render3dShadow = 0;
}

void sirender3d_import(const sirender3d_props_t *props) {
    (void)props;
    ECS_MODULE_IMPORT(sispatial, { 0 });
    ECS_MODULE_IMPORT(siwindow, { 0 });
    ECS_MODULE_IMPORT(sigpu, { 0 });
    ECS_RELATION_REGISTER(RendersTo);
    ECS_COMPONENT_REGISTER(Color, Cuboid, Sphere, Cylinder, Bloom, Camera3d);
    ECS_COMPONENT_REGISTER(Sky, AmbientLight, Fog, Shadows, BloomSettings, Multisampling);
    ECS_COMPONENT_REGISTER(DirectionalLight, StandardMaterial);
    ECS_COMPONENT_REGISTER(Render3dPrimitiveInstance, Render3dMeshInstance);
    r3d = (r3d_state_t){ 0 };
    Render3dOpaque = ecs_new_named(0, "sirender3d::phases::opaque");
    Render3dShadow = ecs_new_named(0, "sirender3d::phases::shadow");

    sigpu_asset_register(R3D_PACK, render3d_gpushader, sizeof(render3d_gpushader));
    r3d.shader = ecs_new_named(0, "sirender3d::shader");
    ecs_set(r3d.shader, GpuShader, { .path = R3D_PACK });
    r3d.primitive_program = lit_program("sirender3d::programs::primitive", "primitive_vertex",
                                        "primitive_fragment", "primitive_shadow");
    r3d.mesh_program =
        lit_program("sirender3d::programs::mesh", "mesh_vertex", "mesh_fragment", "mesh_shadow");
    sigpu_program_params(r3d.mesh_program, ecs_id(StandardMaterial));
    const GpuRaster flat = { 0 };
    r3d.sky_program = program("sirender3d::programs::sky", "fullscreen", "sky_fragment", 0, flat);
    r3d.bloom_down_program = program("sirender3d::programs::bloom_down", "fullscreen",
                                     "bloom_down_fragment", 0, flat);
    r3d.bloom_blur_program = program("sirender3d::programs::bloom_blur", "fullscreen",
                                     "bloom_blur_fragment", 0, flat);
    r3d.composite_program = program("sirender3d::programs::composite", "fullscreen",
                                    "composite_fragment", 0, flat);
    r3d.composite_plain_program = program("sirender3d::programs::composite_plain", "fullscreen",
                                          "composite_plain", 0, flat);
    r3d.linear_sampler = ecs_new_named(0, "sirender3d::samplers::linear");
    ecs_set(r3d.linear_sampler, GpuSampler,
            { .min_filter = GpuFilterLinear, .mag_filter = GpuFilterLinear,
              .address_u = GpuAddressClamp, .address_v = GpuAddressClamp,
              .address_w = GpuAddressClamp });
    r3d.shadow_sampler = ecs_new_named(0, "sirender3d::samplers::shadow");
    ecs_set(r3d.shadow_sampler, GpuSampler,
            { .min_filter = GpuFilterLinear, .mag_filter = GpuFilterLinear,
              .address_u = GpuAddressClamp, .address_v = GpuAddressClamp,
              .address_w = GpuAddressClamp, .compare = GpuCompareLessEqual });
    r3d_primitives_init();
    r3d_views_init();
    r3d_picking_init();
    ecs_at_fini({ .callback = fini_render3d });
}
