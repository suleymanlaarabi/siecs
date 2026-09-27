#ifndef SIUI_STYLE_H
#define SIUI_STYLE_H
#include <siecs.h>
#include <stdint.h>
typedef struct { uint8_t r,g,b,a; } siui_color_t;
typedef struct UiPaint {
    siui_color_t background;
    siui_color_t border_top, border_right, border_bottom, border_left;
} UiPaint;
#ifdef __cplusplus
extern "C" {
#endif
SIECS_PUBLIC_API extern ecs_component_t ecs_id(UiPaint);
SIECS_PUBLIC_API extern ecs_component_desc_t ecs_id(UiPaint_desc);
#ifdef __cplusplus
}
#endif
#endif
