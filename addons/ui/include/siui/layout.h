#ifndef SIUI_LAYOUT_H
#define SIUI_LAYOUT_H
#include <siecs.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

SIREFLECT_ENUM(siui_unit_t, { SIUI_AUTO, SIUI_PX, SIUI_PERCENT });

SIREFLECT_STRUCT(siui_length_t, {
    float value;
    siui_unit_t unit;
});

SIREFLECT_STRUCT(siui_edges_t, { siui_length_t top, right, bottom, left; });

static inline siui_length_t siui_auto(void) { return (siui_length_t){ 0, SIUI_AUTO }; }

static inline siui_length_t siui_px(float value) { return (siui_length_t){ value, SIUI_PX }; }

static inline siui_length_t siui_percent(float value) {
    return (siui_length_t){ value, SIUI_PERCENT };
}

static inline siui_edges_t siui_all(float value) {
    siui_length_t v = siui_px(value);
    return (siui_edges_t){ v, v, v, v };
}

static inline siui_edges_t siui_xy(float x, float y) {
    return (siui_edges_t){ siui_px(y), siui_px(x), siui_px(y), siui_px(x) };
}

static inline siui_edges_t siui_trbl(float t, float r, float b, float l) {
    return (siui_edges_t){ siui_px(t), siui_px(r), siui_px(b), siui_px(l) };
}

SIREFLECT_ENUM(siui_direction_t, { SIUI_ROW, SIUI_COLUMN });

SIREFLECT_ENUM(siui_wrap_t, { SIUI_NOWRAP, SIUI_WRAP });

SIREFLECT_ENUM(
    siui_justify_t,
    { SIUI_START, SIUI_CENTER, SIUI_END, SIUI_SPACE_BETWEEN, SIUI_SPACE_AROUND, SIUI_SPACE_EVENLY }
);

SIREFLECT_ENUM(
    siui_align_t,
    { SIUI_ALIGN_AUTO, SIUI_ALIGN_START, SIUI_ALIGN_CENTER, SIUI_ALIGN_END, SIUI_STRETCH }
);

SIREFLECT_ENUM(siui_position_t, { SIUI_FLOW, SIUI_ABSOLUTE });

SIREFLECT_ENUM(siui_overflow_t, { SIUI_VISIBLE, SIUI_HIDDEN });

#ifdef __cplusplus
}
#endif
// clang-format off
ECS_COMPONENT_DECLARE_CPP(
    UiNode,
    ECS_CPP_FIELDS(
        siui_length_t width, height;
        siui_length_t min_width, min_height, max_width, max_height;
        siui_edges_t margin, padding, border_width;
        siui_direction_t direction;
        siui_wrap_t wrap;
        siui_justify_t justify;
        siui_align_t align_items, align_self, align_content;
        float gap, row_gap, column_gap;
        float flex_grow, flex_shrink;
        siui_length_t flex_basis;
        siui_position_t position;
        siui_length_t top, right, bottom, left;
        float aspect_ratio;
        siui_overflow_t overflow;
        int16_t order, z_index;
    ),
    ECS_CPP_METHODS(
        UiNode &wpx(float value) {
            width = siui_px(value);
            return *this;
        }

        UiNode &hpx(float value) {
            height = siui_px(value);
            return *this;
        }

        UiNode &wpercent(float value) {
            width = siui_percent(value);
            return *this;
        }

        UiNode &hpercent(float value) {
            height = siui_percent(value);
            return *this;
        }

        UiNode &minwpx(float value) {
            min_width = siui_px(value);
            return *this;
        }

        UiNode &maxwpx(float value) {
            max_width = siui_px(value);
            return *this;
        }

        UiNode &minhpx(float value) {
            min_height = siui_px(value);
            return *this;
        }

        UiNode &maxhpx(float value) {
            max_height = siui_px(value);
            return *this;
        }

        UiNode &row() {
            direction = SIUI_ROW;
            return *this;
        }

        UiNode &column() {
            direction = SIUI_COLUMN;
            return *this;
        }

        UiNode &gap_px(float value) {
            gap = value;
            return *this;
        }

        UiNode &row_gap_px(float value) {
            row_gap = value;
            return *this;
        }

        UiNode &column_gap_px(float value) {
            column_gap = value;
            return *this;
        }

        UiNode &padding_px(float value) {
            padding = siui_all(value);
            return *this;
        }

        UiNode &padding_xy(float x, float y) {
            padding = siui_xy(x, y);
            return *this;
        }

        UiNode &margin_px(float value) {
            margin = siui_all(value);
            return *this;
        }

        UiNode &margin_xy(float x, float y) {
            margin = siui_xy(x, y);
            return *this;
        }

        UiNode &border_px(float value) {
            border_width = siui_all(value);
            return *this;
        }

        UiNode &grow(float value) {
            flex_grow = value;
            return *this;
        }

        UiNode &shrink(float value) {
            flex_shrink = value;
            return *this;
        }

        UiNode &items(siui_align_t value) {
            align_items = value;
            return *this;
        }

        UiNode &self(siui_align_t value) {
            align_self = value;
            return *this;
        }

        UiNode &content(siui_align_t value) {
            align_content = value;
            return *this;
        }

        UiNode &justify_content(siui_justify_t value) {
            justify = value;
            return *this;
        }

        UiNode &space_between() {
            justify = SIUI_SPACE_BETWEEN;
            return *this;
        }

        UiNode &wrap_children() {
            wrap = SIUI_WRAP;
            return *this;
        }

        UiNode &absolute() {
            position = SIUI_ABSOLUTE;
            return *this;
        }

        UiNode &flex_basis_px(float value) {
            flex_basis = siui_px(value);
            return *this;
        }

        UiNode &left_px(float value) {
            left = siui_px(value);
            return *this;
        }

        UiNode &right_px(float value) {
            right = siui_px(value);
            return *this;
        }

        UiNode &top_px(float value) {
            top = siui_px(value);
            return *this;
        }

        UiNode &bottom_px(float value) {
            bottom = siui_px(value);
            return *this;
        }

        UiNode &aspect(float value) {
            aspect_ratio = value;
            return *this;
        }

        UiNode &order_by(int16_t value) {
            order = value;
            return *this;
        }

        UiNode &zindex(int16_t value) {
            z_index = value;
            return *this;
        }

        UiNode &clip_children() {
            overflow = SIUI_HIDDEN;
            return *this;
        }
    )
);
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UiComputedLayout {
    float x, y, width, height;
    float content_x, content_y, content_width, content_height;
    float clip_x, clip_y, clip_width, clip_height;
} UiComputedLayout;

typedef struct {
    uint64_t tree_sync_ns, measure_ns, arrange_ns, paint_collection_ns;
    uint64_t gpu_upload_ns;
    uint32_t draw_calls, measured_nodes, arranged_nodes;
    uint64_t generation;
} siui_stats_t;

SIECS_PUBLIC_API bool siui_layout_update(float viewport_width, float viewport_height);

SIECS_PUBLIC_API const UiComputedLayout *siui_layout(ecs_entity_t entity);

SIECS_PUBLIC_API siui_stats_t siui_stats(void);

#ifdef __cplusplus
}
#endif
#endif
