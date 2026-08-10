#ifndef SIECS_EVENT_OPS_H
#define SIECS_EVENT_OPS_H

#include "storage/component_index.h"
#include "relation.h"
#include "table.h"
#include "world_internal.h"

static inline void ecs_apply_component_default_relations(
    ecs_entity_t entity,
    ecs_component_t component
) {
    const ecs_component_required_relation_t *defaults =
        ecs_component_default_relations(component);
    for (const ecs_component_required_relation_t *required = defaults;
         required && required->relation;
         required++) {
        if (!ecs_has_relation_id(entity, required->relation)) {
            ecs_relate_id_now(entity, required->relation, required->target);
        }
    }
}

static inline bool ecs_emit_added_components(
    const ecs_table_t *from_table,
    ecs_table_t *to_table,
    ecs_entity_t entity,
    uint32_t row
) {
    bool has_default_relations = false;
    uint16_t from_i = 0;
    for (uint16_t to_i = 0; to_i < to_table->type.component_count; to_i++) {
        ecs_component_t added = to_table->type.ids[to_i];
        while (from_i < from_table->type.component_count && from_table->type.ids[from_i] < added) {
            from_i++;
        }
        if (from_i < from_table->type.component_count && from_table->type.ids[from_i] == added) {
            continue;
        }

        void *data = ecs_table_component_at_column(to_table, to_i, row);
        const ecs_component_record_t *record = ecs_component_index_get(added);
        if (record->on_add) {
            record->on_add(entity, added, data);
        }
        ecs_emit(to_table, entity, EcsOnAdd, data);
        has_default_relations |=
            ecs_component_default_relations(added) != NULL;
    }
    return has_default_relations;
}

static inline void ecs_apply_added_component_default_relations(
    const ecs_table_t *from_table,
    const ecs_table_t *to_table,
    ecs_entity_t entity
) {
    ecs_type_t from_type = from_table->type;
    ecs_type_t to_type = to_table->type;
    uint16_t from_i = 0;
    for (uint16_t to_i = 0; to_i < to_type.component_count; to_i++) {
        ecs_component_t added = to_type.ids[to_i];
        while (from_i < from_type.component_count && from_type.ids[from_i] < added) {
            from_i++;
        }
        if (from_i == from_type.component_count || from_type.ids[from_i] != added) {
            ecs_apply_component_default_relations(entity, added);
        }
    }
}

static inline void ecs_emit_removed_components(
    ecs_table_t *from_table,
    const ecs_type_t *to_type,
    ecs_entity_t entity,
    uint32_t row
) {
    uint16_t to_i = 0;
    for (uint16_t from_i = 0; from_i < from_table->type.component_count; from_i++) {
        ecs_component_t removed = from_table->type.ids[from_i];
        while (to_i < to_type->component_count && to_type->ids[to_i] < removed) {
            to_i++;
        }
        if (to_i < to_type->component_count && to_type->ids[to_i] == removed) {
            continue;
        }

        void *data = ecs_table_component_at_column(from_table, from_i, row);
        const ecs_component_record_t *record = ecs_component_index_get(removed);
        if (record->on_remove) {
            record->on_remove(entity, removed, data);
        }
        ecs_emit(from_table, entity, EcsOnRemove, data);
    }
}

#endif
