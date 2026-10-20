#ifndef SIECS_TABLE_OPS_H
#define SIECS_TABLE_OPS_H

#include "storage/component_index.h"
#include "table.h"
#include "world_internal.h"
#include <string.h>

typedef struct {
    const ecs_table_t *table;
    uint32_t row;
    uint16_t column;
    bool shared;
} ecs_component_location_t;

/* A location, rather than a data pointer, also represents zero-sized tags.
 * Presence excludes inherited Abstract; value/field access keeps its existing
 * rules. Owned fields use row zero to return the whole column. */
static inline ecs_component_location_t
ecs_component_locate(const ecs_table_t *table, uint32_t row, ecs_component_t id, bool presence) {
    uint16_t column = ecs_table_column_or_invalid(table, id);
    if (column != UINT16_MAX)
        return (ecs_component_location_t){ table, row, column, false };
    if (id == ecs_id(Name) || (presence && id == ecs_id(Abstract)))
        return (ecs_component_location_t){ 0 };
    ecs_entity_t base = ecs_type_isa_target(&table->type);
    while (base) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        table = ecs_get_table(record->table_id);
        column = ecs_table_column_or_invalid(table, id);
        if (column != UINT16_MAX)
            return (ecs_component_location_t){ table, record->table_row, column, true };
        base = ecs_type_isa_target(&table->type);
    }
    return (ecs_component_location_t){ 0 };
}

static inline void *ecs_data_column_at(const ecs_data_column_t *column, uint32_t row) {
    return (uint8_t *)column->data + (size_t)column->size * row;
}

/* Move-construct `to[to_row]` from `from[from_row]`, leaving the source dead. */
static inline void ecs_data_column_move(
    const ecs_data_column_t *from,
    uint32_t from_row,
    const ecs_data_column_t *to,
    uint32_t to_row
) {
    void *src = ecs_data_column_at(from, from_row);
    void *dst = ecs_data_column_at(to, to_row);
    if (from->flags & EcsColumnTrivialMove) {
        ecs_bytes_copy(dst, src, from->size);
        return;
    }

    const ecs_component_record_t *record = ecs_component_index_get(from->id);
    if (record->ops.move_ctor) {
        record->ops.move_ctor(dst, src, 1);
        return;
    }

    record->ops.copy_ctor(dst, src, 1);
    if (record->ops.dtor) {
        record->ops.dtor(src, 1);
    }
}

static inline void ecs_data_column_ctor(const ecs_data_column_t *column, uint32_t row) {
    void *dst = ecs_data_column_at(column, row);
    if (column->flags & EcsColumnZeroCtor) {
        ecs_bytes_zero(dst, column->size);
        return;
    }

    ecs_component_index_get(column->id)->ops.ctor(dst, 1);
}

static inline void ecs_data_column_dtor(const ecs_data_column_t *column, uint32_t row) {
    if (column->flags & EcsColumnNoDtor) {
        return;
    }

    ecs_component_index_get(column->id)->ops.dtor(ecs_data_column_at(column, row), 1);
}

static inline void ecs_table_remove_entity_update_record(
    ecs_table_t *table,
    ecs_entity_t entity,
    uint32_t row,
    bool row_values_live
) {
    ecs_entity_t moved = ecs_table_remove_entity(table, row, row_values_live);
    if (moved != entity) {
        ecs_get_record(moved)->table_row = row;
    }
}

static inline void ecs_table_finish_migration(
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint32_t old_row,
    uint16_t to_table_id,
    uint32_t new_row
) {
    ecs_table_remove_entity_update_record(from_table, entity, old_row, false);
    record->table_id = to_table_id;
    record->table_row = new_row;
}

#endif
