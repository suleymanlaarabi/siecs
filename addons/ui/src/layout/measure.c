#include "ui_internal.h"
float ui_resolve(siui_length_t value,float reference,float fallback) {
    if(value.unit==SIUI_PX) return value.value;
    if(value.unit==SIUI_PERCENT && reference>=0) return value.value*reference*0.01f;
    return fallback;
}
float ui_edge(siui_length_t value,float reference) {
    return fmaxf(0,ui_resolve(value,reference,0));
}
float ui_clamp(float value,siui_length_t minimum,siui_length_t maximum,float reference) {
    float min=ui_resolve(minimum,reference,0);
    float max=ui_resolve(maximum,reference,INFINITY);
    return fmaxf(min,fminf(value,max));
}
void ui_measure(void) {
    for(uint32_t walk=ui_cache.count;walk>0;walk--) {
        ui_entry_t *entry=&ui_cache.entries[ui_cache.walk[walk-1]];
        if(!(entry->flags&(MEASURE_DIRTY|TEXT_DIRTY))) continue;
        const UiNode *node=ecs_get(entry->entity,UiNode);
        if(!node) continue;
        float content_w=0,content_h=0,min_w=0;
        float gap=node->direction==SIUI_ROW ? (node->column_gap?node->column_gap:node->gap)
                                             : (node->row_gap?node->row_gap:node->gap);
        uint32_t flow_count=0;
        for(uint32_t child=entry->first_child;child!=UI_NONE;child=ui_cache.entries[child].next_sibling) {
            ui_entry_t *ce=&ui_cache.entries[child];
            const UiNode *cn=ecs_get(ce->entity,UiNode);
            if(!cn || cn->position==SIUI_ABSOLUTE) continue;
            float ml=ui_edge(cn->margin.left,-1),mr=ui_edge(cn->margin.right,-1);
            float mt=ui_edge(cn->margin.top,-1),mb=ui_edge(cn->margin.bottom,-1);
            float cw=ce->intrinsic_width+ml+mr,ch=ce->intrinsic_height+mt+mb;
            if(node->direction==SIUI_ROW) {
                content_w+=cw; content_h=fmaxf(content_h,ch);
                if(node->wrap==SIUI_WRAP) min_w=fmaxf(min_w,ce->intrinsic_min_width+ml+mr);
                else min_w+=ce->intrinsic_min_width+ml+mr;
            } else {
                content_w=fmaxf(content_w,cw); content_h+=ch;
                min_w=fmaxf(min_w,ce->intrinsic_min_width+ml+mr);
            }
            flow_count++;
        }
        if(flow_count>1) {
            if(node->direction==SIUI_ROW) {
                content_w+=gap*(flow_count-1);
                if(node->wrap==SIUI_NOWRAP) min_w+=gap*(flow_count-1);
            }
            else content_h+=gap*(flow_count-1);
        }
        const UiText *style=ecs_try_get(entry->entity,UiText);
        if(style) {
            float tw=0,th=0;
            ui_text_measure(entry,style,-1,&tw,&th);
            content_w=fmaxf(content_w,tw);
            content_h=fmaxf(content_h,th);
            min_w=fmaxf(min_w,ui_text_min_width(style));
        }
        float horizontal=ui_edge(node->padding.left,-1)+ui_edge(node->padding.right,-1)
                        +ui_edge(node->border_width.left,-1)+ui_edge(node->border_width.right,-1);
        float vertical=ui_edge(node->padding.top,-1)+ui_edge(node->padding.bottom,-1)
                      +ui_edge(node->border_width.top,-1)+ui_edge(node->border_width.bottom,-1);
        float w=ui_resolve(node->width,-1,content_w+horizontal);
        float h=ui_resolve(node->height,-1,content_h+vertical);
        if(node->aspect_ratio>0) {
            if(node->width.unit!=SIUI_AUTO && node->height.unit==SIUI_AUTO) h=w/node->aspect_ratio;
            if(node->height.unit!=SIUI_AUTO && node->width.unit==SIUI_AUTO) w=h*node->aspect_ratio;
        }
        w=ui_clamp(w,node->min_width,node->max_width,-1);
        float wrap_width=w-horizontal;
        if(node->max_width.unit==SIUI_PX) wrap_width=fminf(wrap_width,node->max_width.value-horizontal);
        if(style && style->wrap==SIUI_TEXT_WRAP && wrap_width>=0) {
            float tw=0,th=0;
            ui_text_measure(entry,style,wrap_width,&tw,&th);
            content_h=fmaxf(content_h,th);
            if(node->height.unit==SIUI_AUTO) h=content_h+vertical;
        }
        if(node->wrap==SIUI_WRAP && flow_count) {
            bool row=node->direction==SIUI_ROW;
            float available=fmaxf(0,(row?w-horizontal:h-vertical));
            float used=0,cross=0,total=0;
            float cross_gap=row?(node->row_gap?node->row_gap:node->gap)
                               :(node->column_gap?node->column_gap:node->gap);
            for(uint32_t child=entry->first_child;child!=UI_NONE;child=ui_cache.entries[child].next_sibling) {
                ui_entry_t *ce=&ui_cache.entries[child];
                const UiNode *cn=ecs_get(ce->entity,UiNode);
                if(!cn || cn->position==SIUI_ABSOLUTE) continue;
                float cw=ce->intrinsic_width+ui_edge(cn->margin.left,-1)+ui_edge(cn->margin.right,-1);
                float ch=ce->intrinsic_height+ui_edge(cn->margin.top,-1)+ui_edge(cn->margin.bottom,-1);
                float main=row?cw:ch, other=row?ch:cw;
                float needed=main+(used>0?gap:0);
                if(used>0 && used+needed>available) {
                    total+=cross+cross_gap;used=0;cross=0;needed=main;
                }
                used+=needed;cross=fmaxf(cross,other);
            }
            total+=cross;
            if(row) {content_h=fmaxf(content_h,total);if(node->height.unit==SIUI_AUTO) h=content_h+vertical;}
            else {content_w=fmaxf(content_w,total);if(node->width.unit==SIUI_AUTO) w=ui_clamp(content_w+horizontal,node->min_width,node->max_width,-1);}
        }
        h=ui_clamp(h,node->min_height,node->max_height,-1);
        if(w!=entry->intrinsic_width || h!=entry->intrinsic_height) {
            entry->flags|=ARRANGE_DIRTY;
            if(entry->parent!=UI_NONE) ui_cache.entries[entry->parent].flags|=ARRANGE_DIRTY;
        }
        entry->intrinsic_min_width=fmaxf(0,min_w+horizontal);
        entry->intrinsic_width=fmaxf(0,w);
        entry->intrinsic_height=fmaxf(0,h);
        entry->flags&=~(MEASURE_DIRTY|TEXT_DIRTY);
        ui_cache.stats.measured_nodes++;
    }
}
