#ifndef SIUI_STYLE_H
#define SIUI_STYLE_H
#include <siecs.h>
#include <stdint.h>

SIREFLECT_STRUCT(siui_color_t, { uint8_t r, g, b, a; });

// clang-format off
ECS_COMPONENT_DECLARE_CPP(
    UiPaint,
    ECS_CPP_FIELDS(
        siui_color_t background;
        siui_color_t border_top, border_right, border_bottom, border_left;
    ),
    ECS_CPP_METHODS(
        UiPaint &bg(siui_color_t color) {
            background = color;
            return *this;
        }

        UiPaint &border_color(siui_color_t color) {
            border_top = border_right = border_bottom = border_left = color;
            return *this;
        }

        UiPaint &border_top_color(siui_color_t color) {
            border_top = color;
            return *this;
        }

        UiPaint &border_right_color(siui_color_t color) {
            border_right = color;
            return *this;
        }

        UiPaint &border_bottom_color(siui_color_t color) {
            border_bottom = color;
            return *this;
        }

        UiPaint &border_left_color(siui_color_t color) {
            border_left = color;
            return *this;
        }
    )
);
// clang-format on
#endif
