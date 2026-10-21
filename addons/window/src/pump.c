#include "window_internal.h"

static void set_bit(uint64_t *bits, SDL_Scancode code) { bits[code >> 6] |= 1ull << (code & 63); }
static void clear_bit(uint64_t *bits, SDL_Scancode code) {
    bits[code >> 6] &= ~(1ull << (code & 63));
}

static uint8_t pointer_type(SDL_MouseID which) {
    return which == SDL_TOUCH_MOUSEID ? SiPointerTouch
           : which == SDL_PEN_MOUSEID ? SiPointerPen
                                      : SiPointerMouse;
}

static void push_edge(PointerState *pointer, SiPointerEdge edge) {
    if (pointer->edge_count == SIWINDOW_POINTER_EDGE_CAPACITY) {
        pointer->dropped_edges++;
        return;
    }
    pointer->edges[pointer->edge_count++] = edge;
}

static SiPointerEdge pointer_edge(const PointerState *pointer, uint64_t timestamp, uint8_t kind) {
    return (SiPointerEdge){
        .timestamp_ns = timestamp,
        .pointer_id = pointer->pointer_id,
        .buttons = pointer->buttons,
        .x = pointer->x,
        .y = pointer->y,
        .kind = kind,
        .pointer_type = pointer->pointer_type,
    };
}

static void begin_frame(KeyboardState *keyboard, PointerState *pointer) {
    SDL_memset(keyboard->went_down, 0, sizeof(keyboard->went_down));
    SDL_memset(keyboard->went_up, 0, sizeof(keyboard->went_up));
    pointer->delta_x = pointer->delta_y = pointer->wheel_x = pointer->wheel_y = 0.0f;
    pointer->pressed = pointer->released = 0;
    pointer->edge_count = pointer->dropped_edges = 0;
    pointer->moved = false;
}

/* Losing focus releases everything: the matching key and button ups go to the
 * window that gains focus, so they would never reach this state. */
static void focus_lost(KeyboardState *keyboard, PointerState *pointer) {
    for (uint32_t i = 0; i < SDL_arraysize(keyboard->held); i++) {
        keyboard->went_up[i] |= keyboard->held[i];
        keyboard->held[i] = 0;
    }
    if (pointer->buttons) {
        push_edge(pointer, pointer_edge(pointer, SDL_GetTicksNS(), SiPointerEdgeCancel));
        pointer->released |= pointer->buttons;
        pointer->buttons = 0;
    }
}

/* Focus comes from the event: the window flags may not be updated yet. */
static void focus_changed(const siwindow_slot_t *slot, bool focused) {
    WindowState state = siwindow_read_state(slot);
    state.focused = focused;
    ecs_set_cid(slot->entity, ecs_id(WindowState), &state);
    siwindow_emit(slot, SiWindowFocusChanged, NULL);
}

/* Pointer events of windows other than the focused one are dropped: their
 * coordinates are local to a window PointerState does not describe. */
static bool from_pointer_window(const PointerState *pointer, SDL_WindowID id) {
    const siwindow_slot_t *slot = siwindow_slot_by_id(id);
    return slot && slot->entity == pointer->window;
}

static void window_event(const SDL_Event *event, KeyboardState *keyboard, PointerState *pointer) {
    siwindow_slot_t *slot = siwindow_slot_by_id(event->window.windowID);
    if (!slot)
        return;
    switch (event->type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        siwindow_request_close(slot);
        return;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        siwindow_refresh_state(slot);
        siwindow_emit(slot, SiWindowResized, NULL);
        return;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        keyboard->window = pointer->window = slot->entity;
        focus_changed(slot, true);
        return;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (keyboard->window == slot->entity)
            focus_lost(keyboard, pointer);
        focus_changed(slot, false);
        return;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
        siwindow_refresh_state(slot);
        return;
    default:
        return;
    }
}

static void key_event(const SDL_KeyboardEvent *key, KeyboardState *keyboard) {
    if (key->scancode >= SDL_SCANCODE_COUNT)
        return;
    if (!key->down) {
        clear_bit(keyboard->held, key->scancode);
        set_bit(keyboard->went_up, key->scancode);
        return;
    }
    if (key->repeat)
        return;
    set_bit(keyboard->held, key->scancode);
    set_bit(keyboard->went_down, key->scancode);
    if (key->scancode == SDL_SCANCODE_ESCAPE && !siwindow_config.keep_open_on_escape) {
        siwindow_slot_t *slot = siwindow_slot_by_id(key->windowID);
        if (slot)
            siwindow_request_close(slot);
    }
}

static void pump_window_events(ecs_iter_t *it) {
    (void)it;
    KeyboardState *keyboard = ecs_get_resource(KeyboardState);
    PointerState *pointer = ecs_get_resource(PointerState);
    begin_frame(keyboard, pointer);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
            window_event(&event, keyboard, pointer);
            continue;
        }
        switch (event.type) {
        case SDL_EVENT_QUIT:
            ecs_quit();
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            key_event(&event.key, keyboard);
            break;
        case SDL_EVENT_TEXT_INPUT: {
            siwindow_slot_t *slot = siwindow_slot_by_id(event.text.windowID);
            if (slot)
                siwindow_emit(slot, SiWindowText, event.text.text);
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            if (!from_pointer_window(pointer, event.motion.windowID))
                break;
            pointer->pointer_id = (uint32_t)event.motion.which;
            pointer->pointer_type = pointer_type(event.motion.which);
            pointer->x = event.motion.x;
            pointer->y = event.motion.y;
            pointer->delta_x += event.motion.xrel;
            pointer->delta_y += event.motion.yrel;
            pointer->buttons = event.motion.state;
            pointer->moved = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            const uint32_t bit = SDL_BUTTON_MASK(event.button.button);
            const bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            if (!from_pointer_window(pointer, event.button.windowID))
                break;
            pointer->pointer_id = (uint32_t)event.button.which;
            pointer->pointer_type = pointer_type(event.button.which);
            pointer->x = event.button.x;
            pointer->y = event.button.y;
            if (down) {
                pointer->buttons |= bit;
                pointer->pressed |= bit;
            } else {
                pointer->buttons &= ~bit;
                pointer->released |= bit;
            }
            SiPointerEdge edge = pointer_edge(
                pointer,
                event.button.timestamp,
                down ? SiPointerEdgeDown : SiPointerEdgeUp
            );
            edge.button = event.button.button;
            edge.clicks = event.button.clicks;
            push_edge(pointer, edge);
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            if (!from_pointer_window(pointer, event.wheel.windowID))
                break;
            float x = event.wheel.x, y = event.wheel.y;
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                x = -x;
                y = -y;
            }
            pointer->pointer_id = (uint32_t)event.wheel.which;
            pointer->x = event.wheel.mouse_x;
            pointer->y = event.wheel.mouse_y;
            pointer->wheel_x += x;
            pointer->wheel_y += y;
            SiPointerEdge edge = pointer_edge(pointer, event.wheel.timestamp, SiPointerEdgeWheel);
            edge.wheel_x = x;
            edge.wheel_y = y;
            edge.pointer_type = SiPointerMouse;
            push_edge(pointer, edge);
            break;
        }
        default:
            break;
        }
    }
    keyboard->modifiers = (uint16_t)SDL_GetModState();
}

void siwindow_pump_register(void) {
    ecs_system(
        { .name = "PumpWindowEvents",
          .phase = EcsOnLoad,
          .query.resources = { { .id = ecs_id(PointerState), .access = EcsInOut },
                               { .id = ecs_id(KeyboardState), .access = EcsInOut } },
          .callback = pump_window_events,
          .main_thread_only = true,
          .no_defer = true }
    );
}
