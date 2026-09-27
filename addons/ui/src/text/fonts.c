#include "ui_internal.h"
typedef struct { TTF_Font *font; char *path; float size,line_height; siui_font_handle_t owner; } font_slot_t;
static font_slot_t *fonts;
static uint32_t count,capacity;
static siui_font_handle_t default_font;
static uint32_t add_font(TTF_Font *font,const char *path,float size,float line_height,siui_font_handle_t owner) {
    if(!font) return 0;
    if(count==capacity) {
        uint32_t next=capacity?capacity*2:16;
        font_slot_t *p=SDL_realloc(fonts,next*sizeof(*p));
        if(!p) {TTF_CloseFont(font);return 0;}
        fonts=p;capacity=next;
    }
    fonts[count]=(font_slot_t){font,path?SDL_strdup(path):NULL,size,line_height,owner};
    return ++count;
}
siui_font_handle_t siui_font_open(const char *path,float size) {
    if(!path || size<=0) return 0;
    return add_font(TTF_OpenFont(path,size),path,size,1,0);
}
void siui_font_close(siui_font_handle_t handle) {
    if(!handle || handle>count) return;
    if(!fonts[handle-1].font) return;
    for(uint32_t i=0;i<ui_cache.count;i++) {
        ui_entry_t *entry=&ui_cache.entries[i];
        const UiText *style=ecs_try_get(entry->entity,UiText);
        if(entry->font_handle==handle || (default_font==handle && style && style->font==0)) {
            ui_text_dispose(entry);
            entry->flags|=TEXT_DIRTY|MEASURE_DIRTY|PAINT_DIRTY;
            ui_cache.layout_dirty=ui_cache.paint_dirty=true;
        }
    }
    for(uint32_t i=0;i<count;i++) {
        if(i+1==handle || fonts[i].owner==handle) {
            if(fonts[i].font) TTF_CloseFont(fonts[i].font);
            fonts[i].font=NULL;
            SDL_free(fonts[i].path);fonts[i].path=NULL;
        }
    }
    if(default_font==handle) default_font=0;
}
TTF_Font *ui_font(siui_font_handle_t handle) {
    if(!handle) handle=default_font;
    return handle<=count && handle?fonts[handle-1].font:NULL;
}
TTF_Font *ui_font_sized(siui_font_handle_t handle,float size,float line_height) {
    if(!handle) handle=default_font;
    if(!handle || handle>count) return NULL;
    font_slot_t *base=&fonts[handle-1];
    if(!base->font) return NULL;
    if(size<=0) size=base->size;
    if(line_height<=0) line_height=1;
    if(size==base->size && line_height==1) return base->font;
    for(uint32_t i=0;i<count;i++)
        if(fonts[i].owner==handle && fonts[i].font && fonts[i].size==size && fonts[i].line_height==line_height)
            return fonts[i].font;
    const char *path=base->path;
    TTF_Font *copy=TTF_CopyFont(base->font);
    if(!copy || !TTF_SetFontSize(copy,size)) { if(copy) TTF_CloseFont(copy);return NULL; }
    if(line_height!=1)
        TTF_SetFontLineSkip(copy,SDL_max(1,(int)lroundf(TTF_GetFontLineSkip(copy)*line_height)));
    return add_font(copy,path,size,line_height,handle)?copy:NULL;
}
void ui_fonts_init_default(void) {
    static const char *candidates[]={
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "C:\\Windows\\Fonts\\arial.ttf"
    };
    for(uint32_t i=0;i<sizeof(candidates)/sizeof(candidates[0]);i++) {
        default_font=siui_font_open(candidates[i],16);
        if(default_font) break;
    }
}
void ui_fonts_fini(void) {
    for(uint32_t i=0;i<count;i++) { if(fonts[i].font) TTF_CloseFont(fonts[i].font); SDL_free(fonts[i].path); }
    SDL_free(fonts);fonts=NULL;count=capacity=default_font=0;
}
