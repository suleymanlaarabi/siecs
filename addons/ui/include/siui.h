#ifndef SIUI_H
#define SIUI_H
#include "siui/layout.h"
#include "siui/style.h"
#include "siui/text.h"

/*
 * gpu draws the UI with sigpu (RFC section 13): two layered draw features,
 * rectangles and text, in a pass of the GPU_ORDER_UI band of the surface
 * whose GpuSurface.window_slot is window_slot ("main" when empty), sized to
 * that window in logical pixels. Without gpu, siui only lays out.
 */
ECS_MODULE_DECLARE(siui, {
    bool gpu;
    char window_slot[32];
});

#endif
