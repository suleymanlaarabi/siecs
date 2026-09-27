#include "ui_internal.h"
void ui_mark(ecs_entity_t entity, uint32_t flags) {
    if(flags&TREE_DIRTY) { ui_cache.tree_dirty=true;ui_cache.layout_dirty=true; return; }
    uint32_t at=ui_find(entity);
    if(at==UI_NONE) { ui_cache.tree_dirty=true;ui_cache.layout_dirty=true; return; }
    ui_cache.entries[at].flags|=flags;
    if(flags&(MEASURE_DIRTY|ARRANGE_DIRTY|TEXT_DIRTY)) ui_cache.layout_dirty=true;
    if(flags&(MEASURE_DIRTY|TEXT_DIRTY)) {
        for(uint32_t parent=ui_cache.entries[at].parent;parent!=UI_NONE;parent=ui_cache.entries[parent].parent)
            ui_cache.entries[parent].flags|=MEASURE_DIRTY|ARRANGE_DIRTY;
    }
    if(flags&PAINT_DIRTY) ui_cache.paint_dirty=true;
}
