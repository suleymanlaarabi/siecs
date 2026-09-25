#ifndef SIECS_SIGPU_RENDERING_H
#define SIECS_SIGPU_RENDERING_H
#include "sigpu/bake_config.h"
#include <siecs.h>
#include <siecs_spatial.h>
#include <stdbool.h>
#include <stdint.h>

ECS_MODULE_DECLARE(sigpu, {
    const char *title;
    int width;
    int height;
    int samples;
});

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
 * local Y before GlobalScale3d; nonuniform X/Z scale makes an elliptic cylinder. */
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
ECS_COMPONENT_DECLARE_CPP(
    Bloom,
    ECS_CPP_FIELDS(float intensity;),
    ECS_CPP_METHODS(Bloom() : intensity(0.0f) {} explicit Bloom(float value) : intensity(value){})
);
ECS_COMPONENT_DECLARE_CPP(
    Camera,
    ECS_CPP_FIELDS(float fov;),
    ECS_CPP_METHODS(Camera() : fov(60.0f) {} explicit Camera(float value) : fov(value){})
);
ECS_RESOURCE_DECLARE(WindowConfig, {
    int width;
    int height;
    const char *title;
});
ECS_RESOURCE_DECLARE(Sky, { Color color; });
ECS_RESOURCE_DECLARE(Sun, {
    float x;
    float y;
    float z;
    Color color;
    float intensity;
});
ECS_RESOURCE_DECLARE(AmbientLight, {
    Color color;
    float intensity;
});
ECS_RESOURCE_DECLARE(Fog, {
    Color color;
    float start;
    float end;
});
ECS_RESOURCE_DECLARE(Shadows, {
    bool enabled;
    float distance;
});
ECS_RESOURCE_DECLARE(Multisampling, { int samples; });
ECS_RESOURCE_DECLARE(CameraClip, {
    float near_plane;
    float far_plane;
});
ECS_RESOURCE_DECLARE(BloomSettings, {
    bool enabled;
    float threshold;
    float intensity;
});
#ifdef __cplusplus
extern "C" {
#endif
SIECS_PUBLIC_API bool sigpu_set_fullscreen(bool enabled);
#ifdef __cplusplus
}
#endif
#endif
