#ifndef SIECS_EVENT_OPS_H
#define SIECS_EVENT_OPS_H

#include "storage/component_index.h"
#include "relation.h"
#include "table.h"
#include "world_internal.h"

typedef struct {
    ecs_type_t known;
    ecs_type_t candidate;
    uint16_t known_i;
    uint16_t candidate_i;
} ecs_type_diff_t;

static inline bool ecs_type_diff_next(ecs_type_diff_t *diff, uint16_t *index) {
    while (diff->candidate_i < diff->candidate.component_count) {
        ecs_component_t id = diff->candidate.ids[diff->candidate_i];
        while (diff->known_i < diff->known.component_count &&
               diff->known.ids[diff->known_i] < id) diff->known_i++;
        if (diff->known_i < diff->known.component_count &&
            diff->known.ids[diff->known_i] == id) { diff->candidate_i++; continue; }
        *index = diff->candidate_i++;
        return true;
    }
    return false;
}

static inline void ecs_apply_component_default_relations(
    ecs_entity_t entity,
    ecs_component_t component
) {
    const ecs_component_record_t *record = ecs_component_index_get(component);
    for (uint16_t i = 0; i < record->default_relation_count; i++) {
        const ecs_component_required_relation_t *required = &record->default_relations[i];
        if (!ecs_has_relation_id(entity, required->relation)) {
            ecs_relate_id_now(entity, required->relation, required->target);
        }
    }
}

static inline bool ecs_emit_component_event(
    ecs_table_t *table, ecs_entity_t entity, uint32_t row, uint16_t column, bool add
) {
    ecs_component_t id = table->type.ids[column];
    void *data = ecs_table_component_at_column(table, column, row);
    const ecs_component_record_t *record = ecs_component_index_get(id);
    ecs_component_on_add_t hook = add ? record->on_add : record->on_remove;
    bool has_default_relations = record->default_relation_count != 0;
    if (hook) hook(entity, id, data);
    ecs_emit(table, entity, add ? EcsOnAdd : EcsOnRemove, data);
    return has_default_relations;
}

static inline bool ecs_emit_added_components(
    const ecs_table_t *from_table,
    ecs_table_t *to_table,
    ecs_entity_t entity,
    uint32_t row
) {
    bool has_default_relations = false;
    ecs_type_diff_t diff = { .known = from_table->type, .candidate = to_table->type };
    uint16_t column;
    while (ecs_type_diff_next(&diff, &column)) {
        has_default_relations |= ecs_emit_component_event(to_table, entity, row, column, true);
    }
    return has_default_relations;
}

static inline void ecs_apply_added_component_default_relations(
    const ecs_table_t *from_table,
    const ecs_table_t *to_table,
    ecs_entity_t entity
) {
    ecs_type_diff_t diff = { .known = from_table->type, .candidate = to_table->type };
    uint16_t column;
    while (ecs_type_diff_next(&diff, &column))
        ecs_apply_component_default_relations(entity, diff.candidate.ids[column]);
}

static inline void ecs_emit_removed_components(
    ecs_table_t *from_table,
    const ecs_type_t *to_type,
    ecs_entity_t entity,
    uint32_t row
) {
    ecs_type_diff_t diff = { .known = *to_type, .candidate = from_table->type };
    uint16_t column;
    while (ecs_type_diff_next(&diff, &column))
        ecs_emit_component_event(from_table, entity, row, column, false);
}

#endif
