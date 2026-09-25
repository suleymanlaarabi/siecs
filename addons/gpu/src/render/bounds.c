#include "render/render_internal.h"

static ecs_system_id_t shadow_systems[6];

static void begin_shadow_bounds(ecs_iter_t *it) {
    if (SIGPU_FRAMECONTEXT->swapchain && SIGPU_FRAMECONTEXT->frame_height)
        sigpu_shadow_bounds_begin(
            (float)SIGPU_FRAMECONTEXT->frame_width / SIGPU_FRAMECONTEXT->frame_height
        );
}

static void build_static_shadow_bounds(ecs_iter_t *it) {
    if (SIGPU_FRAMECONTEXT->swapchain && SIGPU_FRAMECONTEXT->frame_height)
        sigpu_static_shadow_bounds_extend();
}

static void build_primitive_shadow_bounds(ecs_iter_t *it, sigpu_primitive_t primitive) {
    if (!SIGPU_FRAMECONTEXT->swapchain || !SIGPU_FRAMECONTEXT->frame_height)
        return;
    field_GlobalPosition3d positions = FIELD(GlobalPosition3d, it, 0);
    field_GlobalScale3d scales = FIELD(GlobalScale3d, it, 1);
    ptrdiff_t stride = ecs_field_is_shared(it, 2) ? 0 : 1;
    for (uint32_t i = 0; i < it->count; i++) {
        GlobalPosition3d p = AT(positions, i);
        primitive_size_t size = scaled_size(
            primitive_size(
                primitive,
                primitive == SIGPU_PRIMITIVE_CUBE
                    ? (const void *)&((const Cuboid *)ecs_field(it, 2))[i * stride]
                : primitive == SIGPU_PRIMITIVE_CYLINDER
                    ? (const void *)&((const Cylinder *)ecs_field(it, 2))[i * stride]
                    : (const void *)&((const Sphere *)ecs_field(it, 2))[i * stride]
            ),
            AT(scales, i)
        );
        sigpu_shadow_bounds_extend(
            (sigpu_vec3_t){ p.x, p.y, p.z },
            primitive_radius(primitive, size),
            false
        );
    }
}

static void build_cuboid_shadow_bounds(ecs_iter_t *it) {
    build_primitive_shadow_bounds(it, SIGPU_PRIMITIVE_CUBE);
}
static void build_cylinder_shadow_bounds(ecs_iter_t *it) {
    build_primitive_shadow_bounds(it, SIGPU_PRIMITIVE_CYLINDER);
}
static void build_sphere_shadow_bounds(ecs_iter_t *it) {
    build_primitive_shadow_bounds(it, SIGPU_PRIMITIVE_SPHERE);
}

static void end_shadow_bounds(ecs_iter_t *it) {
    if (SIGPU_FRAMECONTEXT->swapchain && SIGPU_FRAMECONTEXT->frame_height)
        sigpu_shadow_bounds_end();
}

void sigpu_bounds_register(ecs_system_id_t static_cache_system) {
    ecs_system_desc_t begin = {
        .name = "BuildShadowView",
        .callback = begin_shadow_bounds,
        .phase = EcsPreRender,
        .after = { static_cache_system },
        .main_thread_only = true,
    };
    const ecs_system_id_t begin_system = shadow_systems[0] = ecs_system_init(&begin);
    ecs_system_desc_t static_build = {
        .name = "BuildStaticShadowBounds",
        .callback = build_static_shadow_bounds,
        .phase = EcsPreRender,
        .after = { begin_system },
        .main_thread_only = true,
    };
    const ecs_system_id_t static_build_system = shadow_systems[1] = ecs_system_init(&static_build);
    uint16_t shapes[SIGPU_PRIMITIVE_COUNT] = { ecs_id(Cuboid), ecs_id(Cylinder), ecs_id(Sphere) };
    void (*callbacks[SIGPU_PRIMITIVE_COUNT])(ecs_iter_t *) = { build_cuboid_shadow_bounds,
                                                               build_cylinder_shadow_bounds,
                                                               build_sphere_shadow_bounds };
    const char *names[SIGPU_PRIMITIVE_COUNT] = { "BuildCuboidShadowBounds",
                                                 "BuildCylinderShadowBounds",
                                                 "BuildSphereShadowBounds" };
    ecs_system_id_t build_system = static_build_system;
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        ecs_system_desc_t build={.name=names[p],.query={.components={
            {.id=ecs_id(GlobalPosition3d),.access=EcsIn},
            {.id=ecs_id(GlobalScale3d),.access=EcsIn},
            {.id=shapes[p],.access=EcsIn},
            {.id=ecs_id(Static3dReady),.access=EcsNot},
        }},.callback=callbacks[p],.phase=EcsPreRender,.after={build_system},.main_thread_only=true};
        build_system = shadow_systems[p + 2] = ecs_system_init(&build);
    }
    ecs_system_desc_t end = {
        .name = "EndShadowBounds",
        .callback = end_shadow_bounds,
        .phase = EcsPreRender,
        .after = { build_system },
        .main_thread_only = true,
    };
    shadow_systems[5] = ecs_system_init(&end);
}

void sigpu_bounds_set_enabled(bool enabled) {
    for (size_t i = 0; i < sizeof(shadow_systems) / sizeof(*shadow_systems); i++) {
        if (!shadow_systems[i])
            continue;
        if (enabled)
            ecs_system_enable(shadow_systems[i]);
        else
            ecs_system_disable(shadow_systems[i]);
    }
}

void sigpu_bounds_reset(void) { SDL_memset(shadow_systems, 0, sizeof(shadow_systems)); }
