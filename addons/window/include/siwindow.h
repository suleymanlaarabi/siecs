#ifndef SIECS_SIWINDOW_H
#define SIECS_SIWINDOW_H
#include <siecs.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Windows and input. A window is an entity with a Window component; siwindow
 * creates the native window when the component is set and keeps WindowState
 * up to date. Keyboard and pointer input of the focused window are resources,
 * refreshed once per frame in EcsOnLoad on the main thread.
 *
 * By default a close request or Escape stops the world with ecs_quit; the
 * props turn either off. The close request event is emitted in every case.
 * An application quit (SIGINT, SIGTERM, Cmd+Q) always stops the world.
 */
ECS_MODULE_DECLARE(siwindow, {
    bool keep_open_on_close;
    bool keep_open_on_escape;
});

enum { SiWindowed = 0, SiFullscreen = 1 };

/* Written by the application. Width and height are logical pixels. */
ECS_COMPONENT_DECLARE(Window, {
    char slot[32];
    char title[128];
    int32_t width;
    int32_t height;
    uint8_t mode;
    bool text_input;
});

/* Written by siwindow: actual size, pixel density and focus. Zeroed once the
 * Window component is removed. */
ECS_COMPONENT_DECLARE(WindowState, {
    int32_t width;
    int32_t height;
    int32_t pixel_width;
    int32_t pixel_height;
    float scale;
    bool focused;
    bool minimized;
});

/* Key values are USB HID usages, the same numbering as SDL scancodes. */
#define SIWINDOW_KEYS(X)                                                                           \
    X(A, 4)                                                                                        \
    X(B, 5)                                                                                        \
    X(C, 6)                                                                                        \
    X(D, 7)                                                                                        \
    X(E, 8)                                                                                        \
    X(F, 9)                                                                                        \
    X(G, 10)                                                                                       \
    X(H, 11)                                                                                       \
    X(I, 12)                                                                                       \
    X(J, 13)                                                                                       \
    X(K, 14)                                                                                       \
    X(L, 15)                                                                                       \
    X(M, 16)                                                                                       \
    X(N, 17)                                                                                       \
    X(O, 18)                                                                                       \
    X(P, 19)                                                                                       \
    X(Q, 20)                                                                                       \
    X(R, 21)                                                                                       \
    X(S, 22)                                                                                       \
    X(T, 23)                                                                                       \
    X(U, 24)                                                                                       \
    X(V, 25)                                                                                       \
    X(W, 26)                                                                                       \
    X(X, 27)                                                                                       \
    X(Y, 28)                                                                                       \
    X(Z, 29)                                                                                       \
    X(Digit1, 30)                                                                                  \
    X(Digit2, 31)                                                                                  \
    X(Digit3, 32)                                                                                  \
    X(Digit4, 33)                                                                                  \
    X(Digit5, 34)                                                                                  \
    X(Digit6, 35)                                                                                  \
    X(Digit7, 36)                                                                                  \
    X(Digit8, 37)                                                                                  \
    X(Digit9, 38)                                                                                  \
    X(Digit0, 39)                                                                                  \
    X(Return, 40)                                                                                  \
    X(Escape, 41)                                                                                  \
    X(Backspace, 42)                                                                               \
    X(Tab, 43)                                                                                     \
    X(Space, 44)                                                                                   \
    X(F1, 58)                                                                                      \
    X(F2, 59)                                                                                      \
    X(F3, 60)                                                                                      \
    X(F4, 61)                                                                                      \
    X(F5, 62)                                                                                      \
    X(F6, 63)                                                                                      \
    X(F7, 64)                                                                                      \
    X(F8, 65)                                                                                      \
    X(F9, 66)                                                                                      \
    X(F10, 67)                                                                                     \
    X(F11, 68)                                                                                     \
    X(F12, 69)                                                                                     \
    X(Delete, 76)                                                                                  \
    X(Right, 79)                                                                                   \
    X(Left, 80)                                                                                    \
    X(Down, 81)                                                                                    \
    X(Up, 82)                                                                                      \
    X(LeftCtrl, 224)                                                                               \
    X(LeftShift, 225)                                                                              \
    X(LeftAlt, 226)                                                                                \
    X(LeftGui, 227)                                                                                \
    X(RightCtrl, 228)                                                                              \
    X(RightShift, 229)                                                                             \
    X(RightAlt, 230)                                                                               \
    X(RightGui, 231)

#define SIWINDOW_KEY_ENUM(name, value) Key##name = value,
enum { SIWINDOW_KEYS(SIWINDOW_KEY_ENUM) KeyCount = 512 };
#undef SIWINDOW_KEY_ENUM

#ifdef __cplusplus
#define SIWINDOW_KEY_CLASS(name, value) name = value,
enum class Key : uint16_t { SIWINDOW_KEYS(SIWINDOW_KEY_CLASS) Count = KeyCount };
#undef SIWINDOW_KEY_CLASS
#define SIWINDOW_KEY_INDEX(key) static_cast<uint16_t>(key)
#else
typedef uint16_t Key;
#endif

/*
 * Keyboard of the focused window. held is the current state; pressed and
 * released only hold the keys that changed during the last frame.
 */
ECS_RESOURCE_DECLARE_CPP(
    KeyboardState,
    ECS_CPP_FIELDS(ecs_entity_t window; uint64_t held[8]; uint64_t went_down[8];
                   uint64_t went_up[8]; uint16_t modifiers;),
    ECS_CPP_METHODS(
        static bool bit(const uint64_t *bits, Key key) {
            const uint16_t i = SIWINDOW_KEY_INDEX(key);
            return i < KeyCount && ((bits[i >> 6] >> (i & 63)) & 1u);
        } bool down(Key key) const { return bit(held, key); } bool pressed(Key key)
            const { return bit(went_down, key); } bool released(Key key) const {
            return bit(went_up, key);
        }
    )
);

#ifndef __cplusplus
static inline bool siwindow_key_bit(const uint64_t *bits, Key key) {
    return key < KeyCount && ((bits[key >> 6] >> (key & 63)) & 1u);
}
/* True while the key is held. */
static inline bool siwindow_key_down(const KeyboardState *keyboard, Key key) {
    return siwindow_key_bit(keyboard->held, key);
}
/* True during the frame the key went down. */
static inline bool siwindow_key_pressed(const KeyboardState *keyboard, Key key) {
    return siwindow_key_bit(keyboard->went_down, key);
}
/* True during the frame the key went up. */
static inline bool siwindow_key_released(const KeyboardState *keyboard, Key key) {
    return siwindow_key_bit(keyboard->went_up, key);
}
#endif

enum { SiPointerMouse = 0, SiPointerTouch = 1, SiPointerPen = 2 };
enum { SiPointerEdgeDown, SiPointerEdgeUp, SiPointerEdgeWheel, SiPointerEdgeCancel };

#define SIWINDOW_POINTER_EDGE_CAPACITY 64

/* One button or wheel transition, in arrival order. Coordinates are logical. */
SIREFLECT_STRUCT(SiPointerEdge, {
    uint64_t timestamp_ns;
    uint32_t pointer_id;
    uint32_t buttons;
    float x;
    float y;
    float wheel_x;
    float wheel_y;
    uint8_t kind;
    uint8_t button;
    uint8_t clicks;
    uint8_t pointer_type;
});

/*
 * Pointer of the focused window. Deltas, pressed, released and edges cover the
 * last frame; edges past the capacity are counted in dropped_edges.
 */
ECS_RESOURCE_DECLARE(PointerState, {
    ecs_entity_t window;
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
    bool moved;
    uint32_t edge_count;
    uint32_t dropped_edges;
    SiPointerEdge edges[64];
});

/* Window events, emitted on the window entity. */
enum {
    SiWindowResized,
    SiWindowCloseRequested,
    SiWindowFocusChanged,
    SiWindowText,
    SiWindowEventCount
};

typedef struct {
    ecs_entity_t window;
    int32_t width;
    int32_t height;
    bool focused;
    char text[32];
} siwindow_event_t;

#ifdef __cplusplus
extern "C" {
#endif
/* Event id for one of the SiWindow* event kinds. */
SIECS_PUBLIC_API ecs_event_t siwindow_event_id(uint32_t kind);
/* Window entity whose Window.slot is slot, or 0. */
SIECS_PUBLIC_API ecs_entity_t siwindow_find(const char *slot);
#ifdef __cplusplus
}

/** Typed C++ events; ecs::event<WindowResized> reads the siwindow_event_t payload. */
struct WindowResized : siwindow_event_t {
    static ecs_event_t event_id() noexcept { return siwindow_event_id(SiWindowResized); }
};
struct WindowCloseRequested : siwindow_event_t {
    static ecs_event_t event_id() noexcept { return siwindow_event_id(SiWindowCloseRequested); }
};
struct WindowFocusChanged : siwindow_event_t {
    static ecs_event_t event_id() noexcept { return siwindow_event_id(SiWindowFocusChanged); }
};
struct WindowText : siwindow_event_t {
    static ecs_event_t event_id() noexcept { return siwindow_event_id(SiWindowText); }
};
#endif
#endif
