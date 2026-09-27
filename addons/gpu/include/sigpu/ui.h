#ifndef SIECS_SIGPU_UI_H
#define SIECS_SIGPU_UI_H

#include "sigpu/rendering.h"
#include <stdint.h>

#ifdef __cplusplus
namespace ui {
enum class ValueType : uint8_t { Px, Percent, Auto };
enum class Direction : uint8_t { Column, Row };
enum class Justify : uint8_t { Start, Center, End, SpaceBetween, SpaceAround };
enum class Align : uint8_t { Start, Center, End, SpaceBetween, SpaceAround };
}

using SiUiValueType = ui::ValueType;
using SiUiDirection = ui::Direction;
using SiUiJustify = ui::Justify;
using SiUiAlign = ui::Align;
#else
typedef uint8_t SiUiValueType;
typedef uint8_t SiUiDirection;
typedef uint8_t SiUiJustify;
typedef uint8_t SiUiAlign;

enum {
    SI_UI_VALUE_PX,
    SI_UI_VALUE_PERCENT,
    SI_UI_VALUE_AUTO,
};

enum {
    SI_UI_DIRECTION_COLUMN,
    SI_UI_DIRECTION_ROW,
};

enum {
    SI_UI_JUSTIFY_START,
    SI_UI_JUSTIFY_CENTER,
    SI_UI_JUSTIFY_END,
    SI_UI_JUSTIFY_SPACE_BETWEEN,
    SI_UI_JUSTIFY_SPACE_AROUND,
};

enum {
    SI_UI_ALIGN_START,
    SI_UI_ALIGN_CENTER,
    SI_UI_ALIGN_END,
    SI_UI_ALIGN_SPACE_BETWEEN,
    SI_UI_ALIGN_SPACE_AROUND,
};
#endif

typedef struct SiUiValue {
    SiUiValueType type;
    float value;
#ifdef __cplusplus
    constexpr SiUiValue() : type(ui::ValueType::Percent), value(100.0f) {}
    constexpr SiUiValue(ui::ValueType value_type, float number)
        : type(value_type), value(number) {}

    static constexpr SiUiValue px(float number) { return { ui::ValueType::Px, number }; }
    static constexpr SiUiValue percent(float number) {
        return { ui::ValueType::Percent, number };
    }
    static constexpr SiUiValue auto_size() { return { ui::ValueType::Auto, 0.0f }; }
#endif
} SiUiValue;

typedef struct SiUiInsets {
    float left;
    float top;
    float right;
    float bottom;
#ifdef __cplusplus
    constexpr SiUiInsets() : left(0), top(0), right(0), bottom(0) {}
    constexpr explicit SiUiInsets(float value)
        : left(value), top(value), right(value), bottom(value) {}
    constexpr SiUiInsets(float horizontal, float vertical)
        : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}
    constexpr SiUiInsets(float l, float t, float r, float b)
        : left(l), top(t), right(r), bottom(b) {}
#endif
} SiUiInsets;

ECS_COMPONENT_DECLARE_CPP(
    UiNode,
    ECS_CPP_FIELDS(
        SiUiValue width;
        SiUiValue height;
        SiUiDirection direction;
        SiUiValue gap;
        SiUiJustify justify;
        SiUiAlign align;
        SiUiInsets padding;
        int32_t order;
    ),
    ECS_CPP_METHODS(
        UiNode()
            : width(),
              height(),
              direction(ui::Direction::Row),
              gap(ui::ValueType::Px, 0.0f),
              justify(ui::Justify::Start),
              align(ui::Align::Start),
              padding(),
              order(0) {}

        static UiNode hstack() {
            UiNode node;
            node.gap = SiUiValue::px(5.0f);
            return node;
        }

        static UiNode vstack() {
            UiNode node;
            node.direction = ui::Direction::Column;
            node.gap = SiUiValue::px(4.0f);
            return node;
        }

        UiNode wpx(float number) const {
            UiNode node = *this;
            node.width = SiUiValue::px(number);
            return node;
        }

        UiNode hpx(float number) const {
            UiNode node = *this;
            node.height = SiUiValue::px(number);
            return node;
        }

        UiNode wpercent(float number) const {
            UiNode node = *this;
            node.width = SiUiValue::percent(number);
            return node;
        }

        UiNode hpercent(float number) const {
            UiNode node = *this;
            node.height = SiUiValue::percent(number);
            return node;
        }

        UiNode wauto() const {
            UiNode node = *this;
            node.width = SiUiValue::auto_size();
            return node;
        }

        UiNode hauto() const {
            UiNode node = *this;
            node.height = SiUiValue::auto_size();
            return node;
        }

        UiNode gap_px(float number) const {
            UiNode node = *this;
            node.gap = SiUiValue::px(number);
            return node;
        }

        UiNode with_padding(float number) const {
            UiNode node = *this;
            node.padding = SiUiInsets(number);
            return node;
        }

        UiNode with_justify(ui::Justify value) const {
            UiNode node = *this;
            node.justify = value;
            return node;
        }

        UiNode with_align(ui::Align value) const {
            UiNode node = *this;
            node.align = value;
            return node;
        }

        UiNode with_order(int32_t value) const {
            UiNode node = *this;
            node.order = value;
            return node;
        }
    )
);

ECS_COMPONENT_DECLARE_CPP(
    UiBackground,
    ECS_CPP_FIELDS(Color color;),
    ECS_CPP_METHODS(
        UiBackground() : color(0, 0, 0, 0) {}
        explicit UiBackground(Color value) : color(value) {}
    )
);

ECS_COMPONENT_DECLARE_CPP(
    UiBorder,
    ECS_CPP_FIELDS(float width; Color color;),
    ECS_CPP_METHODS(
        UiBorder() : width(1.0f), color(0, 0, 0, 255) {}
        UiBorder(float border_width, Color border_color)
            : width(border_width), color(border_color) {}
    )
);

#ifdef __cplusplus
namespace ui {
using Value = ::SiUiValue;
using Insets = ::SiUiInsets;
using Node = ::UiNode;
using Background = ::UiBackground;
using Border = ::UiBorder;
}
#else
SIECS_PUBLIC_API SiUiValue sigpu_ui_px(float value);
SIECS_PUBLIC_API SiUiValue sigpu_ui_percent(float value);
SIECS_PUBLIC_API SiUiValue sigpu_ui_auto(void);
SIECS_PUBLIC_API UiNode sigpu_ui_hstack(void);
SIECS_PUBLIC_API UiNode sigpu_ui_vstack(void);

SIECS_PUBLIC_API ecs_entity_t sigpu_ui_create(ecs_entity_t parent, const UiNode *node);
SIECS_PUBLIC_API void sigpu_ui_set_node(ecs_entity_t entity, const UiNode *node);
SIECS_PUBLIC_API void sigpu_ui_set_background(ecs_entity_t entity, Color color);
SIECS_PUBLIC_API void sigpu_ui_set_border(ecs_entity_t entity, float width, Color color);
#endif

#endif
