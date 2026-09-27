#include "ui_internal.h"
#include <sigpu.h>
ECS_COMPONENT_DEFINE(UiNode, ECS_SET_ONLY);
ECS_COMPONENT_DEFINE(UiPaint, ECS_SET_ONLY);
ECS_COMPONENT_DEFINE(UiText, ECS_SET_ONLY);
ECS_MODULE_DEFINE(siui);
ECS_RESOURCE_DEFINE_UNREFLECTED(UiLayoutCache);

static void ui_reflect_types(void) {
    /* Register field types before the component structs that refer to them. */
    sireflect(siui_unit_t);
    sireflect(siui_direction_t);
    sireflect(siui_wrap_t);
    sireflect(siui_justify_t);
    sireflect(siui_align_t);
    sireflect(siui_position_t);
    sireflect(siui_overflow_t);
    sireflect(siui_text_wrap_t);
    sireflect(siui_text_halign_t);
    sireflect(siui_text_valign_t);
    sireflect(siui_length_t);
    sireflect(siui_edges_t);
    sireflect(siui_color_t);
}

static void component_set(ecs_observer_event_t *event) {
    if (event->component == ecs_id(UiNode)) {
        const UiNode *next = event->trigger_data;
        uint32_t at = ui_find(event->entity);
        if (at == UI_NONE) {
            ui_mark_at(at, TREE_DIRTY);
            return;
        }
        UiNode *old = &ui_cache.entries[at].node;
        bool reorder = old->order != next->order || old->z_index != next->z_index;
        *old = *next;
        ui_mark_at(at, reorder ? TREE_DIRTY : MEASURE_DIRTY | ARRANGE_DIRTY);
    } else if (event->component == ecs_id(UiText))
        ui_mark(event->entity, TEXT_DIRTY | MEASURE_DIRTY | PAINT_DIRTY);
    else if (event->component == ecs_id(UiPaint))
        ui_mark(event->entity, PAINT_DIRTY);
}

static void component_add_remove(ecs_observer_event_t *event) {
    if (event->component == ecs_id(UiNode))
        ui_mark(event->entity, TREE_DIRTY);
    else if (event->component == ecs_id(UiText))
        ui_mark(event->entity, TEXT_DIRTY | MEASURE_DIRTY | PAINT_DIRTY);
    else if (event->component == ecs_id(UiPaint))
        ui_mark(event->entity, PAINT_DIRTY);
}

static void relation_changed(ecs_observer_event_t *event) {
    const ecs_relation_event_t *change = event->trigger_data;
    if (change->relation == ecs_rid(ChildOf)) {
        ui_cache.tree_dirty = true;
        ui_cache.layout_dirty = true;
    }
}

static void fini_ui(void *unused) {
    (void)unused;
    ui_render_fini();
    ui_text_fini();
    SDL_free(ui_cache.entries);
    SDL_free(ui_cache.walk);
    SDL_free(ui_cache.paint_walk);
    SDL_free(ui_cache.sort);
    SDL_free(ui_cache.scratch);
    SDL_free(ui_cache.main_sizes);
    SDL_free(ui_cache.cross_sizes);
    SDL_free(ui_cache.line_crosses);
    SDL_free(ui_cache.line_starts);
    ui_cache = (ui_cache_t){ 0 };
    ui_cache_ptr = NULL;
}

void siui_import(const siui_props_t *props) {
    ui_reflect_types();
    ecs_component_register(&ecs_id(UiNode), &ecs_id(UiNode_desc));
    ecs_component_register(&ecs_id(UiPaint), &ecs_id(UiPaint_desc));
    ecs_component_register(&ecs_id(UiText), &ecs_id(UiText_desc));
    ECS_RESOURCE_REGISTER(UiLayoutCache);
    ecs_set_resource(UiLayoutCache, { 0 });
    ui_cache_ptr = ecs_get_resource(UiLayoutCache);
    ui_cache.query = ecs_query({ .components = { ecs_in(UiNode) } });
    ui_cache.tree_dirty = true;
    ui_cache.layout_dirty = true;
    ui_cache.paint_dirty = true;
    ui_text_init();
    ui_fonts_init_default();
    ecs_observer(
        { .on = EcsOnSet, .query.components = { ecs_filter(UiNode) }, .callback = component_set }
    );
    ecs_observer(
        { .on = EcsOnAdd,
          .query.components = { ecs_filter(UiNode) },
          .callback = component_add_remove }
    );
    ecs_observer(
        { .on = EcsOnRemove,
          .query.components = { ecs_filter(UiNode) },
          .callback = component_add_remove }
    );
    ecs_observer(
        { .on = EcsOnRelationSet,
          .query.components = { ecs_filter(UiNode) },
          .callback = relation_changed }
    );
    ecs_observer(
        { .on = EcsOnRelationRemove,
          .query.components = { ecs_filter(UiNode) },
          .callback = relation_changed }
    );
    if (props && props->gpu) {
        if (ecs_module_find(&ecs_id(sigpu)))
            ui_render_register();
        else
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "siui: import sigpu before enabling GPU rendering"
            );
    }
    ecs_at_fini({ .callback = fini_ui });
}

bool siui_layout_update(float viewport_width, float viewport_height) {
    ui_cache.stats = (siui_stats_t){ .generation = ui_cache.generation };
    if (viewport_width != ui_cache.viewport_width || viewport_height != ui_cache.viewport_height) {
        ui_cache.viewport_width = viewport_width;
        ui_cache.viewport_height = viewport_height;
        ui_cache.layout_dirty = true;
        for (uint32_t i = 0; i < ui_cache.count; i++)
            ui_cache.entries[i].flags |= MEASURE_DIRTY | ARRANGE_DIRTY;
    }
    if (!ui_cache.layout_dirty && !ui_cache.tree_dirty)
        return false;
    ui_cache.generation++;
    uint64_t start = SDL_GetTicksNS();
    if (ui_cache.tree_dirty)
        ui_tree_sync();
    ui_cache.stats.tree_sync_ns = SDL_GetTicksNS() - start;
    start = SDL_GetTicksNS();
    ui_measure();
    ui_cache.stats.measure_ns = SDL_GetTicksNS() - start;
    start = SDL_GetTicksNS();
    ui_arrange();
    ui_cache.stats.arrange_ns = SDL_GetTicksNS() - start;
    ui_cache.layout_dirty = false;
    ui_cache.stats.generation = ui_cache.generation;
    return true;
}

const UiComputedLayout *siui_layout(ecs_entity_t entity) {
    uint32_t at = ui_find(entity);
    return at == UI_NONE ? NULL : &ui_cache.entries[at].computed;
}

siui_stats_t siui_stats(void) { return ui_cache.stats; }
