#include "ui_internal.h"
typedef struct { char *bytes; size_t length; } text_slot_t;
static text_slot_t *strings;
static uint32_t string_count,string_capacity;
static TTF_TextEngine *engine;
static bool gpu_engine;
bool ui_text_init(void) {
    if(!TTF_Init()) return false;
    engine=TTF_CreateSurfaceTextEngine();
    return engine!=NULL;
}
TTF_TextEngine *ui_text_engine(void) { return engine; }
void ui_text_gpu_engine(SDL_GPUDevice *device) {
    if(gpu_engine || !device) return;
    TTF_TextEngine *gpu=TTF_CreateGPUTextEngine(device);
    if(!gpu) return;
    uint32_t i=0;
    for(;i<ui_cache.count;i++)
        if(ui_cache.entries[i].ttf_text && !TTF_SetTextEngine(ui_cache.entries[i].ttf_text,gpu)) break;
    if(i<ui_cache.count) {
        for(uint32_t j=0;j<i;j++)
            if(ui_cache.entries[j].ttf_text) TTF_SetTextEngine(ui_cache.entries[j].ttf_text,engine);
        TTF_DestroyGPUTextEngine(gpu);
        return;
    }
    TTF_DestroySurfaceTextEngine(engine);
    engine=gpu;gpu_engine=true;ui_cache.paint_dirty=true;
}
void ui_text_dispose(ui_entry_t *entry) {
    if(entry->ttf_text) {TTF_DestroyText(entry->ttf_text);entry->ttf_text=NULL;}
}
const char *siui_text_string(siui_text_handle_t handle) {
    return handle && handle<=string_count && strings[handle-1].bytes?strings[handle-1].bytes:"";
}
void ui_text_release(siui_text_handle_t handle) {
    if(handle && handle<=string_count) {
        SDL_free(strings[handle-1].bytes);
        strings[handle-1]=(text_slot_t){0};
    }
}
bool siui_set_text_styled(ecs_entity_t entity,const char *utf8,size_t length,const UiText *style) {
    if(!utf8 && length) return false;
    char *copy=SDL_malloc(length+1);
    if(!copy) return false;
    if(length) memcpy(copy,utf8,length);
    copy[length]=0;
    const UiText *current=ecs_try_get(entity,UiText);
    UiText next=current?*current:(UiText){.font_size=16,.line_height=1,.color={255,255,255,255},.wrap=SIUI_TEXT_WRAP};
    if(style) {
        if(style->font) next.font=style->font;
        if(style->font_size>0) next.font_size=style->font_size;
        if(style->line_height>0) next.line_height=style->line_height;
        if(style->color.a) next.color=style->color;
        next.wrap=style->wrap;
        next.horizontal_align=style->horizontal_align;
        next.vertical_align=style->vertical_align;
    }
    if(!next.text) {
        if(string_count==string_capacity) {
            uint32_t capacity=string_capacity?string_capacity*2:32;
            text_slot_t *p=SDL_realloc(strings,capacity*sizeof(*p));
            if(!p) {SDL_free(copy);return false;}
            strings=p;string_capacity=capacity;
        }
        next.text=++string_count;
        strings[next.text-1]=(text_slot_t){0};
    }
    SDL_free(strings[next.text-1].bytes);
    strings[next.text-1]=(text_slot_t){copy,length};
    ecs_set_cid(entity,ecs_id(UiText),&next);
    ui_mark(entity,TEXT_DIRTY|MEASURE_DIRTY|PAINT_DIRTY);
    return true;
}
bool siui_set_text(ecs_entity_t entity,const char *utf8,size_t length) {
    return siui_set_text_styled(entity,utf8,length,NULL);
}
bool ui_text_measure(ui_entry_t *entry,const UiText *style,float width,float *out_w,float *out_h) {
    *out_w=*out_h=0;
    const char *string=siui_text_string(style->text);
    if(!string[0]) {
        ui_text_dispose(entry);
        entry->text_handle=style->text;
        return true;
    }
    TTF_Font *font=ui_font_sized(style->font,style->font_size,style->line_height);
    if(!font || !engine) return false;
    if(!entry->ttf_text || entry->font_handle!=style->font || entry->font_size!=style->font_size ||
       entry->line_height!=style->line_height) {
        ui_text_dispose(entry);
        entry->ttf_text=TTF_CreateText(engine,font,string,0);
        entry->font_handle=style->font;
        entry->font_size=style->font_size;
        entry->line_height=style->line_height;
        entry->text_handle=style->text;
        entry->text_wrap_width=-1;
    } else if(entry->flags&TEXT_DIRTY || entry->text_handle!=style->text) {
        if(!TTF_SetTextString(entry->ttf_text,string,0)) return false;
        entry->text_handle=style->text;
    }
    if(!entry->ttf_text) return false;
    int wrap=width>=0 && style->wrap==SIUI_TEXT_WRAP ? (int)fmaxf(1,ceilf(width)) : 0;
    if(entry->text_wrap_width!=wrap) {
        if(!TTF_SetTextWrapWidth(entry->ttf_text,wrap)) return false;
        entry->text_wrap_width=wrap;
    }
    int w=0,h=0;
    if(!TTF_GetTextSize(entry->ttf_text,&w,&h)) return false;
    *out_w=(float)w;
    *out_h=(float)h;
    return true;
}
float ui_text_min_width(const UiText *style) {
    const char *str=siui_text_string(style->text);
    TTF_Font *font=ui_font_sized(style->font,style->font_size,style->line_height);
    if(!font || !str[0]) return 0;
    int maximum=0;
    const char *at=str;
    while(*at) {
        while(*at==' ' || *at=='\n' || *at=='\t') at++;
        const char *first=at;
        while(*at && *at!=' ' && *at!='\n' && *at!='\t') at++;
        if(at>first) {
            int width=0;
            if(TTF_GetStringSize(font,first,(size_t)(at-first),&width,NULL))
                maximum=SDL_max(maximum,width);
        }
    }
    return (float)maximum;
}
void ui_text_fini(void) {
    for(uint32_t i=0;i<ui_cache.count;i++) ui_text_dispose(&ui_cache.entries[i]);
    for(uint32_t i=0;i<string_count;i++) SDL_free(strings[i].bytes);
    SDL_free(strings);strings=NULL;string_count=string_capacity=0;
    if(engine) {
        if(gpu_engine) TTF_DestroyGPUTextEngine(engine);
        else TTF_DestroySurfaceTextEngine(engine);
        engine=NULL;gpu_engine=false;
    }
    ui_fonts_fini();
    TTF_Quit();
}
