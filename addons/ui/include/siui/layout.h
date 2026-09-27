#ifndef SIUI_LAYOUT_H
#define SIUI_LAYOUT_H
#include <siecs.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { SIUI_AUTO, SIUI_PX, SIUI_PERCENT } siui_unit_t;
typedef struct { float value; siui_unit_t unit; } siui_length_t;
typedef struct { siui_length_t top, right, bottom, left; } siui_edges_t;
static inline siui_length_t siui_auto(void) { return (siui_length_t){0, SIUI_AUTO}; }
static inline siui_length_t siui_px(float value) { return (siui_length_t){value, SIUI_PX}; }
static inline siui_length_t siui_percent(float value) { return (siui_length_t){value, SIUI_PERCENT}; }
static inline siui_edges_t siui_all(float value) {
    siui_length_t v = siui_px(value);
    return (siui_edges_t){v,v,v,v};
}
static inline siui_edges_t siui_xy(float x, float y) {
    return (siui_edges_t){siui_px(y),siui_px(x),siui_px(y),siui_px(x)};
}
static inline siui_edges_t siui_trbl(float t, float r, float b, float l) {
    return (siui_edges_t){siui_px(t),siui_px(r),siui_px(b),siui_px(l)};
}
typedef enum { SIUI_ROW, SIUI_COLUMN } siui_direction_t;
typedef enum { SIUI_NOWRAP, SIUI_WRAP } siui_wrap_t;
typedef enum {
    SIUI_START, SIUI_CENTER, SIUI_END, SIUI_SPACE_BETWEEN,
    SIUI_SPACE_AROUND, SIUI_SPACE_EVENLY
} siui_justify_t;
typedef enum { SIUI_ALIGN_AUTO, SIUI_ALIGN_START, SIUI_ALIGN_CENTER, SIUI_ALIGN_END, SIUI_STRETCH } siui_align_t;
typedef enum { SIUI_FLOW, SIUI_ABSOLUTE } siui_position_t;
typedef enum { SIUI_VISIBLE, SIUI_HIDDEN } siui_overflow_t;
typedef struct UiNode {
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
} UiNode;
SIECS_PUBLIC_API extern ecs_component_t ecs_id(UiNode);
SIECS_PUBLIC_API extern ecs_component_desc_t ecs_id(UiNode_desc);
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
