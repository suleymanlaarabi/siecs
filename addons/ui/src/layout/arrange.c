#include "ui_internal.h"

static float main_before(const UiNode *n, bool row, float ref) {
    return ui_edge(row ? n->margin.left : n->margin.top, ref);
}

static float main_after(const UiNode *n, bool row, float ref) {
    return ui_edge(row ? n->margin.right : n->margin.bottom, ref);
}

static float cross_before(const UiNode *n, bool row, float ref) {
    return ui_edge(row ? n->margin.top : n->margin.left, ref);
}

static float cross_after(const UiNode *n, bool row, float ref) {
    return ui_edge(row ? n->margin.bottom : n->margin.right, ref);
}

static void set_rect(ui_entry_t *e, float x, float y, float w, float h) {
    UiComputedLayout *c = &e->computed;
    if (c->x != x || c->y != y || c->width != w || c->height != h) {
        c->x = x;
        c->y = y;
        c->width = fmaxf(0, w);
        c->height = fmaxf(0, h);
        e->flags |= ARRANGE_DIRTY;
        ui_cache.paint_dirty = true;
    }
}

static bool content_rect(ui_entry_t *e, const UiNode *n, float ref) {
    UiComputedLayout *c = &e->computed;
    UiComputedLayout old = *c;
    float l = ui_edge(n->border_width.left, ref) + ui_edge(n->padding.left, ref);
    float r = ui_edge(n->border_width.right, ref) + ui_edge(n->padding.right, ref);
    float t = ui_edge(n->border_width.top, ref) + ui_edge(n->padding.top, ref);
    float b = ui_edge(n->border_width.bottom, ref) + ui_edge(n->padding.bottom, ref);
    c->content_x = c->x + l;
    c->content_y = c->y + t;
    c->content_width = fmaxf(0, c->width - l - r);
    c->content_height = fmaxf(0, c->height - t - b);
    float x0 = 0, y0 = 0, x1 = ui_cache.viewport_width, y1 = ui_cache.viewport_height;
    if (e->parent != UI_NONE) {
        const UiComputedLayout *pc = &ui_cache.entries[e->parent].computed;
        x0 = pc->clip_x;
        y0 = pc->clip_y;
        x1 = x0 + pc->clip_width;
        y1 = y0 + pc->clip_height;
    }
    if (n->overflow == SIUI_HIDDEN) {
        x0 = fmaxf(x0, c->content_x);
        y0 = fmaxf(y0, c->content_y);
        x1 = fminf(x1, c->content_x + c->content_width);
        y1 = fminf(y1, c->content_y + c->content_height);
    }
    c->clip_x = x0;
    c->clip_y = y0;
    c->clip_width = fmaxf(0, x1 - x0);
    c->clip_height = fmaxf(0, y1 - y0);
    bool clip_changed = old.clip_x != c->clip_x || old.clip_y != c->clip_y ||
                        old.clip_width != c->clip_width || old.clip_height != c->clip_height;
    if (clip_changed || old.content_x != c->content_x || old.content_y != c->content_y ||
        old.content_width != c->content_width || old.content_height != c->content_height) {
        ui_cache.paint_dirty = true;
    }
    return clip_changed;
}

static void arrange_absolute(ui_entry_t *parent, uint32_t child) {
    ui_entry_t *ce = &ui_cache.entries[child];
    const UiNode *n = &ce->node;
    const UiComputedLayout *p = &parent->computed;
    float l = ui_resolve(n->left, p->content_width, NAN),
          r = ui_resolve(n->right, p->content_width, NAN);
    float t = ui_resolve(n->top, p->content_height, NAN),
          b = ui_resolve(n->bottom, p->content_height, NAN);
    float w = ui_resolve(n->width, p->content_width, ce->intrinsic_width);
    float h = ui_resolve(n->height, p->content_height, ce->intrinsic_height);
    if (isfinite(l) && isfinite(r) && n->width.unit == SIUI_AUTO)
        w = p->content_width - l - r;
    if (isfinite(t) && isfinite(b) && n->height.unit == SIUI_AUTO)
        h = p->content_height - t - b;
    w = ui_clamp(w, n->min_width, n->max_width, p->content_width);
    h = ui_clamp(h, n->min_height, n->max_height, p->content_height);
    float x = isfinite(l)   ? p->content_x + l
              : isfinite(r) ? p->content_x + p->content_width - r - w
                            : p->content_x;
    float y = isfinite(t)   ? p->content_y + t
              : isfinite(b) ? p->content_y + p->content_height - b - h
                            : p->content_y;
    set_rect(ce, x, y, w, h);
}

static void arrange_children(ui_entry_t *parent, const UiNode *node) {
    const UiComputedLayout *p = &parent->computed;
    const bool row = node->direction == SIUI_ROW;
    const float main_extent = row ? p->content_width : p->content_height;
    const float cross_extent = row ? p->content_height : p->content_width;
    const float main_gap = row ? (node->column_gap ? node->column_gap : node->gap)
                               : (node->row_gap ? node->row_gap : node->gap);
    const float cross_gap = row ? (node->row_gap ? node->row_gap : node->gap)
                                : (node->column_gap ? node->column_gap : node->gap);
    uint32_t count = 0;
    for (uint32_t child = parent->first_child; child != UI_NONE;
         child = ui_cache.entries[child].next_sibling) {
        const UiNode *n = &ui_cache.entries[child].node;
        if (n->position == SIUI_ABSOLUTE) {
            arrange_absolute(parent, child);
            continue;
        }
        ui_cache.scratch[count++] = child;
        ui_entry_t *ce = &ui_cache.entries[child];
        siui_length_t main_length = row ? n->width : n->height;
        siui_length_t cross_length = row ? n->height : n->width;
        float main =
            ui_resolve(main_length, main_extent, row ? ce->intrinsic_width : ce->intrinsic_height);
        if (n->flex_basis.unit != SIUI_AUTO)
            main = ui_resolve(n->flex_basis, main_extent, main);
        float cross = ui_resolve(
            cross_length,
            cross_extent,
            row ? ce->intrinsic_height : ce->intrinsic_width
        );
        ui_cache.main_sizes[child] = fmaxf(0, main);
        ui_cache.cross_sizes[child] = fmaxf(0, cross);
    }
    if (!count)
        return;
    uint32_t lines = 0, start = 0;
    while (start < count) {
        uint32_t end = start;
        float used = 0, line_cross = 0;
        while (end < count) {
            uint32_t child = ui_cache.scratch[end];
            const UiNode *n = &ui_cache.entries[child].node;
            float wanted = ui_cache.main_sizes[child] + main_before(n, row, main_extent) +
                           main_after(n, row, main_extent);
            if (end > start)
                wanted += main_gap;
            if (node->wrap == SIUI_WRAP && end > start && used + wanted > main_extent)
                break;
            used += wanted;
            line_cross = fmaxf(
                line_cross,
                ui_cache.cross_sizes[child] + cross_before(n, row, cross_extent) +
                    cross_after(n, row, cross_extent)
            );
            end++;
        }
        ui_cache.line_starts[lines] = start;
        ui_cache.line_crosses[lines] = line_cross;
        lines++;
        start = end;
    }
    if (lines == 1 && node->wrap == SIUI_NOWRAP)
        ui_cache.line_crosses[0] = fmaxf(ui_cache.line_crosses[0], cross_extent);
    float total_cross = cross_gap * (lines - 1);
    for (uint32_t line = 0; line < lines; line++)
        total_cross += ui_cache.line_crosses[line];
    float extra_cross = fmaxf(0, cross_extent - total_cross);
    float cross_cursor = 0;
    switch (node->align_content) {
    case SIUI_ALIGN_CENTER:
        cross_cursor = extra_cross * 0.5f;
        break;
    case SIUI_ALIGN_END:
        cross_cursor = extra_cross;
        break;
    case SIUI_STRETCH:
        for (uint32_t i = 0; i < lines; i++)
            ui_cache.line_crosses[i] += extra_cross / lines;
        break;
    default:
        break;
    }
    for (uint32_t line = 0; line < lines; line++) {
        uint32_t begin = ui_cache.line_starts[line];
        uint32_t end = line + 1 < lines ? ui_cache.line_starts[line + 1] : count;
        float used = main_gap * (end - begin - 1), grow = 0, shrink = 0;
        for (uint32_t i = begin; i < end; i++) {
            uint32_t child = ui_cache.scratch[i];
            const UiNode *n = &ui_cache.entries[child].node;
            used += ui_cache.main_sizes[child] + main_before(n, row, main_extent) +
                    main_after(n, row, main_extent);
            grow += fmaxf(0, n->flex_grow);
            shrink += fmaxf(0, n->flex_shrink) * ui_cache.main_sizes[child];
        }
        float free_space = main_extent - used;
        for (uint32_t i = begin; i < end; i++) {
            uint32_t child = ui_cache.scratch[i];
            const UiNode *n = &ui_cache.entries[child].node;
            float value = ui_cache.main_sizes[child];
            if (free_space > 0 && grow > 0)
                value += free_space * fmaxf(0, n->flex_grow) / grow;
            else if (free_space < 0 && shrink > 0)
                value += free_space * fmaxf(0, n->flex_shrink) * value / shrink;
            siui_length_t min = row ? n->min_width : n->min_height,
                          max = row ? n->max_width : n->max_height;
            ui_cache.main_sizes[child] = ui_clamp(fmaxf(0, value), min, max, main_extent);
        }
        used = main_gap * (end - begin - 1);
        for (uint32_t i = begin; i < end; i++) {
            uint32_t child = ui_cache.scratch[i];
            const UiNode *n = &ui_cache.entries[child].node;
            used += ui_cache.main_sizes[child] + main_before(n, row, main_extent) +
                    main_after(n, row, main_extent);
        }
        float remain = fmaxf(0, main_extent - used), main_cursor = 0, gap = main_gap;
        switch (node->justify) {
        case SIUI_CENTER:
            main_cursor = remain * 0.5f;
            break;
        case SIUI_END:
            main_cursor = remain;
            break;
        case SIUI_SPACE_BETWEEN:
            if (end - begin > 1)
                gap += remain / (end - begin - 1);
            break;
        case SIUI_SPACE_AROUND: {
            float extra = remain / (end - begin);
            gap += extra;
            main_cursor = extra * 0.5f;
            break;
        }
        case SIUI_SPACE_EVENLY: {
            float extra = remain / (end - begin + 1);
            gap += extra;
            main_cursor = extra;
            break;
        }
        default:
            break;
        }
        for (uint32_t i = begin; i < end; i++) {
            uint32_t child = ui_cache.scratch[i];
            ui_entry_t *ce = &ui_cache.entries[child];
            const UiNode *n = &ce->node;
            main_cursor += main_before(n, row, main_extent);
            float cross = ui_cache.cross_sizes[child];
            siui_align_t align = n->align_self ? n->align_self : node->align_items;
            siui_length_t cross_length = row ? n->height : n->width;
            float before = cross_before(n, row, cross_extent),
                  after = cross_after(n, row, cross_extent);
            if (align == SIUI_STRETCH && cross_length.unit == SIUI_AUTO)
                cross = fmaxf(0, ui_cache.line_crosses[line] - before - after);
            float cross_free = fmaxf(0, ui_cache.line_crosses[line] - cross - before - after);
            float offset = align == SIUI_ALIGN_CENTER ? cross_free * 0.5f
                           : align == SIUI_ALIGN_END  ? cross_free
                                                      : 0;
            float x =
                row ? p->content_x + main_cursor : p->content_x + cross_cursor + before + offset;
            float y =
                row ? p->content_y + cross_cursor + before + offset : p->content_y + main_cursor;
            float w = row ? ui_cache.main_sizes[child] : cross;
            float h = row ? cross : ui_cache.main_sizes[child];
            w = ui_clamp(w, n->min_width, n->max_width, p->content_width);
            h = ui_clamp(h, n->min_height, n->max_height, p->content_height);
            set_rect(ce, x, y, w, h);
            main_cursor += ui_cache.main_sizes[child] + main_after(n, row, main_extent) + gap;
        }
        cross_cursor += ui_cache.line_crosses[line] + cross_gap;
    }
}

void ui_arrange(void) {
    for (uint32_t i = 0; i < ui_cache.count; i++) {
        ui_entry_t *e = &ui_cache.entries[ui_cache.walk[i]];
        const UiNode *n = &e->node;
        if (e->parent == UI_NONE) {
            float w = ui_resolve(n->width, ui_cache.viewport_width, e->intrinsic_width);
            float h = ui_resolve(n->height, ui_cache.viewport_height, e->intrinsic_height);
            set_rect(
                e,
                0,
                0,
                ui_clamp(w, n->min_width, n->max_width, ui_cache.viewport_width),
                ui_clamp(h, n->min_height, n->max_height, ui_cache.viewport_height)
            );
        }
        if (!(e->flags & ARRANGE_DIRTY))
            continue;
        bool clip_changed = content_rect(
            e,
            n,
            e->parent == UI_NONE ? ui_cache.viewport_width
                                 : ui_cache.entries[e->parent].computed.content_width
        );
        if (clip_changed)
            for (uint32_t child = e->first_child; child != UI_NONE;
                 child = ui_cache.entries[child].next_sibling)
                ui_cache.entries[child].flags |= ARRANGE_DIRTY;
        arrange_children(e, n);
        e->flags &= ~ARRANGE_DIRTY;
        ui_cache.stats.arranged_nodes++;
    }
}
