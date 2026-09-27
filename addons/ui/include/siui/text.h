#ifndef SIUI_TEXT_H
#define SIUI_TEXT_H
#include "siui/style.h"
#include <siecs.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t siui_text_handle_t;
typedef uint32_t siui_font_handle_t;
typedef enum { SIUI_TEXT_NOWRAP, SIUI_TEXT_WRAP } siui_text_wrap_t;
typedef enum { SIUI_TEXT_LEFT, SIUI_TEXT_CENTER, SIUI_TEXT_RIGHT } siui_text_halign_t;
typedef enum { SIUI_TEXT_TOP, SIUI_TEXT_MIDDLE, SIUI_TEXT_BOTTOM } siui_text_valign_t;
typedef struct UiText {
    siui_text_handle_t text;
    siui_font_handle_t font;
    float font_size, line_height;
    siui_color_t color;
    siui_text_wrap_t wrap;
    siui_text_halign_t horizontal_align;
    siui_text_valign_t vertical_align;
} UiText;
#ifdef __cplusplus
extern "C" {
#endif
SIECS_PUBLIC_API extern ecs_component_t ecs_id(UiText);
SIECS_PUBLIC_API extern ecs_component_desc_t ecs_id(UiText_desc);
SIECS_PUBLIC_API siui_font_handle_t siui_font_open(const char *path, float size);
SIECS_PUBLIC_API void siui_font_close(siui_font_handle_t font);
SIECS_PUBLIC_API bool siui_set_text(ecs_entity_t entity, const char *utf8, size_t length);
SIECS_PUBLIC_API bool siui_set_text_styled(ecs_entity_t entity, const char *utf8, size_t length,
                                           const UiText *style);
SIECS_PUBLIC_API const char *siui_text_string(siui_text_handle_t handle);
#ifdef __cplusplus
}
#endif
#endif
