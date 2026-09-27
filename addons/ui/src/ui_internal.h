#ifndef SIUI_INTERNAL_H
#define SIUI_INTERNAL_H
#include <siui.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define UI_NONE UINT32_MAX
enum { TREE_DIRTY=1, MEASURE_DIRTY=2, ARRANGE_DIRTY=4, TEXT_DIRTY=8, PAINT_DIRTY=16 };
typedef struct {
    ecs_entity_t entity;
    uint32_t parent, first_child, next_sibling;
    uint32_t paint_first_child, paint_next_sibling;
    float intrinsic_min_width, intrinsic_width, intrinsic_height;
    UiComputedLayout computed;
    uint32_t flags;
    int16_t order;
    int16_t z_index;
    uint64_t generation;
    TTF_Text *ttf_text;
    float text_wrap_width;
    siui_text_handle_t text_handle;
    siui_font_handle_t font_handle;
    float font_size;
    float line_height;
} ui_entry_t;
ECS_RESOURCE_DECLARE(UiLayoutCache, {
    ui_entry_t *entries;
    uint32_t *walk, *paint_walk, *sort, *scratch;
    float *main_sizes, *cross_sizes, *line_crosses;
    uint32_t *line_starts;
    uint32_t count, capacity, roots;
    bool tree_dirty, layout_dirty, paint_dirty;
    float viewport_width, viewport_height;
    uint64_t generation;
    siui_stats_t stats;
    ecs_query_id_t query;
});
typedef UiLayoutCache ui_cache_t;
extern ui_cache_t *ui_cache_ptr;
#define ui_cache (*ui_cache_ptr)
uint32_t ui_find(ecs_entity_t entity);
void ui_tree_sync(void);
void ui_mark(ecs_entity_t entity, uint32_t flags);
void ui_measure(void);
void ui_arrange(void);
void ui_text_dispose(ui_entry_t *entry);
void ui_text_release(siui_text_handle_t handle);
bool ui_text_measure(ui_entry_t *entry, const UiText *style, float width, float *out_w, float *out_h);
float ui_text_min_width(const UiText *style);
TTF_Font *ui_font(siui_font_handle_t handle);
TTF_Font *ui_font_sized(siui_font_handle_t handle, float size, float line_height);
void ui_fonts_fini(void);
void ui_fonts_init_default(void);
bool ui_text_init(void);
void ui_text_fini(void);
void ui_text_gpu_engine(SDL_GPUDevice *device);
TTF_TextEngine *ui_text_engine(void);
float ui_resolve(siui_length_t value, float reference, float fallback);
float ui_clamp(float value, siui_length_t minimum, siui_length_t maximum, float reference);
float ui_edge(siui_length_t value, float reference);
void ui_render_register(void);
void ui_render_fini(void);
#endif
