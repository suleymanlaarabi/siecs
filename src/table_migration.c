#include "table_migration.h"
#include "world_internal.h"
#include <stdint.h>
#include <string.h>

static inline void move_column(
        const ecs_table_t *from_table,
    const uint16_t from_col,
    const uint32_t from_row,
    ecs_table_t *to_table,
    const uint16_t to_col,
    const uint32_t to_row
) {
    const ecs_column_t *from_column = &from_table->cls[from_col];
    void *src = ecs_table_component_at_column(from_table, from_col, from_row);
    void *dst = ecs_table_component_at_column(to_table, to_col, to_row);
    if (from_column->flags & EcsColumnTrivialMove) {
        memcpy(dst, src, from_column->size);
        return;
    }

    ecs_component_t component = from_table->type.ids[from_col];
    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component);
    ecs_component_value_move_ctor(record, dst, src, 1);
}

static inline void ctor_column(
        const ecs_table_t *table,
    const uint16_t col,
    const uint32_t row
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
    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component);
    ecs_component_value_ctor(record, dst, 1);
}

static inline void dtor_column(
        const ecs_table_t *table,
    const uint16_t col,
    const uint32_t row
) {
    const ecs_column_t *column = &table->cls[col];
    if (column->flags & EcsColumnNoDtor) {
        return;
    }

    ecs_component_t component = table->type.ids[col];
    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component);
    void *ptr = ecs_table_component_at_column(table, col, row);
    ecs_component_value_dtor(record, ptr, 1);
}

static inline void finish_migration(
        ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint32_t old_row,
    const uint16_t to_table_id,
    const uint32_t new_row
) {
    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row, false);
    if (moved != entity) {
        ecs_get_record(moved)->table_row = old_row;
    }

    record->table_id = to_table_id;
    record->table_row = new_row;
}

void ecs_migrate_to_table(
        ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t fi = 0, ti = 0;
    while (fi < from_table->type.count && ti < to_table->type.count) {
        uint16_t fid = from_table->type.ids[fi];
        uint16_t tid = to_table->type.ids[ti];
        if (fid == tid) {
            move_column(from_table, fi, old_row, to_table, ti, new_row);
            fi++;
            ti++;
        } else if (fid < tid) {
            dtor_column(from_table, fi, old_row);
            fi++;
        } else {
            ctor_column(to_table, ti, new_row);
            ti++;
        }
    }
    for (; fi < from_table->type.count; fi++) {
        dtor_column(from_table, fi, old_row);
    }
    for (; ti < to_table->type.count; ti++) {
        ctor_column(to_table, ti, new_row);
    }

    finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
}

void *ecs_migrate_add(
        ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    const uint16_t to_table_id,
    const ecs_component_t added_id
) {
    const uint32_t old_row = record->table_row;
    const uint32_t new_row = ecs_table_add_entity(to_table, entity);

    const uint16_t k = ecs_table_get_column_index(to_table, added_id);
    ctor_column(to_table, k, new_row);

    uint16_t i = 0;
    for (; i < from_table->type.data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= k) {
            break;
        }
        move_column(from_table, from_col, old_row, to_table, from_col, new_row);
    }
    for (; i < from_table->type.data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        move_column(from_table, from_col, old_row, to_table, from_col + 1, new_row);
    }

    finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(to_table, k, new_row);
}

void *ecs_migrate_add_many(
        ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    const uint16_t to_table_id,
    const ecs_component_t requested_id
) {
    const uint32_t old_row = record->table_row;
    const uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t from_data = 0;
    for (uint16_t to_data = 0; to_data < to_table->type.data_count; to_data++) {
        const uint16_t to_col = to_table->data_columns[to_data];
        const ecs_component_t to_id = to_table->type.ids[to_col];

        while (from_data < from_table->type.data_count) {
            const uint16_t from_col = from_table->data_columns[from_data];
            const ecs_component_t from_id = from_table->type.ids[from_col];
            if (from_id >= to_id) {
                break;
            }
            from_data++;
        }

        if (from_data < from_table->type.data_count) {
            const uint16_t from_col = from_table->data_columns[from_data];
            if (from_table->type.ids[from_col] == to_id) {
                move_column(from_table, from_col, old_row, to_table, to_col, new_row);
                from_data++;
                continue;
            }
        }

        ctor_column(to_table, to_col, new_row);
    }

    finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(
        to_table,
        ecs_table_get_column_index(to_table, requested_id),
        new_row
    );
}

void ecs_migrate_remove(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id,
    uint16_t col_idx
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t i = 0;
    for (; i < from_table->type.data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= col_idx) {
            break;
        }
        move_column(from_table, from_col, old_row, to_table, from_col, new_row);
    }
    if (i < from_table->type.data_count && from_table->data_columns[i] == col_idx) {
        dtor_column(from_table, col_idx, old_row);
        i++;
    }
    for (; i < from_table->type.data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        move_column(from_table, from_col, old_row, to_table, from_col - 1, new_row);
    }

    finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
}
