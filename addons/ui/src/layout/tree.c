#include "ui_internal.h"
ui_cache_t *ui_cache_ptr;
static int compare_entities(const void *a, const void *b) {
    const ui_entry_t *left=a, *right=b;
    return (left->entity > right->entity) - (left->entity < right->entity);
}
uint32_t ui_find(ecs_entity_t entity) {
    uint32_t lo=0, hi=ui_cache.count;
    while (lo<hi) {
        uint32_t mid=lo+(hi-lo)/2;
        if (ui_cache.entries[mid].entity < entity) lo=mid+1;
        else hi=mid;
    }
    return lo<ui_cache.count && ui_cache.entries[lo].entity==entity ? lo : UI_NONE;
}
static int compare_children(const void *a, const void *b) {
    uint32_t ia=*(const uint32_t *)a, ib=*(const uint32_t *)b;
    const ui_entry_t *ea=&ui_cache.entries[ia], *eb=&ui_cache.entries[ib];
    if (ea->parent!=eb->parent) return (ea->parent>eb->parent)-(ea->parent<eb->parent);
    const UiNode *na=ecs_get(ea->entity,UiNode), *nb=ecs_get(eb->entity,UiNode);
    int16_t oa=na?na->order:0, ob=nb?nb->order:0;
    if (oa!=ob) return (oa>ob)-(oa<ob);
    return (ea->entity>eb->entity)-(ea->entity<eb->entity);
}
static int compare_paint(const void *a,const void *b) {
    uint32_t ia=*(const uint32_t *)a,ib=*(const uint32_t *)b;
    const ui_entry_t *ea=&ui_cache.entries[ia],*eb=&ui_cache.entries[ib];
    if(ea->parent!=eb->parent) return (ea->parent>eb->parent)-(ea->parent<eb->parent);
    const UiNode *na=ecs_get(ea->entity,UiNode),*nb=ecs_get(eb->entity,UiNode);
    int16_t za=na?na->z_index:0,zb=nb?nb->z_index:0;
    if(za!=zb) return (za>zb)-(za<zb);
    if(ea->order!=eb->order) return (ea->order>eb->order)-(ea->order<eb->order);
    return (ea->entity>eb->entity)-(ea->entity<eb->entity);
}
static void reserve(uint32_t size) {
    if (size<=ui_cache.capacity) return;
    uint32_t capacity=ui_cache.capacity?ui_cache.capacity:32;
    while(capacity<size) capacity*=2;
    ui_cache.entries=SDL_realloc(ui_cache.entries,capacity*sizeof(ui_entry_t));
    ui_cache.walk=SDL_realloc(ui_cache.walk,capacity*sizeof(uint32_t));
    ui_cache.paint_walk=SDL_realloc(ui_cache.paint_walk,capacity*sizeof(uint32_t));
    ui_cache.sort=SDL_realloc(ui_cache.sort,capacity*sizeof(uint32_t));
    ui_cache.scratch=SDL_realloc(ui_cache.scratch,capacity*sizeof(uint32_t));
    ui_cache.main_sizes=SDL_realloc(ui_cache.main_sizes,capacity*sizeof(float));
    ui_cache.cross_sizes=SDL_realloc(ui_cache.cross_sizes,capacity*sizeof(float));
    ui_cache.line_crosses=SDL_realloc(ui_cache.line_crosses,capacity*sizeof(float));
    ui_cache.line_starts=SDL_realloc(ui_cache.line_starts,capacity*sizeof(uint32_t));
    ui_cache.capacity=capacity;
}
void ui_tree_sync(void) {
    uint32_t count=0;
    for (ecs_iter_t it=ecs_query_iter(ui_cache.query);ecs_iter_next(&it);) count+=it.count;
    ui_entry_t *old=ui_cache.entries;
    uint32_t old_count=ui_cache.count;
    ui_cache.entries=NULL;
    ui_cache.count=0;
    ui_cache.capacity=0;
    /* Keep cached SDL text objects while rebuilding the flat array. */
    reserve(count);
    for (ecs_iter_t it=ecs_query_iter(ui_cache.query);ecs_iter_next(&it);)
        for(uint32_t i=0;i<it.count;i++) ui_cache.entries[ui_cache.count++]=(ui_entry_t){.entity=it.entities[i]};
    qsort(ui_cache.entries,count,sizeof(ui_entry_t),compare_entities);
    for(uint32_t i=0;i<count;i++) {
        ui_entry_t *entry=&ui_cache.entries[i];
        uint32_t lo=0,hi=old_count;
        while(lo<hi) { uint32_t mid=lo+(hi-lo)/2; if(old[mid].entity<entry->entity) lo=mid+1; else hi=mid; }
        if(lo<old_count && old[lo].entity==entry->entity) {
            entry->ttf_text=old[lo].ttf_text;
            entry->text_handle=old[lo].text_handle;
            entry->font_handle=old[lo].font_handle;
            entry->font_size=old[lo].font_size;
            entry->line_height=old[lo].line_height;
            old[lo].ttf_text=NULL;
        }
        entry->parent=ui_find(ecs_target(entry->entity,ChildOf));
        const UiNode *node=ecs_get(entry->entity,UiNode);
        entry->order=node?node->order:0;
        entry->z_index=node?node->z_index:0;
        entry->first_child=entry->next_sibling=UI_NONE;
        entry->paint_first_child=entry->paint_next_sibling=UI_NONE;
        entry->flags=MEASURE_DIRTY|ARRANGE_DIRTY|TEXT_DIRTY|PAINT_DIRTY;
        ui_cache.sort[i]=i;
    }
    for(uint32_t i=0;i<old_count;i++) ui_text_dispose(&old[i]);
    SDL_free(old);
    qsort(ui_cache.sort,count,sizeof(uint32_t),compare_children);
    ui_cache.roots=UI_NONE;
    uint32_t previous=UI_NONE, previous_parent=UI_NONE;
    for(uint32_t i=0;i<count;i++) {
        uint32_t at=ui_cache.sort[i], parent=ui_cache.entries[at].parent;
        if(parent!=previous_parent || i==0) {
            if(parent==UI_NONE) ui_cache.roots=at;
            else ui_cache.entries[parent].first_child=at;
        } else ui_cache.entries[previous].next_sibling=at;
        previous=at; previous_parent=parent;
    }
    /* Iterative preorder, with no call stack growth for deep UI trees. */
    uint32_t at=ui_cache.roots, walk_count=0;
    while(at!=UI_NONE) {
        ui_cache.walk[walk_count++]=at;
        if(ui_cache.entries[at].first_child!=UI_NONE) at=ui_cache.entries[at].first_child;
        else {
            while(at!=UI_NONE && ui_cache.entries[at].next_sibling==UI_NONE) at=ui_cache.entries[at].parent;
            if(at!=UI_NONE) at=ui_cache.entries[at].next_sibling;
        }
    }
    qsort(ui_cache.sort,count,sizeof(uint32_t),compare_paint);
    uint32_t paint_roots=UI_NONE;
    previous=previous_parent=UI_NONE;
    for(uint32_t i=0;i<count;i++) {
        uint32_t index=ui_cache.sort[i],parent=ui_cache.entries[index].parent;
        if(parent!=previous_parent || i==0) {
            if(parent==UI_NONE) paint_roots=index;
            else ui_cache.entries[parent].paint_first_child=index;
        } else ui_cache.entries[previous].paint_next_sibling=index;
        previous=index;previous_parent=parent;
    }
    at=paint_roots;walk_count=0;
    while(at!=UI_NONE) {
        ui_cache.paint_walk[walk_count++]=at;
        if(ui_cache.entries[at].paint_first_child!=UI_NONE) at=ui_cache.entries[at].paint_first_child;
        else {
            while(at!=UI_NONE && ui_cache.entries[at].paint_next_sibling==UI_NONE) at=ui_cache.entries[at].parent;
            if(at!=UI_NONE) at=ui_cache.entries[at].paint_next_sibling;
        }
    }
    ui_cache.tree_dirty=false;
    ui_cache.paint_dirty=true;
}
