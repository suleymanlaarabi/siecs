#ifndef SIUI_RENDER_INTERNAL_H
#define SIUI_RENDER_INTERNAL_H
#include "ui_internal.h"
typedef struct {
    float x,y,width,height;
    siui_color_t background;
    float top,right,bottom,left;
    siui_color_t top_color,right_color,bottom_color,left_color;
} ui_rect_instance_t;
typedef struct {
    float x,y,u,v;
    siui_color_t color;
    float image_type;
} ui_text_vertex_t;
typedef enum {UI_DRAW_RECT,UI_DRAW_TEXT,UI_DRAW_SOLID_TEXT} ui_draw_kind_t;
typedef struct {
    ui_draw_kind_t kind;
    SDL_GPUTexture *atlas;
    SDL_Rect clip;
    uint32_t first,count;
} ui_draw_command_t;
typedef struct {
    ui_rect_instance_t *rects;
    ui_text_vertex_t *vertices;
    ui_draw_command_t *commands;
    uint32_t rect_count,rect_capacity,vertex_count,vertex_capacity,command_count,command_capacity;
} ui_batches_t;
extern ui_batches_t ui_batches;
void ui_collect(const sigpu_overlay_context_t *context);
void ui_batches_fini(void);
#endif
