#ifndef SIECS_SIRENDER3D_H
#define SIECS_SIRENDER3D_H
#include <siecs.h>
#include <siecs_spatial.h>
#include <sigpu.h>
#include <siwindow.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * sirender3d: 3D rendering with sigpu draw features (RFC section 12).
 *
 * A camera is an entity with Camera3d, a transform (sispatial) and a
 * RendersTo relation to a GpuSurface. Fog, sky, ambient light, shadows,
 * bloom and MSAA are components on the camera; the renderer's defaults
 * apply when they are absent. The sun is the first entity with
 * DirectionalLight. Each surface gets the standard graph: shadow cascades,
 * forward, bloom, composite into its color target; siui and other
 * contributors draw after it, then sigpu presents.
 *
 * Primitives (Cuboid, Sphere, Cylinder) draw with Color and Bloom; meshes
 * are entities with a transform, a DrawGeometry relation to a GpuGeometry
 * (attributes position float3, normal byte4_norm, uv float2) and a
 * DrawMaterial relation to a StandardMaterial entity.
 */
ECS_MODULE_DECLARE(sirender3d, { uint8_t _unused; });

/* The camera renders to the target surface. */
ECS_RELATION_DECLARE(RendersTo);

/* Graphics phases of the 3D features. */
SIECS_PUBLIC_API extern ecs_entity_t Render3dOpaque;
SIECS_PUBLIC_API extern ecs_entity_t Render3dShadow;

ECS_COMPONENT_DECLARE_CPP(
    Color,
    ECS_CPP_FIELDS(uint8_t r; uint8_t g; uint8_t b; uint8_t a;),
    ECS_CPP_METHODS(
        constexpr Color() : r(0),
        g(0),
        b(0),
        a(255) {} constexpr Color(
            uint8_t r_value,
            uint8_t g_value,
            uint8_t b_value,
            uint8_t a_value = 255
        ) : r(r_value),
        g(g_value),
        b(b_value),
        a(a_value) {} static constexpr Color yellow() {
            return { 255, 255, 0, 255 };
        } static constexpr Color green() {
            return { 0, 255, 0, 255 };
        } static constexpr Color red() {
            return { 255, 0, 0, 255 };
        } static constexpr Color blue() {
            return { 0, 0, 255, 255 };
        } static constexpr Color lblue() {
            return { 100, 100, 255, 255 };
        } static constexpr Color brown() {
            return { 139, 69, 19, 255 };
        } static constexpr Color gray() { return { 128, 128, 128, 255 }; }
    )
);

ECS_COMPONENT_DECLARE_CPP(
    Cuboid,
    ECS_CPP_FIELDS(float width; float height; float depth;),
    ECS_CPP_METHODS(
        constexpr Cuboid() : width(1.0f),
        height(1.0f),
        depth(1.0f) {} constexpr Cuboid(
            float width_value,
            float height_value,
            float depth_value
        ) : width(width_value),
        height(height_value),
        depth(depth_value) {} static constexpr Cuboid splat(float value) {
            return { value, value, value };
        }
    )
);

/* Centered on GlobalPosition3d. Radius is measured before GlobalScale3d;
 * nonuniform scale produces an ellipsoid. GlobalOrientation3d rotates it. */
ECS_COMPONENT_DECLARE_CPP(
    Sphere,
    ECS_CPP_FIELDS(float radius;),
    ECS_CPP_METHODS(constexpr Sphere() : radius(0.5f) {
    } explicit constexpr Sphere(float value) : radius(value){})
);

/* Centered on GlobalPosition3d. Radius applies to local X/Z and height to
 * local Y before GlobalScale3d; nonuniform X/Z scale makes an elliptic
 * cylinder. */
ECS_COMPONENT_DECLARE_CPP(
    Cylinder,
    ECS_CPP_FIELDS(float radius; float height;),
    ECS_CPP_METHODS(
        constexpr Cylinder() : radius(0.5f),
        height(1.0f) {
        } constexpr Cylinder(float radius_value, float height_value) : radius(radius_value),
        height(height_value){}
    )
);

/* Emissive intensity: the color times intensity glows through bloom. */
ECS_COMPONENT_DECLARE_CPP(
    Bloom,
    ECS_CPP_FIELDS(float intensity;),
    ECS_CPP_METHODS(Bloom() : intensity(0.0f) {} explicit Bloom(float value) : intensity(value){})
);

enum { Render3dPerspective = 0, Render3dOrthographic = 1 };

/*
 * fov is vertical, in degrees (perspective) or the view height in world
 * units (orthographic). viewport is normalized (x, y, width, height); zero
 * covers the target. Cameras on one surface draw in increasing order; the
 * first one clears it, the others only clear their viewport.
 */
ECS_COMPONENT_DECLARE_CPP(
    Camera3d,
    ECS_CPP_FIELDS(float fov; float near_plane; float far_plane; uint8_t projection;
                   float viewport[4]; int32_t order;),
    ECS_CPP_METHODS(
        Camera3d() : fov(60.0f),
        near_plane(0.1f),
        far_plane(1000.0f),
        projection(0),
        viewport{ 0, 0, 0, 0 },
        order(0) {} explicit Camera3d(float value, float near_value = 0.1f,
                                      float far_value = 1000.0f) : fov(value),
        near_plane(near_value),
        far_plane(far_value),
        projection(0),
        viewport{ 0, 0, 0, 0 },
        order(0){}
    )
);

/* Camera settings. */
ECS_COMPONENT_DECLARE(Sky, { Color color; });
ECS_COMPONENT_DECLARE(AmbientLight, {
    Color color;
    float intensity;
});
ECS_COMPONENT_DECLARE(Fog, {
    Color color;
    float start;
    float end;
});
ECS_COMPONENT_DECLARE(Shadows, {
    bool enabled;
    float distance;
});
ECS_COMPONENT_DECLARE(BloomSettings, {
    bool enabled;
    float threshold;
    float intensity;
});
ECS_COMPONENT_DECLARE(Multisampling, { int32_t samples; });

/* The sun: light travels along (x, y, z). */
ECS_COMPONENT_DECLARE(DirectionalLight, {
    float x;
    float y;
    float z;
    Color color;
    float intensity;
});

/* Material of meshes: base_color times the albedo texture (0: white). */
ECS_COMPONENT_DECLARE(StandardMaterial, {
    uint8_t base_color[4];
    float emissive;
    ecs_entity_t albedo;
    ecs_entity_t albedo_sampler;
});

/* Pointer events of 3D picking (RFC section 12.4). */
ECS_COMPONENT_DECLARE_CPP(
    PointerEvents,
    ECS_CPP_FIELDS(uint32_t mask;),
    ECS_CPP_METHODS(PointerEvents() : mask(0) {
    } explicit PointerEvents(uint32_t value) : mask(value){})
);
enum {
    SiPointerEnterMask = 1u << 0,
    SiPointerLeaveMask = 1u << 1,
    SiPointerMoveMask = 1u << 2,
    SiPointerDownMask = 1u << 3,
    SiPointerUpMask = 1u << 4,
    SiPointerCancelMask = 1u << 5,
    SiClickMask = 1u << 6,
    SiPressMask = 1u << 7,
    SiWheelMask = 1u << 8,
};
enum {
    SiPointerEnter,
    SiPointerLeave,
    SiPointerMove,
    SiPointerDown,
    SiPointerUp,
    SiPointerCancel,
    SiClick,
    SiPress,
    SiPointerWheel,
    SiPointerEventCount
};

typedef struct {
    ecs_entity_t target;
    ecs_entity_t related_target;
    uint64_t timestamp_ns;
    uint32_t pointer_id;
    uint32_t buttons;
    uint16_t modifiers;
    uint8_t pointer_type;
    uint8_t button;
    uint8_t clicks;
    float x;
    float y;
    float delta_x;
    float delta_y;
    float wheel_x;
    float wheel_y;
    float ray_origin_x;
    float ray_origin_y;
    float ray_origin_z;
    float ray_direction_x;
    float ray_direction_y;
    float ray_direction_z;
    float point_x;
    float point_y;
    float point_z;
    float normal_x;
    float normal_y;
    float normal_z;
    float distance;
} sirender3d_pointer_event_t;

#ifdef __cplusplus
extern "C" {
#endif
/* Event id of one of the SiPointer* kinds, emitted on the entity hit. */
SIECS_PUBLIC_API ecs_event_t sirender3d_pointer_event_id(uint32_t kind);
SIECS_PUBLIC_API const uint32_t *sirender3d_pointer_event_abi(void);
/* The features of the primitives and meshes. */
SIECS_PUBLIC_API ecs_entity_t sirender3d_feature(uint32_t kind);
/*
 * World ray through a window point (logical coordinates) of the camera
 * that renders to the surface of that window; false without such camera.
 */
SIECS_PUBLIC_API bool sirender3d_pointer_ray(ecs_entity_t window, float x, float y,
                                             float origin[3], float direction[3]);
#ifdef __cplusplus
}

/** Typed C++ tags for the pointer events emitted by sirender3d picking. */
struct PointerEnter {
    static constexpr uint32_t mask = SiPointerEnterMask;
    static ecs_event_t event_id() noexcept { return sirender3d_pointer_event_id(SiPointerEnter); }
};
struct PointerLeave {
    static constexpr uint32_t mask = SiPointerLeaveMask;
    static ecs_event_t event_id() noexcept { return sirender3d_pointer_event_id(SiPointerLeave); }
};
struct PointerMove {
    static constexpr uint32_t mask = SiPointerMoveMask;
    static ecs_event_t event_id() noexcept { return sirender3d_pointer_event_id(SiPointerMove); }
};
struct PointerDown {
    static constexpr uint32_t mask = SiPointerDownMask;
    static ecs_event_t event_id() noexcept { return sirender3d_pointer_event_id(SiPointerDown); }
};
struct PointerUp {
    static constexpr uint32_t mask = SiPointerUpMask;
    static ecs_event_t event_id() noexcept { return sirender3d_pointer_event_id(SiPointerUp); }
};
struct PointerCancel {
    static constexpr uint32_t mask = SiPointerCancelMask;
    static ecs_event_t event_id() noexcept { return sirender3d_pointer_event_id(SiPointerCancel); }
};
struct Click {
    static constexpr uint32_t mask = SiClickMask;
    static ecs_event_t event_id() noexcept { return sirender3d_pointer_event_id(SiClick); }
};
struct Press {
    static constexpr uint32_t mask = SiPressMask;
    static ecs_event_t event_id() noexcept { return sirender3d_pointer_event_id(SiPress); }
};
struct PointerWheel {
    static constexpr uint32_t mask = SiWheelMask;
    static ecs_event_t event_id() noexcept { return sirender3d_pointer_event_id(SiPointerWheel); }
};
#endif

enum { Render3dCuboids = 0, Render3dSpheres = 1, Render3dCylinders = 2, Render3dMeshes = 3 };
#endif
