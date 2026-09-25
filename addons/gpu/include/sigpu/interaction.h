#ifndef SIECS_SIGPU_INTERACTION_H
#define SIECS_SIGPU_INTERACTION_H
#include "sigpu/input.h"
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
} sigpu_pointer_event_t;
#ifdef __cplusplus
extern "C" {
#endif
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
