#include "ui_internal.h"
#include "shaders/ui_pack.generated.h"
#include <sigpu.h>
#include <siwindow.h>

/*
 * GPU drawing of siui (RFC section 13). After layout, every painted node
 * publishes what it draws in UiRectDraw / UiTextDraw, set only when the
 * values change; two layered features turn them into instances, ordered by
 * paint order (UiDrawOrder), in a pass of the surface's graph. Text is
 * rasterized once per change into a glyph atlas, a GpuTexture.
 */

#define UI_PACK "siui://ui.gpushader"
#define UI_ATLAS_SIZE 1024u

/* Instances, as the shader reads them. */
ECS_COMPONENT_DECLARE(UiRectDraw, {
    float x, y, width, height;
    float clip_x, clip_y, clip_width, clip_height;
    float top, right, bottom, left;
    uint32_t background;
    uint32_t top_color, right_color, bottom_color, left_color;
});
ECS_COMPONENT_DECLARE(UiTextDraw, {
    float x, y, width, height;
    float clip_x, clip_y, clip_width, clip_height;
    float u0, v0, u1, v1;
    uint32_t color;
});
ECS_COMPONENT_DECLARE(UiDrawOrder, { uint32_t paint; });
ECS_COMPONENT_DEFINE(UiRectDraw);
ECS_COMPONENT_DEFINE(UiTextDraw);
ECS_COMPONENT_DEFINE(UiDrawOrder);

_Static_assert(sizeof(UiRectDraw) == 68, "RectInstance layout");
_Static_assert(sizeof(UiTextDraw) == 52, "TextInstance layout");

typedef struct {
    bool enabled;
    char slot[32];
    ecs_entity_t phase;
    ecs_entity_t rect_program;
    ecs_entity_t text_program;
    ecs_entity_t quad;
    ecs_entity_t sampler;
    ecs_entity_t atlas;
    ecs_entity_t surface;
    ecs_entity_t graph;
    ecs_entity_t pass;
    float view[4];
    /* Shelf packing of the atlas. */
    uint32_t atlas_size;
    uint32_t shelf_x, shelf_y, shelf_height;
    uint32_t generation;
    uint8_t *pixels;
} ui_renderer_t;

static ui_renderer_t renderer;

static uint32_t pack(siui_color_t color) {
    return (uint32_t)color.r | (uint32_t)color.g << 8 | (uint32_t)color.b << 16 |
           (uint32_t)color.a << 24;
}

/* ----------------------------------------------------------------------
 * Glyph atlas
 * ---------------------------------------------------------------------- */

static void atlas_reset(uint32_t size) {
    renderer.atlas_size = size;
    renderer.shelf_x = renderer.shelf_y = renderer.shelf_height = 0;
    renderer.generation++;
    SDL_free(renderer.pixels);
    renderer.pixels = SDL_calloc(size, size);
    ecs_set(renderer.atlas, GpuTexture,
            { .format = GpuFormatR8, .usage = GpuTextureSampled, .width = size, .height = size });
}

/* A free region of the atlas, or false when it is full. */
static bool atlas_place(uint32_t width, uint32_t height, uint32_t *x, uint32_t *y) {
    const uint32_t padded_w = width + 1, padded_h = height + 1;
    if (padded_w > renderer.atlas_size || padded_h > renderer.atlas_size) {
        return false;
    }
    if (renderer.shelf_x + padded_w > renderer.atlas_size) {
        renderer.shelf_y += renderer.shelf_height;
        renderer.shelf_x = renderer.shelf_height = 0;
    }
    if (renderer.shelf_y + padded_h > renderer.atlas_size) {
        return false;
    }
    *x = renderer.shelf_x;
    *y = renderer.shelf_y;
    renderer.shelf_x += padded_w;
    renderer.shelf_height = SDL_max(renderer.shelf_height, padded_h);
    return true;
}

/* Draws the entry's text in white and keeps its alpha in the atlas. */
static bool rasterize(ui_entry_t *entry) {
    int w = 0, h = 0;
    if (!entry->ttf_text || !TTF_GetTextSize(entry->ttf_text, &w, &h) || w <= 0 || h <= 0) {
        entry->atlas_width = entry->atlas_height = 0;
        entry->raster_dirty = false;
        entry->atlas_generation = renderer.generation;
        return true;
    }
    uint32_t x, y;
    if (!atlas_place((uint32_t)w, (uint32_t)h, &x, &y)) {
        return false;
    }
    SDL_Surface *surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        return true;
    }
    SDL_FillSurfaceRect(surface, NULL, 0);
    TTF_SetTextColor(entry->ttf_text, 255, 255, 255, 255);
    TTF_DrawSurfaceText(entry->ttf_text, 0, 0, surface);
    const uint8_t *row = surface->pixels;
    for (int j = 0; j < h; j++, row += surface->pitch) {
        uint8_t *out = renderer.pixels + (size_t)(y + (uint32_t)j) * renderer.atlas_size + x;
        for (int i = 0; i < w; i++) {
            out[i] = row[i * 4 + 3];
        }
    }
    SDL_DestroySurface(surface);
    entry->atlas_x = x;
    entry->atlas_y = y;
    entry->atlas_width = (uint32_t)w;
    entry->atlas_height = (uint32_t)h;
    entry->raster_dirty = false;
    entry->atlas_generation = renderer.generation;
    const uint8_t *first = renderer.pixels + (size_t)y * renderer.atlas_size + x;
    sigpu_texture_upload(renderer.atlas,
                         &(GpuTextureRegion){ .x = x, .y = y, .width = (uint32_t)w,
                                              .height = (uint32_t)h, .depth = 1 },
                         first, (size_t)renderer.atlas_size * (uint32_t)(h - 1) + (uint32_t)w,
                         renderer.atlas_size);
    return true;
}

/* ----------------------------------------------------------------------
 * Publication
 * ---------------------------------------------------------------------- */

static void set_if_changed(ecs_entity_t entity, ecs_component_t component, const void *value,
                           size_t size) {
    const void *current = ecs_try_get_cid(entity, component);
    if (!current || !ecs_has_cid_owned(entity, component) || memcmp(current, value, size) != 0) {
        ecs_set_cid(entity, component, value);
    }
}

static void remove_owned(ecs_entity_t entity, ecs_component_t component) {
    if (ecs_has_cid_owned(entity, component)) {
        ecs_remove_cid(entity, component);
    }
}

void ui_render_forget(ecs_entity_t entity) {
    if (!renderer.enabled || !ecs_is_alive(entity)) {
        return;
    }
    remove_owned(entity, ecs_id(UiRectDraw));
    remove_owned(entity, ecs_id(UiTextDraw));
    remove_owned(entity, ecs_id(UiDrawOrder));
}

static UiRectDraw rect_draw(const ui_entry_t *entry, const UiPaint *paint,
                            const UiComputedLayout *clip) {
    const UiComputedLayout *c = &entry->computed;
    const UiNode *node = &entry->node;
    return (UiRectDraw){
        .x = c->x, .y = c->y, .width = c->width, .height = c->height,
        .clip_x = clip->clip_x, .clip_y = clip->clip_y, .clip_width = clip->clip_width,
        .clip_height = clip->clip_height,
        .top = ui_edge(node->border_width.top, c->width),
        .right = ui_edge(node->border_width.right, c->width),
        .bottom = ui_edge(node->border_width.bottom, c->width),
        .left = ui_edge(node->border_width.left, c->width),
        .background = pack(paint->background),
        .top_color = pack(paint->border_top),
        .right_color = pack(paint->border_right),
        .bottom_color = pack(paint->border_bottom),
        .left_color = pack(paint->border_left),
    };
}

static bool text_draw(ui_entry_t *entry, const UiText *style, UiTextDraw *out) {
    const UiComputedLayout *c = &entry->computed;
    float width = 0, height = 0;
    if (!ui_text_measure(entry, style, c->content_width, &width, &height) || !entry->ttf_text) {
        return false;
    }
    if ((entry->raster_dirty || entry->atlas_generation != renderer.generation) &&
        !rasterize(entry)) {
        return false;
    }
    if (!entry->atlas_width) {
        return false;
    }
    float x = c->content_x, y = c->content_y;
    if (style->horizontal_align == SIUI_TEXT_CENTER) {
        x += (c->content_width - width) * 0.5f;
    } else if (style->horizontal_align == SIUI_TEXT_RIGHT) {
        x += c->content_width - width;
    }
    if (style->vertical_align == SIUI_TEXT_MIDDLE) {
        y += (c->content_height - height) * 0.5f;
    } else if (style->vertical_align == SIUI_TEXT_BOTTOM) {
        y += c->content_height - height;
    }
    const float size = (float)renderer.atlas_size;
    *out = (UiTextDraw){
        .x = x, .y = y, .width = (float)entry->atlas_width, .height = (float)entry->atlas_height,
        .clip_x = c->clip_x, .clip_y = c->clip_y, .clip_width = c->clip_width,
        .clip_height = c->clip_height,
        .u0 = (float)entry->atlas_x / size, .v0 = (float)entry->atlas_y / size,
        .u1 = (float)(entry->atlas_x + entry->atlas_width) / size,
        .v1 = (float)(entry->atlas_y + entry->atlas_height) / size,
        /* A zero color is the C++ default, white. */
        .color = pack(style->color) ? pack(style->color) : 0xffffffffu,
    };
    return true;
}

/* Publishes every node in paint order; returns false when the atlas filled
 * up and must be rebuilt. */
static bool publish(void) {
    for (uint32_t i = 0; i < ui_cache.count; i++) {
        ui_entry_t *entry = &ui_cache.entries[ui_cache.paint_walk[i]];
        const ecs_entity_t entity = entry->entity;
        const UiPaint *paint = ecs_try_get(entity, UiPaint);
        const UiText *style = ecs_try_get(entity, UiText);
        const UiComputedLayout *c = &entry->computed;
        /* A rect is clipped by its parent, its text by itself. */
        const UiComputedLayout root_clip = { .clip_width = ui_cache.viewport_width,
                                             .clip_height = ui_cache.viewport_height };
        const UiComputedLayout *clip = entry->parent == UI_NONE
                                           ? &root_clip
                                           : &ui_cache.entries[entry->parent].computed;
        bool drawn = false;
        if (paint && c->width > 0 && c->height > 0) {
            const UiRectDraw rect = rect_draw(entry, paint, clip);
            set_if_changed(entity, ecs_id(UiRectDraw), &rect, sizeof(rect));
            drawn = true;
        } else {
            remove_owned(entity, ecs_id(UiRectDraw));
        }
        UiTextDraw text;
        if (style && text_draw(entry, style, &text)) {
            set_if_changed(entity, ecs_id(UiTextDraw), &text, sizeof(text));
            drawn = true;
        } else {
            if (style && entry->ttf_text && entry->raster_dirty) {
                return false;
            }
            remove_owned(entity, ecs_id(UiTextDraw));
        }
        if (drawn) {
            const UiDrawOrder order = { i };
            set_if_changed(entity, ecs_id(UiDrawOrder), &order, sizeof(order));
        }
    }
    return true;
}

/* ----------------------------------------------------------------------
 * Features and pass
 * ---------------------------------------------------------------------- */

static void extract_rects(ecs_iter_t *it) {
    const UiRectDraw *rects = ecs_field(it, 0);
    const UiDrawOrder *orders = ecs_field(it, 1);
    UiRectDraw *out = sigpu_instances(it);
    uint32_t *keys = sigpu_order_keys(it);
    if (!out) {
        return;
    }
    memcpy(out, rects, it->count * sizeof(UiRectDraw));
    for (uint32_t i = 0; i < it->count; i++) {
        keys[i] = orders[i].paint * 2u;
    }
}

static void extract_texts(ecs_iter_t *it) {
    const UiTextDraw *texts = ecs_field(it, 0);
    const UiDrawOrder *orders = ecs_field(it, 1);
    UiTextDraw *out = sigpu_instances(it);
    uint32_t *keys = sigpu_order_keys(it);
    if (!out) {
        return;
    }
    memcpy(out, texts, it->count * sizeof(UiTextDraw));
    for (uint32_t i = 0; i < it->count; i++) {
        keys[i] = orders[i].paint * 2u + 1u;
    }
}

static void ui_pass(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    (void)user_data;
    gpu_cmd_uniform(ctx, "ui_view", renderer.view, sizeof(renderer.view));
}

static ecs_entity_t find_surface(void) {
    ecs_entity_t found = 0;
    ecs_query_each(it, i, ecs_in(GpuSurface)) {
        const GpuSurface *surfaces = ecs_field(&it, 0);
        if (!found && strcmp(surfaces[i].window_slot, renderer.slot) == 0) {
            found = it.entities[i];
        }
    }
    return found;
}

/* The UI pass of the surface's graph, created when the graph exists. */
static void ensure_pass(void) {
    if (renderer.surface && !ecs_is_alive(renderer.surface)) {
        renderer.surface = renderer.graph = renderer.pass = 0;
    }
    if (!renderer.surface) {
        renderer.surface = find_surface();
    }
    const GpuSurfaceTargets *targets =
        renderer.surface ? ecs_try_get(renderer.surface, GpuSurfaceTargets) : NULL;
    if (!targets || (renderer.pass && renderer.graph == targets->graph &&
                     ecs_is_alive(renderer.pass))) {
        return;
    }
    renderer.graph = targets->graph;
    renderer.pass = sigpu_pass(targets->graph, &(GpuPassDesc){
        .name = "siui", .kind = GpuPassRender, .order = GPU_ORDER_UI, .phase = renderer.phase,
        .execute = ui_pass });
    sigpu_pass_color(renderer.pass, targets->color, GpuLoadLoad, GpuStoreStore, NULL, 0);
    sigpu_pass_sample(renderer.pass, "atlas", renderer.atlas, renderer.sampler);
}

static void ui_frame(ecs_iter_t *it) {
    (void)it;
    ensure_pass();
    const ecs_entity_t window = siwindow_find(renderer.slot);
    const WindowState *state = window ? ecs_try_get(window, WindowState) : NULL;
    if (!state || state->width <= 0 || state->height <= 0) {
        return;
    }
    renderer.view[0] = (float)state->width;
    renderer.view[1] = (float)state->height;
    siui_layout_update((float)state->width, (float)state->height);
    if (!ui_cache.paint_dirty) {
        return;
    }
    ui_cache.paint_dirty = false;
    if (!publish()) {
        /* Full: a larger atlas, every text drawn again. */
        atlas_reset(renderer.atlas_size * 2);
        if (!publish()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "siui: text does not fit the atlas");
        }
    }
}

static ecs_entity_t ui_program(ecs_entity_t shader, const char *name, const char *vertex,
                               const char *fragment) {
    GpuProgram desc = { .shader = shader, .phase = renderer.phase,
                        .raster = { .blend = GpuBlendAlpha } };
    SDL_strlcpy(desc.vertex, vertex, sizeof(desc.vertex));
    SDL_strlcpy(desc.fragment, fragment, sizeof(desc.fragment));
    const ecs_entity_t program = ecs_new_named(0, name);
    ecs_set_cid(program, ecs_id(GpuProgram), &desc);
    return program;
}

void ui_render_register(const char *window_slot) {
    if (!ecs_module_find(&ecs_id(sigpu))) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "siui: import sigpu before enabling GPU drawing");
        return;
    }
    renderer = (ui_renderer_t){ .enabled = true };
    SDL_strlcpy(renderer.slot, window_slot && window_slot[0] ? window_slot : "main",
                sizeof(renderer.slot));
    ECS_COMPONENT_REGISTER(UiRectDraw, UiTextDraw, UiDrawOrder);
    sigpu_asset_register(UI_PACK, ui_gpushader, sizeof(ui_gpushader));
    renderer.phase = ecs_new_named(0, "siui::phases::ui");
    const ecs_entity_t shader = ecs_new_named(0, "siui::shader");
    ecs_set(shader, GpuShader, { .path = UI_PACK });
    renderer.rect_program = ui_program(shader, "siui::programs::rect", "rect_vertex",
                                       "rect_fragment");
    renderer.text_program = ui_program(shader, "siui::programs::text", "text_vertex",
                                       "text_fragment");
    renderer.sampler = ecs_new_named(0, "siui::sampler");
    ecs_set(renderer.sampler, GpuSampler,
            { .min_filter = GpuFilterLinear, .mag_filter = GpuFilterLinear,
              .address_u = GpuAddressClamp, .address_v = GpuAddressClamp });
    renderer.atlas = ecs_new_named(0, "siui::atlas");
    atlas_reset(UI_ATLAS_SIZE);
    const float corners[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
    const ecs_entity_t vertices = ecs_new_named(0, "siui::quad_vertices");
    ecs_set(vertices, GpuBuffer, { .size = sizeof(corners), .usage = GpuBufferVertex });
    sigpu_buffer_upload(vertices, 0, corners, sizeof(corners));
    renderer.quad = ecs_new_named(0, "siui::quad");
    ecs_set(renderer.quad, GpuGeometry, {
        .vertices = vertices, .stride = sizeof(corners[0]), .lod_count = 1, .attribute_count = 1,
        .attributes = { { .name = "corner", .format = GpuVertexFloat2 } },
        .lods = { { .index_count = 6 } } });
    sigpu_feature(&(GpuFeatureDesc){
        .name = "siui::rects",
        .instance = ecs_id(UiRectDraw),
        .query = { .components = { ecs_in_changed(UiRectDraw), ecs_in_changed(UiDrawOrder) } },
        .extract = extract_rects,
        .geometry = renderer.quad,
        .program = renderer.rect_program,
        .ordering = GpuOrderLayered,
    });
    sigpu_feature(&(GpuFeatureDesc){
        .name = "siui::texts",
        .instance = ecs_id(UiTextDraw),
        .query = { .components = { ecs_in_changed(UiTextDraw), ecs_in_changed(UiDrawOrder) } },
        .extract = extract_texts,
        .geometry = renderer.quad,
        .program = renderer.text_program,
        .ordering = GpuOrderLayered,
    });
    ecs_system({ .name = "UiDraw", .phase = EcsPreRender, .callback = ui_frame,
                 .main_thread_only = true, .no_defer = true });
}

void ui_render_fini(void) {
    SDL_free(renderer.pixels);
    renderer = (ui_renderer_t){ 0 };
}
