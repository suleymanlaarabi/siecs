#include "ui_internal.h"

static TTF_TextEngine *engine;

static bool gpu_engine;

bool ui_text_init(void) {
    if (!TTF_Init())
        return false;
    engine = TTF_CreateSurfaceTextEngine();
    return engine != NULL;
}

TTF_TextEngine *ui_text_engine(void) { return engine; }

void ui_text_gpu_engine(SDL_GPUDevice *device) {
    if (gpu_engine)
        return;
    TTF_TextEngine *gpu = TTF_CreateGPUTextEngine(device);
    if (!gpu)
        return;
    uint32_t i = 0;
    for (; i < ui_cache.count; i++)
        if (ui_cache.entries[i].ttf_text && !TTF_SetTextEngine(ui_cache.entries[i].ttf_text, gpu))
            break;
    if (i < ui_cache.count) {
        for (uint32_t j = 0; j < i; j++)
            if (ui_cache.entries[j].ttf_text)
                TTF_SetTextEngine(ui_cache.entries[j].ttf_text, engine);
        TTF_DestroyGPUTextEngine(gpu);
        return;
    }
    TTF_DestroySurfaceTextEngine(engine);
    engine = gpu;
    gpu_engine = true;
    ui_cache.paint_dirty = true;
}

void ui_text_dispose(ui_entry_t *entry) {
    if (entry->ttf_text) {
        TTF_DestroyText(entry->ttf_text);
        entry->ttf_text = NULL;
    }
}

bool ui_text_measure(
    ui_entry_t *entry,
    const UiText *style,
    float width,
    float *out_w,
    float *out_h
) {
    *out_w = *out_h = 0;
    const char *string = style->text ? style->text : "";
    if (!string[0]) {
        ui_text_dispose(entry);
        entry->text_value = string;
        return true;
    }
    TTF_Font *font = ui_font_sized(style->font, style->font_size, style->line_height);
    if (!font || !engine)
        return false;
    if (!entry->ttf_text || entry->font_handle != style->font ||
        entry->font_size != style->font_size || entry->line_height != style->line_height) {
        ui_text_dispose(entry);
        entry->ttf_text = TTF_CreateText(engine, font, string, 0);
        entry->font_handle = style->font;
        entry->font_size = style->font_size;
        entry->line_height = style->line_height;
        entry->text_value = string;
        entry->text_wrap_width = -1;
    } else if (entry->flags & TEXT_DIRTY || entry->text_value != string) {
        if (!TTF_SetTextString(entry->ttf_text, string, 0))
            return false;
        entry->text_value = string;
    }
    if (!entry->ttf_text)
        return false;
    int wrap = width >= 0 && style->wrap == SIUI_TEXT_WRAP ? (int)fmaxf(1, ceilf(width)) : 0;
    if (entry->text_wrap_width != wrap) {
        if (!TTF_SetTextWrapWidth(entry->ttf_text, wrap))
            return false;
        entry->text_wrap_width = wrap;
    }
    int w = 0, h = 0;
    if (!TTF_GetTextSize(entry->ttf_text, &w, &h))
        return false;
    *out_w = (float)w;
    *out_h = (float)h;
    return true;
}

float ui_text_min_width(const UiText *style) {
    const char *str = style->text ? style->text : "";
    TTF_Font *font = ui_font_sized(style->font, style->font_size, style->line_height);
    if (!font || !str[0])
        return 0;
    int maximum = 0;
    const char *at = str;
    while (*at) {
        while (*at == ' ' || *at == '\n' || *at == '\t')
            at++;
        const char *first = at;
        while (*at && *at != ' ' && *at != '\n' && *at != '\t')
            at++;
        if (at > first) {
            int width = 0;
            if (TTF_GetStringSize(font, first, (size_t)(at - first), &width, NULL))
                maximum = SDL_max(maximum, width);
        }
    }
    return (float)maximum;
}

void ui_text_fini(void) {
    for (uint32_t i = 0; i < ui_cache.count; i++)
        ui_text_dispose(&ui_cache.entries[i]);
    if (engine) {
        if (gpu_engine)
            TTF_DestroyGPUTextEngine(engine);
        else
            TTF_DestroySurfaceTextEngine(engine);
        engine = NULL;
        gpu_engine = false;
    }
    ui_fonts_fini();
    TTF_Quit();
}
