#ifndef SIECS_SIGPU_H
#define SIECS_SIGPU_H

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
ECS_COMPONENT_DECLARE_CPP(
    PointerEvents,
    ECS_CPP_FIELDS(uint32_t mask;),
    ECS_CPP_METHODS(PointerEvents() : mask(0) {
    } explicit PointerEvents(uint32_t value) : mask(value){})
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
ECS_RESOURCE_DECLARE(BloomSettings, {
    bool enabled;
    float threshold;
    float intensity;
});

#ifndef __cplusplus
typedef uint8_t Key;
#endif
enum {
    KeyA,
    KeyD,
    KeyW,
    KeyS,
    KeyQ,
    KeyZ,
    KeyE,
    KeyLeft,
    KeyRight,
    KeyUp,
    KeyDown,
    KeySpace,
    KeyI,
    KeyCount
};
#ifdef __cplusplus
enum class Key : uint8_t {
    A = KeyA,
    D = KeyD,
    W = KeyW,
    S = KeyS,
    Q = KeyQ,
    Z = KeyZ,
    E = KeyE,
    Left = KeyLeft,
    Right = KeyRight,
    Up = KeyUp,
    Down = KeyDown,
    Space = KeySpace,
    I = KeyI,
    Count = KeyCount,
};
#endif

ECS_RESOURCE_DECLARE_CPP(
    Keyboard,
    ECS_CPP_FIELDS(bool keys[KeyCount];),
    ECS_CPP_METHODS(bool down(Key key) const { return keys[static_cast<uint8_t>(key)]; })
);

ECS_RESOURCE_DECLARE(Pointer, {
    float x;
    float y;
    float delta_x;
    float delta_y;
    float wheel_x;
    float wheel_y;
    uint32_t buttons;
    uint32_t pressed;
    uint32_t released;
    uint32_t pointer_id;
    uint8_t pointer_type;
});
enum { SiPointerMouse = 0, SiPointerTouch = 1, SiPointerPen = 2 };

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
} sigpu_pointer_event_t;

/* Native code should use ecs_id(Type); these are for dynamic bindings. */
#ifdef __cplusplus
extern "C" {
#endif

SIECS_PUBLIC_API uint16_t sigpu_component_id(const char *name);
SIECS_PUBLIC_API uint16_t sigpu_resource_id(const char *name);
SIECS_PUBLIC_API sireflect_handle_t sigpu_resource_type(const char *name);
SIECS_PUBLIC_API ecs_event_t sigpu_pointer_event_id(uint32_t kind);
SIECS_PUBLIC_API const uint32_t *sigpu_pointer_event_abi(void);

#ifdef __cplusplus
}

/** Typed C++ tags for the pointer events emitted by the sigpu interaction system. */
struct PointerEnter {
    static constexpr uint32_t mask = SiPointerEnterMask;
    static ecs_event_t event_id() noexcept { return sigpu_pointer_event_id(SiPointerEnter); }
};
struct PointerLeave {
    static constexpr uint32_t mask = SiPointerLeaveMask;
    static ecs_event_t event_id() noexcept { return sigpu_pointer_event_id(SiPointerLeave); }
};
struct PointerMove {
    static constexpr uint32_t mask = SiPointerMoveMask;
    static ecs_event_t event_id() noexcept { return sigpu_pointer_event_id(SiPointerMove); }
};
struct PointerDown {
    static constexpr uint32_t mask = SiPointerDownMask;
    static ecs_event_t event_id() noexcept { return sigpu_pointer_event_id(SiPointerDown); }
};
struct PointerUp {
    static constexpr uint32_t mask = SiPointerUpMask;
    static ecs_event_t event_id() noexcept { return sigpu_pointer_event_id(SiPointerUp); }
};
struct PointerCancel {
    static constexpr uint32_t mask = SiPointerCancelMask;
    static ecs_event_t event_id() noexcept { return sigpu_pointer_event_id(SiPointerCancel); }
};
struct Click {
    static constexpr uint32_t mask = SiClickMask;
    static ecs_event_t event_id() noexcept { return sigpu_pointer_event_id(SiClick); }
};
struct Press {
    static constexpr uint32_t mask = SiPressMask;
    static ecs_event_t event_id() noexcept { return sigpu_pointer_event_id(SiPress); }
};
struct PointerWheel {
    static constexpr uint32_t mask = SiWheelMask;
    static ecs_event_t event_id() noexcept { return sigpu_pointer_event_id(SiPointerWheel); }
};
#endif

#endif
