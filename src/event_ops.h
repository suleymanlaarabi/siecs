#ifndef SIECS_EVENT_OPS_H
#define SIECS_EVENT_OPS_H

#include "relation.h"
#include "storage/component_index.h"
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
        while (diff->known_i < diff->known.component_count && diff->known.ids[diff->known_i] < id)
            diff->known_i++;
        if (diff->known_i < diff->known.component_count && diff->known.ids[diff->known_i] == id) {
            diff->candidate_i++;
            continue;
        }
        *index = diff->candidate_i++;
        return true;
    }
    return false;
}

static inline void
ecs_apply_component_default_relations(ecs_entity_t entity, ecs_component_t component) {
    const ecs_component_record_t *record = ecs_component_index_get(component);
    for (uint16_t i = 0; i < record->default_relation_count; i++) {
        const ecs_component_required_relation_t *required = &record->default_relations[i];
        if (!ecs_has_relation_id(entity, required->relation)) {
            ecs_relate_id_now(entity, required->relation, required->target);
        }
    }
}

/* Emit OnAdd/OnRemove for one column. Returns whether a hook or observer ran, which may have
 * created tables or moved the entity. */
static inline bool ecs_emit_component_event(
    ecs_table_t *table,
    ecs_entity_t entity,
    uint32_t row,
    uint16_t column,
    bool add,
    bool *has_default_relations
) {
    ecs_component_t id = table->type.ids[column];
    void *data = ecs_table_component_at_column(table, column, row);
    const ecs_component_record_t *record = ecs_component_index_get(id);
    ecs_component_on_add_t hook = add ? record->on_add : record->on_remove;
    *has_default_relations |= record->default_relation_count != 0;
    if (ECS_UNLIKELY(hook != NULL)) {
        hook(entity, id, data);
        if (!ecs_is_alive(entity))
            return true;
        const ecs_entity_record_t *current = ecs_get_record(entity);
        table = ecs_get_table(current->table_id);
        column = ecs_table_column_or_invalid(table, id);
        if (column == UINT16_MAX)
            return true;
        data = ecs_table_component_at_column(table, column, current->table_row);
        ecs_emit(table, entity, add ? EcsOnAdd : EcsOnRemove, id, data);
        return true;
    }
    return ecs_emit(table, entity, add ? EcsOnAdd : EcsOnRemove, id, data);
}

/*
 * Emit one event per component of `candidate` (the layout of `table_id`, where the entity
 * sits at `row`) missing from `known`. Until a callback runs, the diff index is the column
 * and nothing is reloaded. Afterwards the entity's record is reloaded before the next event,
 * since a callback may have moved table storage or migrated, killed or recycled the entity.
 * The diffed id arrays are separate allocations that table growth never moves.
 */
static inline bool ecs_emit_type_diff(
    ecs_type_t known,
    ecs_type_t candidate,
    uint16_t table_id,
    uint32_t row,
    ecs_entity_t entity,
    bool add
) {
    bool has_default_relations = false;
    ecs_type_diff_t diff = { .known = known, .candidate = candidate };
    ecs_table_t *table = ecs_get_table(table_id);
    bool stale = false, moved = false;
    uint16_t index;
    while (ecs_type_diff_next(&diff, &index)) {
        uint16_t column = index;
        if (ECS_UNLIKELY(stale)) {
            if (!ecs_is_alive(entity))
                break;
            const ecs_entity_record_t *record = ecs_get_record(entity);
            moved |= record->table_id != table_id;
            table_id = record->table_id;
            table = ecs_get_table(table_id);
            row = record->table_row;
            stale = false;
        }
        if (ECS_UNLIKELY(moved)) {
            column = ecs_table_column_or_invalid(table, candidate.ids[index]);
            if (column == UINT16_MAX)
                continue;
        }
        stale = ecs_emit_component_event(table, entity, row, column, add, &has_default_relations);
    }
    return has_default_relations;
}

static inline bool ecs_emit_added_components(
    const ecs_table_t *from_table,
    ecs_table_t *to_table,
    ecs_entity_t entity,
    uint32_t row
) {
    return ecs_emit_type_diff(
        from_table->type,
        to_table->type,
        (uint16_t)(to_table - table_index.tables),
        row,
        entity,
        true
    );
}

/* Takes types, not tables: the emission above may have moved table storage. */
static inline void ecs_apply_added_component_default_relations(
    const ecs_type_t *from_type,
    const ecs_type_t *to_type,
    ecs_entity_t entity
) {
    ecs_type_diff_t diff = { .known = *from_type, .candidate = *to_type };
    uint16_t column;
    while (ecs_type_diff_next(&diff, &column) && ecs_is_alive(entity))
        ecs_apply_component_default_relations(entity, diff.candidate.ids[column]);
}

static inline void ecs_emit_removed_components(
    ecs_table_t *from_table,
    const ecs_type_t *to_type,
    ecs_entity_t entity,
    uint32_t row
) {
    ecs_emit_type_diff(
        *to_type,
        from_table->type,
        (uint16_t)(from_table - table_index.tables),
        row,
        entity,
        false
    );
}

#endif
