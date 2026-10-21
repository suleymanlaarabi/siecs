#include "window_internal.h"
#include <string.h>

ECS_MODULE_DEFINE(siwindow);
ECS_COMPONENT_DEFINE(Window);
ECS_COMPONENT_DEFINE(WindowState);
ECS_RESOURCE_DEFINE(KeyboardState);
ECS_RESOURCE_DEFINE(PointerState);

#define SAME_KEY(key, scancode) ((int)Key##key == (int)SDL_SCANCODE_##scancode)
_Static_assert((int)KeyCount == (int)SDL_SCANCODE_COUNT, "Key covers every SDL scancode");
_Static_assert(SAME_KEY(A, A) && SAME_KEY(Z, Z), "letter keys");
_Static_assert(SAME_KEY(Digit1, 1) && SAME_KEY(Digit0, 0), "digit keys");
_Static_assert(SAME_KEY(Escape, ESCAPE) && SAME_KEY(Space, SPACE), "control keys");
_Static_assert(SAME_KEY(F12, F12) && SAME_KEY(Delete, DELETE), "function keys");
_Static_assert(SAME_KEY(Up, UP) && SAME_KEY(RightGui, RGUI), "navigation keys");
#undef SAME_KEY
_Static_assert(
    sizeof(((KeyboardState *)0)->held) * 8 == KeyCount &&
        sizeof(((KeyboardState *)0)->went_down) == sizeof(((KeyboardState *)0)->held) &&
        sizeof(((KeyboardState *)0)->went_up) == sizeof(((KeyboardState *)0)->held),
    "keyboard bitsets hold KeyCount bits"
);
_Static_assert(
    SDL_arraysize(((PointerState *)0)->edges) == SIWINDOW_POINTER_EDGE_CAPACITY,
    "PointerState.edges holds SIWINDOW_POINTER_EDGE_CAPACITY edges"
);

siwindow_props_t siwindow_config;
static siwindow_slot_t windows[SIWINDOW_CAPACITY];
static uint32_t window_count;
static ecs_event_t events[SiWindowEventCount] = { UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX };
static siwindow_destroy_fn destroy_callbacks[4];
static uint32_t destroy_callback_count;

static const int32_t default_width = 1280;
static const int32_t default_height = 720;

static siwindow_slot_t *slot_by_entity(ecs_entity_t entity) {
    for (uint32_t i = 0; i < window_count; i++)
        if (windows[i].entity == entity)
            return &windows[i];
    return NULL;
}

siwindow_slot_t *siwindow_slot_by_id(SDL_WindowID id) {
    for (uint32_t i = 0; i < window_count; i++)
        if (windows[i].id == id)
            return &windows[i];
    return NULL;
}

WindowState siwindow_read_state(const siwindow_slot_t *slot) {
    WindowState state = { .scale = SDL_GetWindowDisplayScale(slot->native) };
    SDL_GetWindowSize(slot->native, &state.width, &state.height);
    SDL_GetWindowSizeInPixels(slot->native, &state.pixel_width, &state.pixel_height);
    const SDL_WindowFlags flags = SDL_GetWindowFlags(slot->native);
    state.focused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
    state.minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
    return state;
}

void siwindow_refresh_state(const siwindow_slot_t *slot) {
    WindowState state = siwindow_read_state(slot);
    ecs_set_cid(slot->entity, ecs_id(WindowState), &state);
}

void siwindow_emit(const siwindow_slot_t *slot, uint32_t kind, const char *text) {
    const WindowState *state = ecs_get(slot->entity, WindowState);
    siwindow_event_t payload = { .window = slot->entity };
    if (state) {
        payload.width = state->width;
        payload.height = state->height;
        payload.focused = state->focused;
    }
    if (text)
        SDL_strlcpy(payload.text, text, sizeof(payload.text));
    ecs_observer_trigger(slot->entity, events[kind], &payload);
}

void siwindow_request_close(const siwindow_slot_t *slot) {
    siwindow_emit(slot, SiWindowCloseRequested, NULL);
    if (!siwindow_config.keep_open_on_close)
        ecs_quit();
}

static void apply_text_input(siwindow_slot_t *slot, bool enabled) {
    if (enabled == slot->text_input)
        return;
    slot->text_input = enabled;
    if (enabled)
        SDL_StartTextInput(slot->native);
    else
        SDL_StopTextInput(slot->native);
}

static bool slot_available(const char *name, ecs_entity_t entity) {
    const ecs_entity_t owner = siwindow_find(name);
    if (name[0] && (!owner || owner == entity))
        return true;
    SDL_LogError(
        SDL_LOG_CATEGORY_VIDEO,
        "siwindow: window slot \"%s\" is empty or already used",
        name
    );
    return false;
}

static void create_window(ecs_entity_t entity, const Window *desc) {
    if (!slot_available(desc->slot, entity))
        return;
    if (window_count == SIWINDOW_CAPACITY) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "siwindow: too many windows");
        return;
    }
    const int32_t width = desc->width > 0 ? desc->width : default_width;
    const int32_t height = desc->height > 0 ? desc->height : default_height;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (desc->mode == SiFullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window *native = SDL_CreateWindow(desc->title, width, height, flags);
    if (!native) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "siwindow: %s", SDL_GetError());
        return;
    }
    siwindow_slot_t *slot = &windows[window_count++];
    *slot = (siwindow_slot_t){
        .entity = entity,
        .native = native,
        .id = SDL_GetWindowID(native),
        .width = desc->width,
        .height = desc->height,
        .mode = desc->mode,
    };
    SDL_strlcpy(slot->slot, desc->slot, sizeof(slot->slot));
    apply_text_input(slot, desc->text_input);
    siwindow_refresh_state(slot);
    KeyboardState *keyboard = ecs_get_resource(KeyboardState);
    PointerState *pointer = ecs_get_resource(PointerState);
    if (!keyboard->window)
        keyboard->window = entity;
    if (!pointer->window)
        pointer->window = entity;
}

/* Apply only the fields the application changed: the user may have resized the
 * window since, and Window keeps the last requested size, not the current one.
 * Only size and mode changes wait for the window system. A slot already used
 * by another window keeps the old one. */
static void update_window(siwindow_slot_t *slot, const Window *desc) {
    if (SDL_strcmp(slot->slot, desc->slot) != 0 && slot_available(desc->slot, slot->entity))
        SDL_strlcpy(slot->slot, desc->slot, sizeof(slot->slot));
    if (SDL_strcmp(SDL_GetWindowTitle(slot->native), desc->title) != 0)
        SDL_SetWindowTitle(slot->native, desc->title);
    apply_text_input(slot, desc->text_input);
    bool geometry = false;
    if (desc->width != slot->width || desc->height != slot->height) {
        slot->width = desc->width;
        slot->height = desc->height;
        if (desc->width > 0 && desc->height > 0) {
            SDL_SetWindowSize(slot->native, desc->width, desc->height);
            geometry = true;
        }
    }
    if (desc->mode != slot->mode) {
        slot->mode = desc->mode;
        SDL_SetWindowFullscreen(slot->native, desc->mode == SiFullscreen);
        geometry = true;
    }
    if (!geometry)
        return;
    SDL_SyncWindow(slot->native);
    siwindow_refresh_state(slot);
}

/* The filter matches entities that have Window: other components set on the
 * window entity, WindowState included, reach these observers too. */
static void window_set(ecs_observer_event_t *event) {
    if (event->component != ecs_id(Window))
        return;
    const Window *desc = event->trigger_data;
    siwindow_slot_t *slot = slot_by_entity(event->entity);
    if (slot)
        update_window(slot, desc);
    else
        create_window(event->entity, desc);
}

static void destroy_native(const siwindow_slot_t *slot) {
    for (uint32_t i = 0; i < destroy_callback_count; i++)
        destroy_callbacks[i](slot->entity, slot->native);
    SDL_DestroyWindow(slot->native);
}

static void forget_input_window(ecs_entity_t entity) {
    KeyboardState *keyboard = ecs_get_resource(KeyboardState);
    PointerState *pointer = ecs_get_resource(PointerState);
    if (keyboard->window == entity)
        *keyboard = (KeyboardState){ 0 };
    if (pointer->window == entity)
        *pointer = (PointerState){ 0 };
}

static void window_remove(ecs_observer_event_t *event) {
    if (event->component != ecs_id(Window))
        return;
    siwindow_slot_t *slot = slot_by_entity(event->entity);
    if (!slot)
        return;
    destroy_native(slot);
    forget_input_window(slot->entity);
    /* Observers run before the migration completes, so the state is cleared in
     * place rather than removed. */
    if (ecs_has(slot->entity, WindowState))
        ecs_set_cid(slot->entity, ecs_id(WindowState), &(WindowState){ 0 });
    *slot = windows[--window_count];
}

static void fini_windows(void *data) {
    (void)data;
    for (uint32_t i = 0; i < window_count; i++)
        destroy_native(&windows[i]);
    window_count = 0;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

ecs_entity_t siwindow_find(const char *slot) {
    for (uint32_t i = 0; slot && i < window_count; i++)
        if (SDL_strcmp(windows[i].slot, slot) == 0)
            return windows[i].entity;
    return 0;
}

void *siwindow_native(ecs_entity_t window) {
    const siwindow_slot_t *slot = slot_by_entity(window);
    return slot ? slot->native : NULL;
}

void siwindow_on_destroy(siwindow_destroy_fn callback) {
    if (destroy_callback_count == SDL_arraysize(destroy_callbacks)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "siwindow: too many destroy callbacks");
        return;
    }
    destroy_callbacks[destroy_callback_count++] = callback;
}

ecs_event_t siwindow_event_id(uint32_t kind) {
    return kind < SiWindowEventCount ? events[kind] : 0;
}

void siwindow_import(const siwindow_props_t *props) {
    siwindow_config = props ? *props : (siwindow_props_t){ 0 };
    window_count = 0;
    destroy_callback_count = 0;
    /* Closing the last window is a close request like any other. */
    SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "siwindow: %s", SDL_GetError());
    sireflect_register_struct(&sireflect_desc(SiPointerEdge));
    ECS_COMPONENT_REGISTER(Window, WindowState);
    ECS_RESOURCE_REGISTER(KeyboardState);
    ECS_RESOURCE_REGISTER(PointerState);
    ecs_set_resource(KeyboardState, { 0 });
    ecs_set_resource(PointerState, { 0 });
    for (uint32_t i = 0; i < SiWindowEventCount; i++)
        ecs_event_register(&events[i]);
    ecs_observer(
        { .on = EcsOnSet, .query.components = { ecs_filter(Window) }, .callback = window_set }
    );
    ecs_observer(
        { .on = EcsOnRemove, .query.components = { ecs_filter(Window) }, .callback = window_remove }
    );
    siwindow_pump_register();
    ecs_at_fini({ .callback = fini_windows });
}
