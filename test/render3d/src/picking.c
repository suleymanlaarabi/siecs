#include "util.h"

static int events[SiPointerEventCount];
static ecs_entity_t last_target;

static void on_event(ecs_observer_event_t *event) {
    for (uint32_t kind = 0; kind < SiPointerEventCount; kind++) {
        if (event->event == sirender3d_pointer_event_id(kind)) {
            events[kind]++;
        }
    }
    last_target = ((const sirender3d_pointer_event_t *)event->trigger_data)->target;
}

static ecs_entity_t pickable(float x, float z, float size) {
    ecs_entity_t e = ecs_new();
    ecs_set(e, Position3d, { x, 0, z });
    ecs_set(e, Cuboid, { size, size, size });
    ecs_set(e, PointerEvents, { 0xffff });
    for (uint32_t kind = 0; kind < SiPointerEventCount; kind++) {
        ecs_observer({ .on = sirender3d_pointer_event_id(kind), .entity = e,
                       .callback = on_event });
    }
    return e;
}

void picking_ray_hits_nearest(void) {
    r3d_test_t t = r3d_test_start(64, 48, 1);
    pickable(0, -5, 2);
    ecs_entity_t near = pickable(0, 0, 2);
    r3d_test_frames(3);
    float origin[3], direction[3];
    test_true(sirender3d_pointer_ray(t.window, 32, 24, origin, direction));
    test_flt(10.0f, origin[2]);
    test_assert(direction[2] < -0.99f);
    float distance = 0;
    const ecs_entity_t hit = sigpu_feature_raycast(sirender3d_feature(Render3dCuboids), origin,
                                                   direction, NULL, NULL, &distance);
    test_uint(near, hit);
    r3d_test_stop();
}

void picking_click_events(void) {
    memset(events, 0, sizeof(events));
    r3d_test_t t = r3d_test_start(64, 48, 1);
    ecs_entity_t target = pickable(0, 0, 4);
    r3d_test_frames(3);
    PointerState *pointer = ecs_get_resource(PointerState);
    pointer->window = t.window;
    pointer->x = 32;
    pointer->y = 24;
    ecs_run_phase(EcsPreRender);
    test_int(1, events[SiPointerEnter]);
    test_uint(target, last_target);
    pointer->edge_count = 2;
    pointer->edges[0] = (SiPointerEdge){ .x = 32, .y = 24, .kind = SiPointerEdgeDown,
                                         .button = 1 };
    pointer->edges[1] = (SiPointerEdge){ .x = 33, .y = 24, .kind = SiPointerEdgeUp,
                                         .button = 1 };
    ecs_run_phase(EcsPreRender);
    test_int(1, events[SiPointerDown]);
    test_int(1, events[SiPointerUp]);
    test_int(1, events[SiClick]);
    r3d_test_stop();
}

void picking_dpi_independent(void) {
    r3d_test_t t = r3d_test_start(64, 48, 1);
    pickable(0, 0, 2);
    r3d_test_frames(3);
    /* Logical coordinates map through WindowState, whatever the pixel size. */
    WindowState state = *ecs_get(t.window, WindowState);
    state.pixel_width = state.width * 2;
    state.pixel_height = state.height * 2;
    ecs_set_cid(t.window, ecs_id(WindowState), &state);
    float origin[3], direction[3];
    test_true(sirender3d_pointer_ray(t.window, (float)state.width * 0.5f,
                                     (float)state.height * 0.5f, origin, direction));
    test_assert(fabsf(direction[0]) < 0.01f && fabsf(direction[1]) < 0.01f);
    r3d_test_stop();
}
