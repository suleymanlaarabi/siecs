#include "render_internal.h"

ui_batches_t ui_batches;

static void reserve_rects(uint32_t size) {
    if (size <= ui_batches.rect_capacity)
        return;
    uint32_t capacity = ui_batches.rect_capacity ? ui_batches.rect_capacity : 64;
    while (capacity < size)
        capacity *= 2;
    ui_batches.rects = SDL_realloc(ui_batches.rects, capacity * sizeof(ui_rect_instance_t));
    ui_batches.rect_capacity = capacity;
}

static void reserve_vertices(uint32_t size) {
    if (size <= ui_batches.vertex_capacity)
        return;
    uint32_t capacity = ui_batches.vertex_capacity ? ui_batches.vertex_capacity : 256;
    while (capacity < size)
        capacity *= 2;
    ui_batches.vertices = SDL_realloc(ui_batches.vertices, capacity * sizeof(ui_text_vertex_t));
    ui_batches.vertex_capacity = capacity;
}

static void reserve_commands(uint32_t size) {
    if (size <= ui_batches.command_capacity)
        return;
    uint32_t capacity = ui_batches.command_capacity ? ui_batches.command_capacity : 64;
    while (capacity < size)
        capacity *= 2;
    ui_batches.commands = SDL_realloc(ui_batches.commands, capacity * sizeof(ui_draw_command_t));
    ui_batches.command_capacity = capacity;
}

static SDL_Rect clip_rect(const UiComputedLayout *c, uint32_t width, uint32_t height) {
    int x = SDL_clamp((int)floorf(c->clip_x), 0, (int)width);
    int y = SDL_clamp((int)floorf(c->clip_y), 0, (int)height);
    int right = SDL_clamp((int)ceilf(c->clip_x + c->clip_width), 0, (int)width);
    int bottom = SDL_clamp((int)ceilf(c->clip_y + c->clip_height), 0, (int)height);
    return (SDL_Rect){ x, y, SDL_max(0, right - x), SDL_max(0, bottom - y) };
}

static bool same_clip(SDL_Rect a, SDL_Rect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static void
command(ui_draw_kind_t kind, SDL_GPUTexture *atlas, SDL_Rect clip, uint32_t first, uint32_t count) {
    if (clip.w <= 0 || clip.h <= 0)
        return;
    if (ui_batches.command_count) {
        ui_draw_command_t *last = &ui_batches.commands[ui_batches.command_count - 1];
        if (last->kind == kind && last->atlas == atlas && same_clip(last->clip, clip) &&
            last->first + last->count == first) {
            last->count += count;
            return;
        }
    }
    reserve_commands(ui_batches.command_count + 1);
    ui_batches.commands[ui_batches.command_count++] =
        (ui_draw_command_t){ kind, atlas, clip, first, count };
}

static void
collect_rect(const ui_entry_t *entry, const UiNode *node, const UiPaint *paint, SDL_Rect clip) {
    const UiComputedLayout *c = &entry->computed;
    if (c->width <= 0 || c->height <= 0)
        return;
    reserve_rects(ui_batches.rect_count + 1);
    uint32_t first = ui_batches.rect_count++;
    ui_batches.rects[first] = (ui_rect_instance_t){
        .x = c->x,
        .y = c->y,
        .width = c->width,
        .height = c->height,
        .background = paint->background,
        .top = ui_edge(node->border_width.top, c->width),
        .right = ui_edge(node->border_width.right, c->width),
        .bottom = ui_edge(node->border_width.bottom, c->width),
        .left = ui_edge(node->border_width.left, c->width),
        .top_color = paint->border_top,
        .right_color = paint->border_right,
        .bottom_color = paint->border_bottom,
        .left_color = paint->border_left,
    };
    command(UI_DRAW_RECT, NULL, clip, first, 1);
}

static void collect_text(ui_entry_t *entry, const UiText *style, SDL_Rect clip) {
    const UiComputedLayout *c = &entry->computed;
    float width = 0, height = 0;
    if (!ui_text_measure(entry, style, c->content_width, &width, &height) || !entry->ttf_text)
        return;
    float ox = c->content_x, oy = c->content_y;
    if (style->horizontal_align == SIUI_TEXT_CENTER)
        ox += (c->content_width - width) * 0.5f;
    else if (style->horizontal_align == SIUI_TEXT_RIGHT)
        ox += c->content_width - width;
    if (style->vertical_align == SIUI_TEXT_MIDDLE)
        oy += (c->content_height - height) * 0.5f;
    else if (style->vertical_align == SIUI_TEXT_BOTTOM)
        oy += c->content_height - height;
    for (TTF_GPUAtlasDrawSequence *seq = TTF_GetGPUTextDrawData(entry->ttf_text); seq;
         seq = seq->next) {
        if (seq->num_indices <= 0)
            continue;
        uint32_t first = ui_batches.vertex_count;
        reserve_vertices(first + (uint32_t)seq->num_indices);
        for (int i = 0; i < seq->num_indices; i++) {
            int v = seq->indices[i];
            ui_batches.vertices[ui_batches.vertex_count++] = (ui_text_vertex_t){
                .x = ox + seq->xy[v].x,
                .y = oy - seq->xy[v].y,
                .u = seq->uv ? seq->uv[v].x : 0,
                .v = seq->uv ? seq->uv[v].y : 0,
                .color = style->color,
                .image_type = (float)seq->image_type,
            };
        }
        command(
            seq->atlas_texture ? UI_DRAW_TEXT : UI_DRAW_SOLID_TEXT,
            seq->atlas_texture,
            clip,
            first,
            (uint32_t)seq->num_indices
        );
    }
}

void ui_collect(const sigpu_overlay_context_t *context) {
    if (!ui_cache.paint_dirty)
        return;
    uint64_t start = SDL_GetTicksNS();
    ui_batches.rect_count = ui_batches.vertex_count = ui_batches.command_count = 0;
    for (uint32_t i = 0; i < ui_cache.count; i++) {
        ui_entry_t *entry = &ui_cache.entries[ui_cache.paint_walk[i]];
        SDL_Rect clip = clip_rect(&entry->computed, context->width, context->height);
        const UiPaint *paint = ecs_try_get(entry->entity, UiPaint);
        if (paint) {
            SDL_Rect paint_clip =
                entry->parent == UI_NONE
                    ? (SDL_Rect){ 0, 0, (int)context->width, (int)context->height }
                    : clip_rect(
                          &ui_cache.entries[entry->parent].computed,
                          context->width,
                          context->height
                      );
            collect_rect(entry, &entry->node, paint, paint_clip);
        }
        const UiText *style = ecs_try_get(entry->entity, UiText);
        if (style)
            collect_text(entry, style, clip);
    }
    ui_cache.paint_dirty = false;
    ui_cache.stats.paint_collection_ns = SDL_GetTicksNS() - start;
}

void ui_batches_fini(void) {
    SDL_free(ui_batches.rects);
    SDL_free(ui_batches.vertices);
    SDL_free(ui_batches.commands);
    ui_batches = (ui_batches_t){ 0 };
}
