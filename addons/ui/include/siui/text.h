#ifndef SIUI_TEXT_H
#define SIUI_TEXT_H
#include "siui/style.h"
#include <siecs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t siui_font_handle_t;

SIREFLECT_ENUM(siui_text_wrap_t, { SIUI_TEXT_NOWRAP, SIUI_TEXT_WRAP });

SIREFLECT_ENUM(siui_text_halign_t, { SIUI_TEXT_LEFT, SIUI_TEXT_CENTER, SIUI_TEXT_RIGHT });

SIREFLECT_ENUM(siui_text_valign_t, { SIUI_TEXT_TOP, SIUI_TEXT_MIDDLE, SIUI_TEXT_BOTTOM });

#ifdef __cplusplus
#define SIUI_CPP_DEFAULT_TEXT_COLOR = { 255, 255, 255, 255 }
#else
#define SIUI_CPP_DEFAULT_TEXT_COLOR
#endif
/* The C++ default member initializer cannot appear in Sireflect's C layout.
 * Both field views stay layout-checked by ECS_COMPONENT_DECLARE_CPP_REFLECTED. */
// clang-format off
ECS_COMPONENT_DECLARE_CPP_REFLECTED(
    UiText,
    ECS_CPP_FIELDS(
        const char *text;
        uint32_t font;
        float font_size, line_height;
        siui_color_t color SIUI_CPP_DEFAULT_TEXT_COLOR;
        siui_text_wrap_t wrap;
        siui_text_halign_t horizontal_align;
        siui_text_valign_t vertical_align;
    ),
    ECS_CPP_FIELDS(
        const char *text;
        uint32_t font;
        float font_size, line_height;
        siui_color_t color;
        siui_text_wrap_t wrap;
        siui_text_halign_t horizontal_align;
        siui_text_valign_t vertical_align;
    ),
    ECS_CPP_METHODS()
);
// clang-format on
#undef SIUI_CPP_DEFAULT_TEXT_COLOR

#ifdef __cplusplus
extern "C" {
#endif

SIECS_PUBLIC_API siui_font_handle_t siui_font_open(const char *path, float size);
SIECS_PUBLIC_API void siui_font_close(siui_font_handle_t font);

#ifdef __cplusplus
}
#endif
#endif
