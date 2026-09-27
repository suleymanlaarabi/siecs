#include "ui_internal.h"
#include <sigpu.h>
SIECS_PUBLIC_API ecs_component_t ecs_id(UiNode)=0;
SIECS_PUBLIC_API ecs_component_t ecs_id(UiPaint)=0;
SIECS_PUBLIC_API ecs_component_t ecs_id(UiText)=0;
SIECS_PUBLIC_API ecs_component_desc_t ecs_id(UiNode_desc)={.name="UiNode",.size=sizeof(UiNode),ECS_SET_ONLY};
SIECS_PUBLIC_API ecs_component_desc_t ecs_id(UiPaint_desc)={.name="UiPaint",.size=sizeof(UiPaint),ECS_SET_ONLY};
SIECS_PUBLIC_API ecs_component_desc_t ecs_id(UiText_desc)={.name="UiText",.size=sizeof(UiText),ECS_SET_ONLY};
ECS_MODULE_DEFINE(siui);
ECS_RESOURCE_DEFINE(UiLayoutCache);

static void component_set(ecs_observer_event_t *event) {
    if(event->component==ecs_id(UiNode)) {
        const UiNode *next=event->trigger_data;
        uint32_t at=ui_find(event->entity);
        if(at!=UI_NONE && next && (ui_cache.entries[at].order!=next->order ||
            ui_cache.entries[at].z_index!=next->z_index)) {ui_mark(event->entity,TREE_DIRTY);return;}
        ui_mark(event->entity,MEASURE_DIRTY|ARRANGE_DIRTY);
    } else if(event->component==ecs_id(UiText)) ui_mark(event->entity,TEXT_DIRTY|MEASURE_DIRTY|PAINT_DIRTY);
    else if(event->component==ecs_id(UiPaint)) ui_mark(event->entity,PAINT_DIRTY);
}
static void component_add_remove(ecs_observer_event_t *event) {
    if(event->component==ecs_id(UiNode)) ui_mark(event->entity,TREE_DIRTY);
    else if(event->component==ecs_id(UiText)) {
        if(event->event==EcsOnRemove && event->trigger_data)
            ui_text_release(((const UiText *)event->trigger_data)->text);
        ui_mark(event->entity,TEXT_DIRTY|MEASURE_DIRTY|PAINT_DIRTY);
    }
    else if(event->component==ecs_id(UiPaint)) ui_mark(event->entity,PAINT_DIRTY);
}
static void relation_changed(ecs_observer_event_t *event) {
    const ecs_relation_event_t *change=event->trigger_data;
    if(change && change->relation==ecs_rid(ChildOf)) {ui_cache.tree_dirty=true;ui_cache.layout_dirty=true;}
}
static void fini_ui(void *unused) {
    (void)unused;
    ui_render_fini();
    ui_text_fini();
    SDL_free(ui_cache.entries);SDL_free(ui_cache.walk);SDL_free(ui_cache.paint_walk);SDL_free(ui_cache.sort);
    SDL_free(ui_cache.scratch);SDL_free(ui_cache.main_sizes);SDL_free(ui_cache.cross_sizes);
    SDL_free(ui_cache.line_crosses);SDL_free(ui_cache.line_starts);
    ui_cache=(ui_cache_t){0};
    ui_cache_ptr=NULL;
}
void siui_import(const siui_props_t *props) {
    ecs_component_register(&ecs_id(UiNode),&ecs_id(UiNode_desc));
    ecs_component_register(&ecs_id(UiPaint),&ecs_id(UiPaint_desc));
    ecs_component_register(&ecs_id(UiText),&ecs_id(UiText_desc));
    ECS_RESOURCE_REGISTER(UiLayoutCache);
    ecs_set_resource(UiLayoutCache,{0});
    ui_cache_ptr=ecs_get_resource(UiLayoutCache);
    ui_cache.query=ecs_query({.components={ecs_in(UiNode)}});
    ui_cache.tree_dirty=true;
    ui_cache.layout_dirty=true;
    ui_cache.paint_dirty=true;
    ui_text_init();
    ui_fonts_init_default();
    ecs_observer({.on=EcsOnSet,.callback=component_set});
    ecs_observer({.on=EcsOnAdd,.callback=component_add_remove});
    ecs_observer({.on=EcsOnRemove,.callback=component_add_remove});
    ecs_observer({.on=EcsOnRelationSet,.callback=relation_changed});
    ecs_observer({.on=EcsOnRelationRemove,.callback=relation_changed});
    if(props && props->gpu) {
        if(ecs_module_find(&ecs_id(sigpu))) ui_render_register();
        else SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"siui: import sigpu before enabling GPU rendering");
    }
    ecs_at_fini({.callback=fini_ui});
}
bool siui_layout_update(float viewport_width,float viewport_height) {
    if(!ui_cache_ptr || viewport_width<0 || viewport_height<0) return false;
    ui_cache.stats=(siui_stats_t){.generation=ui_cache.generation};
    if(viewport_width!=ui_cache.viewport_width || viewport_height!=ui_cache.viewport_height) {
        ui_cache.viewport_width=viewport_width;ui_cache.viewport_height=viewport_height;
        ui_cache.layout_dirty=true;
        for(uint32_t i=0;i<ui_cache.count;i++) ui_cache.entries[i].flags|=MEASURE_DIRTY|ARRANGE_DIRTY;
    }
    if(!ui_cache.layout_dirty && !ui_cache.tree_dirty) return false;
    ui_cache.generation++;
    uint64_t start=SDL_GetTicksNS();
    if(ui_cache.tree_dirty) ui_tree_sync();
    ui_cache.stats.tree_sync_ns=SDL_GetTicksNS()-start;
    start=SDL_GetTicksNS();ui_measure();ui_cache.stats.measure_ns=SDL_GetTicksNS()-start;
    start=SDL_GetTicksNS();ui_arrange();ui_cache.stats.arrange_ns=SDL_GetTicksNS()-start;
    ui_cache.layout_dirty=false;
    ui_cache.stats.generation=ui_cache.generation;
    return true;
}
const UiComputedLayout *siui_layout(ecs_entity_t entity) {
    if(!ui_cache_ptr) return NULL;
    uint32_t at=ui_find(entity);
    return at==UI_NONE?NULL:&ui_cache.entries[at].computed;
}
siui_stats_t siui_stats(void) { return ui_cache_ptr?ui_cache.stats:(siui_stats_t){0}; }
