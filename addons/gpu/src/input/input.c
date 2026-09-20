#include "input.h"

ECS_RESOURCE_DEFINE(Keyboard);
ECS_RESOURCE_DEFINE(Pointer);

static const sireflect_enum_desc_t engine_key_reflection = {
    .name = "EngineKey",
    .values = "{ A = 0, D = 1, W = 2, S = 3, Q = 4, Z = 5, E = 6, Left = 7, Right = 8, Up = 9, Down = 10, Space = 11, I = 12 }",
    .size = sizeof(EngineKey), .align = _Alignof(EngineKey),
};
static const sireflect_struct_desc_t keyboard_reflection = {
    .name = "Keyboard", .fields = "{ bool keys[13]; }", .size = sizeof(Keyboard), .align = _Alignof(Keyboard),
};
static const sireflect_struct_desc_t pointer_reflection = {
    .name = "Pointer", .fields = "{ float x; float y; float delta_x; float delta_y; float wheel_x; float wheel_y; uint32_t buttons; uint32_t pressed; uint32_t released; uint32_t pointer_id; uint8_t pointer_type; }",
    .size = sizeof(Pointer), .align = _Alignof(Pointer),
};

static siinput_pointer_edge_t edges[SIINPUT_POINTER_QUEUE_CAPACITY];
static uint32_t edge_count;
static uint32_t dropped_events;
static bool had_motion;
static uint16_t modifiers;
static ecs_system_id_t begin_input_system;
static sireflect_handle_t keyboard_type;
static sireflect_handle_t pointer_type;

static void push_edge(siinput_pointer_edge_t edge) {
    if (edge_count == SIINPUT_POINTER_QUEUE_CAPACITY) { dropped_events++; return; }
    edges[edge_count++] = edge;
}

static void update_keyboard(Keyboard *keyboard) {
    static const SDL_Scancode scancodes[] = {
        SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_W, SDL_SCANCODE_S,
        SDL_SCANCODE_Q, SDL_SCANCODE_Z, SDL_SCANCODE_E, SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_SPACE, SDL_SCANCODE_I,
    };
    const bool *state = SDL_GetKeyboardState(NULL);
    for (size_t i = 0; i < KeyCount; i++) keyboard->keys[i] = state[scancodes[i]];
}

static void begin_input(ecs_iter_t *it) {
    Pointer *pointer = ecs_get_resource(Pointer);
    Keyboard *keyboard = ecs_get_resource(Keyboard);
    pointer->delta_x = pointer->delta_y = pointer->wheel_x = pointer->wheel_y = 0.0f;
    pointer->pressed = pointer->released = 0;
    edge_count = 0;
    had_motion = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            ecs_quit(); break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_ESCAPE) ecs_quit();
            break;
        case SDL_EVENT_MOUSE_MOTION:
            pointer->pointer_id = (uint32_t)event.motion.which;
            pointer->pointer_type = event.motion.which == SDL_TOUCH_MOUSEID ? SiPointerTouch :
                                    event.motion.which == SDL_PEN_MOUSEID ? SiPointerPen : SiPointerMouse;
            pointer->x = event.motion.x; pointer->y = event.motion.y;
            pointer->delta_x += event.motion.xrel; pointer->delta_y += event.motion.yrel;
            pointer->buttons = event.motion.state; had_motion = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            const uint32_t bit = SDL_BUTTON_MASK(event.button.button);
            pointer->pointer_id = (uint32_t)event.button.which;
            pointer->pointer_type = event.button.which == SDL_TOUCH_MOUSEID ? SiPointerTouch : SiPointerMouse;
            pointer->x = event.button.x; pointer->y = event.button.y;
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { pointer->buttons |= bit; pointer->pressed |= bit; }
            else { pointer->buttons &= ~bit; pointer->released |= bit; }
            push_edge((siinput_pointer_edge_t){ event.button.timestamp, pointer->pointer_id, pointer->buttons,
                pointer->x, pointer->y, 0, 0,
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? SiInputEdgeDown : SiInputEdgeUp,
                event.button.button, event.button.clicks, pointer->pointer_type });
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            float x = event.wheel.x, y = event.wheel.y;
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) { x = -x; y = -y; }
            pointer->pointer_id = (uint32_t)event.wheel.which;
            pointer->x = event.wheel.mouse_x; pointer->y = event.wheel.mouse_y;
            pointer->wheel_x += x; pointer->wheel_y += y;
            push_edge((siinput_pointer_edge_t){ event.wheel.timestamp, pointer->pointer_id, pointer->buttons,
                pointer->x, pointer->y, x, y, SiInputEdgeWheel, 0, 0, SiPointerMouse });
            break;
        }
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_DESTROYED:
            if (pointer->buttons) push_edge((siinput_pointer_edge_t){ SDL_GetTicksNS(), pointer->pointer_id,
                pointer->buttons, pointer->x, pointer->y, 0, 0, SiInputEdgeCancel, 0, 0, pointer->pointer_type });
            pointer->buttons = 0;
            break;
        default: break;
        }
    }
    modifiers = (uint16_t)SDL_GetModState();
    update_keyboard(keyboard);
}

void sigpu_input_init(void) {
    sireflect_register_enum(&engine_key_reflection);
    ecs_resource_register(&ecs_id(Keyboard), &ecs_id(Keyboard_desc));
    ecs_resource_register(&ecs_id(Pointer), &ecs_id(Pointer_desc));
    keyboard_type = sireflect_register_struct(&keyboard_reflection);
    pointer_type = sireflect_register_struct(&pointer_reflection);
    ecs_set_resource(Keyboard, { 0 });
    ecs_set_resource(Pointer, { 0 });
    begin_input_system = ecs_system({ .name = "BeginInput", .phase = EcsPreUpdate,
        .query.resources = { { .id = ecs_id(Pointer), .access = EcsInOut }, { .id = ecs_id(Keyboard), .access = EcsInOut } },
        .callback = begin_input, .main_thread_only = true, .no_defer = true });
}

ecs_system_id_t sigpu_input_system(void) { return begin_input_system; }
uint16_t sigpu_input_resource_id(const char *name) {
    if (SDL_strcmp(name, "Keyboard") == 0) return ecs_id(Keyboard);
    if (SDL_strcmp(name, "Pointer") == 0) return ecs_id(Pointer);
    return 0;
}
sireflect_handle_t sigpu_input_resource_type(const char *name) {
    if (SDL_strcmp(name, "Keyboard") == 0) return keyboard_type;
    if (SDL_strcmp(name, "Pointer") == 0) return pointer_type;
    return 0;
}
bool sigpu_input_had_motion(void) { return had_motion; }
uint16_t sigpu_input_modifiers(void) { return modifiers; }
const siinput_pointer_edge_t *sigpu_input_pointer_edges(uint32_t *count) {
    *count = edge_count;
    return edges;
}
