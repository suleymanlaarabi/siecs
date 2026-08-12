#ifndef SIECS_TABLE_OPS_H
#define SIECS_TABLE_OPS_H

#include "storage/component_index.h"
#include "table.h"
#include "world_internal.h"
#include <string.h>

static inline void ecs_table_move_column(
    const ecs_table_t *from_table,
    uint16_t from_col,
    uint32_t from_row,
    ecs_table_t *to_table,
    uint16_t to_col,
    uint32_t to_row
) {
    const ecs_column_t *from_column = &from_table->cls[from_col];
    void *src = ecs_table_component_at_column(from_table, from_col, from_row);
    void *dst = ecs_table_component_at_column(to_table, to_col, to_row);
    if (from_column->flags & EcsColumnTrivialMove) {
        memcpy(dst, src, from_column->size);
        return;
    }

    ecs_component_t component = from_table->type.ids[from_col];
    const ecs_component_record_t *record = ecs_component_index_get(component);
    if (record->ops.move_ctor) {
        record->ops.move_ctor(dst, src, 1);
        return;
    }

    record->ops.copy_ctor(dst, src, 1);
    if (record->ops.dtor) {
        record->ops.dtor(src, 1);
    }
}

static inline void ecs_table_ctor_column(
    const ecs_table_t *table,
    uint16_t col,
    uint32_t row
) {
    const ecs_column_t *column = &table->cls[col];
    if (column->size == 0) {
        return;
    }

    void *dst = ecs_table_component_at_column(table, col, row);
    if (column->flags & EcsColumnZeroCtor) {
        memset(dst, 0, column->size);
        return;
    }

    ecs_component_t component = table->type.ids[col];
    const ecs_component_record_t *record = ecs_component_index_get(component);
    record->ops.ctor(dst, 1);
}

static inline void ecs_table_dtor_column(
    const ecs_table_t *table,
    uint16_t col,
    uint32_t row
) {
    const ecs_column_t *column = &table->cls[col];
    if (column->flags & EcsColumnNoDtor) {
        return;
    }

    ecs_component_t component = table->type.ids[col];
    const ecs_component_record_t *record = ecs_component_index_get(component);
    void *ptr = ecs_table_component_at_column(table, col, row);
    record->ops.dtor(ptr, 1);
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
