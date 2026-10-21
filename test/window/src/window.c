#include <SDL3/SDL.h>
#include <bake_test.h>
#include <siwindow/native.h>

static uint32_t event_count;
static siwindow_event_t last_event;

static void count_event(ecs_observer_event_t *event) {
    event_count++;
    last_event = *(const siwindow_event_t *)event->trigger_data;
}

static void start(siwindow_props_t props) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    event_count = 0;
    last_event = (siwindow_event_t){ 0 };
    ecs_init();
    siwindow_import(&props);
}

static ecs_entity_t main_window(void) {
    ecs_entity_t window = ecs_new();
    ecs_set(window, Window, { .slot = "main", .title = "test", .width = 640, .height = 480 });
    return window;
}

static SDL_WindowID window_id(ecs_entity_t window) {
    return SDL_GetWindowID((SDL_Window *)siwindow_native(window));
}

static void observe(ecs_entity_t window, uint32_t kind) {
    ecs_observer({ .on = siwindow_event_id(kind), .entity = window, .callback = count_event });
}

static void push(SDL_Event event) { test_true(SDL_PushEvent(&event)); }

static void push_key(ecs_entity_t window, SDL_Scancode code, bool down, bool repeat) {
    SDL_Event event = { .type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP };
    event.key.windowID = window_id(window);
    event.key.scancode = code;
    event.key.down = down;
    event.key.repeat = repeat;
    push(event);
}

static void push_window(ecs_entity_t window, SDL_EventType type) {
    SDL_Event event = { .type = type };
    event.window.windowID = window_id(window);
    push(event);
}

static void push_button(ecs_entity_t window, bool down) {
    SDL_Event event = { .type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP };
    event.button.windowID = window_id(window);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = down;
    event.button.x = 10.0f;
    event.button.y = 20.0f;
    push(event);
}

void window_created_on_set(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    test_uint(window, siwindow_find("main"));
    test_assert(siwindow_native(window) != NULL);
    const WindowState *state = ecs_get(window, WindowState);
    test_assert(state != NULL);
    test_int(640, state->width);
    test_int(480, state->height);
    test_uint(window, ecs_get_resource_read(KeyboardState)->window);
    test_uint(window, ecs_get_resource_read(PointerState)->window);
    ecs_fini();
}

void window_update_applies_title_and_size(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    ecs_set(window, Window, { .slot = "main", .title = "renamed", .width = 320, .height = 200 });
    ecs_progress();
    test_str("renamed", SDL_GetWindowTitle(siwindow_native(window)));
    const WindowState *state = ecs_get(window, WindowState);
    test_int(320, state->width);
    test_int(200, state->height);
    ecs_fini();
}

void window_duplicate_slot_rejected(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    ecs_entity_t other = ecs_new();
    ecs_set(other, Window, { .slot = "main", .title = "other" });
    test_uint(window, siwindow_find("main"));
    test_assert(siwindow_native(other) == NULL);
    ecs_fini();
}

void window_remove_destroys_window(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    ecs_remove(window, Window);
    test_uint(0, siwindow_find("main"));
    test_assert(siwindow_native(window) == NULL);
    test_uint(0, ecs_get_resource_read(KeyboardState)->window);
    ecs_fini();
}

void window_remove_clears_state(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    ecs_remove(window, Window);
    const WindowState *state = ecs_get(window, WindowState);
    test_int(0, state->width);
    test_false(state->focused);
    ecs_fini();
}

static ecs_entity_t destroyed_window;
static SDL_WindowID destroyed_id;

static void record_destroy(ecs_entity_t window, void *native) {
    destroyed_window = window;
    destroyed_id = SDL_GetWindowID(native);
}

void window_destroy_callback_sees_live_window(void) {
    start((siwindow_props_t){ 0 });
    destroyed_window = 0;
    destroyed_id = 0;
    siwindow_on_destroy(record_destroy);
    ecs_entity_t window = main_window();
    const SDL_WindowID id = window_id(window);
    ecs_kill(window);
    test_uint(window, destroyed_window);
    test_uint(id, destroyed_id);
    ecs_fini();
}

void window_slot_rename(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    ecs_set(window, Window, { .slot = "editor", .title = "test", .width = 640, .height = 480 });
    test_uint(window, siwindow_find("editor"));
    test_uint(0, siwindow_find("main"));
    ecs_entity_t other = main_window();
    test_uint(other, siwindow_find("main"));
    ecs_set(other, Window, { .slot = "editor", .title = "test", .width = 640, .height = 480 });
    test_uint(window, siwindow_find("editor"));
    test_uint(other, siwindow_find("main"));
    ecs_fini();
}

void window_quit_event_quits_when_kept_open(void) {
    start((siwindow_props_t){ .keep_open_on_close = true });
    main_window();
    push((SDL_Event){ .type = SDL_EVENT_QUIT });
    test_false(ecs_progress());
    ecs_fini();
}

void window_pointer_ignores_other_window(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    ecs_entity_t other = ecs_new();
    ecs_set(other, Window, { .slot = "other", .title = "other", .width = 320, .height = 200 });
    ecs_progress();
    /* Creating a window may move focus to it: aim at the one without it. */
    const ecs_entity_t unfocused =
        ecs_get_resource_read(PointerState)->window == window ? other : window;
    SDL_Event motion = { .type = SDL_EVENT_MOUSE_MOTION };
    motion.motion.windowID = window_id(unfocused);
    motion.motion.x = 50.0f;
    push(motion);
    push_button(unfocused, true);
    ecs_progress();
    const PointerState *pointer = ecs_get_resource_read(PointerState);
    test_false(pointer->moved);
    test_uint(0, pointer->edge_count);
    test_uint(0, pointer->buttons);
    ecs_fini();
}

void window_close_request_quits(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    observe(window, SiWindowCloseRequested);
    test_true(ecs_progress());
    push_window(window, SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    test_false(ecs_progress());
    test_uint(1, event_count);
    test_uint(window, last_event.window);
    ecs_fini();
}

void window_close_request_keeps_open(void) {
    start((siwindow_props_t){ .keep_open_on_close = true });
    ecs_entity_t window = main_window();
    observe(window, SiWindowCloseRequested);
    push_window(window, SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    test_true(ecs_progress());
    test_uint(1, event_count);
    test_assert(siwindow_native(window) != NULL);
    ecs_fini();
}

void window_escape_requests_close(void) {
    start((siwindow_props_t){ .keep_open_on_close = true });
    ecs_entity_t window = main_window();
    observe(window, SiWindowCloseRequested);
    push_key(window, SDL_SCANCODE_ESCAPE, true, false);
    test_true(ecs_progress());
    test_uint(1, event_count);
    ecs_fini();
}

void window_escape_kept_open_is_a_key(void) {
    start((siwindow_props_t){ .keep_open_on_escape = true });
    ecs_entity_t window = main_window();
    observe(window, SiWindowCloseRequested);
    push_key(window, SDL_SCANCODE_ESCAPE, true, false);
    test_true(ecs_progress());
    test_uint(0, event_count);
    test_true(siwindow_key_pressed(ecs_get_resource_read(KeyboardState), KeyEscape));
    ecs_fini();
}

void window_key_edges_last_one_frame(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    const KeyboardState *keyboard = ecs_get_resource_read(KeyboardState);
    push_key(window, SDL_SCANCODE_W, true, false);
    ecs_progress();
    test_true(siwindow_key_down(keyboard, KeyW));
    test_true(siwindow_key_pressed(keyboard, KeyW));
    test_false(siwindow_key_down(keyboard, KeyS));
    ecs_progress();
    test_true(siwindow_key_down(keyboard, KeyW));
    test_false(siwindow_key_pressed(keyboard, KeyW));
    push_key(window, SDL_SCANCODE_W, false, false);
    ecs_progress();
    test_false(siwindow_key_down(keyboard, KeyW));
    test_true(siwindow_key_released(keyboard, KeyW));
    ecs_progress();
    test_false(siwindow_key_released(keyboard, KeyW));
    ecs_fini();
}

void window_key_repeat_is_not_a_press(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    const KeyboardState *keyboard = ecs_get_resource_read(KeyboardState);
    push_key(window, SDL_SCANCODE_A, true, false);
    ecs_progress();
    push_key(window, SDL_SCANCODE_A, true, true);
    ecs_progress();
    test_true(siwindow_key_down(keyboard, KeyA));
    test_false(siwindow_key_pressed(keyboard, KeyA));
    ecs_fini();
}

void window_focus_lost_releases_input(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    ecs_progress(); /* the driver reports its own focus events at creation */
    observe(window, SiWindowFocusChanged);
    push_key(window, SDL_SCANCODE_D, true, false);
    push_button(window, true);
    ecs_progress();
    const PointerState *pointer = ecs_get_resource_read(PointerState);
    test_uint(SDL_BUTTON_LMASK, pointer->buttons);
    push_window(window, SDL_EVENT_WINDOW_FOCUS_LOST);
    ecs_progress();
    const KeyboardState *keyboard = ecs_get_resource_read(KeyboardState);
    test_false(siwindow_key_down(keyboard, KeyD));
    test_true(siwindow_key_released(keyboard, KeyD));
    test_uint(0, pointer->buttons);
    test_uint(1, pointer->edge_count);
    test_uint(SiPointerEdgeCancel, pointer->edges[0].kind);
    test_uint(1, event_count);
    test_false(last_event.focused);
    ecs_fini();
}

void window_pointer_motion_accumulates(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    for (int i = 0; i < 2; i++) {
        SDL_Event event = { .type = SDL_EVENT_MOUSE_MOTION };
        event.motion.windowID = window_id(window);
        event.motion.x = 5.0f * (float)(i + 1);
        event.motion.y = 7.0f;
        event.motion.xrel = 2.0f;
        event.motion.yrel = -1.0f;
        push(event);
    }
    ecs_progress();
    const PointerState *pointer = ecs_get_resource_read(PointerState);
    test_true(pointer->moved);
    test_flt(4.0f, pointer->delta_x);
    test_flt(-2.0f, pointer->delta_y);
    test_flt(10.0f, pointer->x);
    ecs_progress();
    test_false(pointer->moved);
    test_flt(0.0f, pointer->delta_x);
    test_flt(10.0f, pointer->x);
    ecs_fini();
}

void window_pointer_edges_overflow(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = main_window();
    for (int i = 0; i < 70; i++)
        push_button(window, i % 2 == 0);
    ecs_progress();
    const PointerState *pointer = ecs_get_resource_read(PointerState);
    test_uint(SIWINDOW_POINTER_EDGE_CAPACITY, pointer->edge_count);
    test_uint(6, pointer->dropped_edges);
    test_uint(SiPointerEdgeDown, pointer->edges[0].kind);
    test_uint(SiPointerEdgeUp, pointer->edges[1].kind);
    test_flt(20.0f, pointer->edges[0].y);
    ecs_fini();
}

void window_text_input_event(void) {
    start((siwindow_props_t){ 0 });
    ecs_entity_t window = ecs_new();
    ecs_set(window, Window, { .slot = "main", .title = "text", .text_input = true });
    observe(window, SiWindowText);
    static const char text[] = "\xc3\xa9t\xc3\xa9";
    SDL_Event event = { .type = SDL_EVENT_TEXT_INPUT };
    event.text.windowID = window_id(window);
    event.text.text = text;
    push(event);
    ecs_progress();
    test_uint(1, event_count);
    test_str(text, last_event.text);
    ecs_fini();
}
