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
        table = ecs_get_table(ecs_entity_record_table(record));
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

/* Lifecycle hooks of non-trivial values (out of line: row operations inline
 * only the byte copies of plain values). */
void ecs_value_move_hooked(ecs_component_t id, void *dst, void *src);
void ecs_value_ctor_hooked(ecs_component_t id, void *dst);
void ecs_value_dtor_hooked(ecs_component_t id, void *ptr);

/* Move-construct `to[to_row]` from `from[from_row]`, leaving the source dead. */
static ECS_ALWAYS_INLINE void ecs_data_column_move(
    const ecs_data_column_t *from,
    uint32_t from_row,
    const ecs_data_column_t *to,
    uint32_t to_row
) {
    void *src = ecs_data_column_at(from, from_row);
    void *dst = ecs_data_column_at(to, to_row);
    if (ECS_LIKELY(from->flags & EcsColumnTrivialMove))
        ecs_bytes_copy(dst, src, from->size);
    else
        ecs_value_move_hooked(from->id, dst, src);
}

static ECS_ALWAYS_INLINE void ecs_data_column_ctor(const ecs_data_column_t *column, uint32_t row) {
    void *dst = ecs_data_column_at(column, row);
    if (ECS_LIKELY(column->flags & EcsColumnZeroCtor))
        ecs_bytes_zero(dst, column->size);
    else
        ecs_value_ctor_hooked(column->id, dst);
}

static ECS_ALWAYS_INLINE void ecs_data_column_dtor(const ecs_data_column_t *column, uint32_t row) {
    if (ECS_UNLIKELY(!(column->flags & EcsColumnNoDtor)))
        ecs_value_dtor_hooked(column->id, ecs_data_column_at(column, row));
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
    atomic_store_explicit(&record->table_id, to_table_id, memory_order_relaxed);
    record->table_row = new_row;
}

#endif
