#ifndef SIECS_SIGPU_INPUT_H
#define SIECS_SIGPU_INPUT_H
#include <siecs.h>
#include <stdbool.h>
#include <stdint.h>
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
#endif
