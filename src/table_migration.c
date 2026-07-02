#include "table_migration.h"
#include "world_internal.h"
#include <stdint.h>
#include <string.h>

static inline void copy_column(
    const ecs_column_t *restrict from,
    const uint32_t from_row,
    ecs_column_t *restrict to,
    const uint32_t to_row
) {
    if (from->size == 0) {
        return;
    }
    memcpy(
        (uint8_t *)to->data + (from->size * to_row),
        (uint8_t *)from->data + (from->size * from_row),
        from->size
    );
}

static inline void finish_migration(
    const ecs_world_t *world,
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint32_t old_row,
    const uint16_t to_table_id,
    const uint32_t new_row
) {
    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity) {
        ecs_get_record(world, moved)->table_row = old_row;
    }

    record->table_id = to_table_id;
    record->table_row = new_row;
}

void ecs_migrate_to_table(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(world, to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t fi = 0, ti = 0;
    while (fi < from_table->type.count && ti < to_table->type.count) {
        uint16_t fid = from_table->type.ids[fi];
        uint16_t tid = to_table->type.ids[ti];
        if (fid == tid) {
            copy_column(&from_table->cls[fi], old_row, &to_table->cls[ti], new_row);
            fi++;
            ti++;
        } else if (fid < tid) {
            fi++;
        } else {
            ecs_column_t *c = &to_table->cls[ti];
            if (c->size != 0) {
                memset((uint8_t *)c->data + (c->size * new_row), 0, c->size);
            }
            ti++;
        }
    }
    for (; ti < to_table->type.count; ti++) {
        ecs_column_t *c = &to_table->cls[ti];
        if (c->size != 0) {
            memset((uint8_t *)c->data + (c->size * new_row), 0, c->size);
        }
    }

    finish_migration(world, record, entity, from_table, old_row, to_table_id, new_row);
}

void *ecs_migrate_add(
    const ecs_world_t *world,
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
    ecs_column_t *added = &to_table->cls[k];
    if (added->size != 0) {
        memset((uint8_t *)added->data + (added->size * new_row), 0, added->size);
    }

    uint16_t i = 0;
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= k) {
            break;
        }
        copy_data_column(&from_table->cls[from_col], old_row, &to_table->cls[from_col], new_row);
    }
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        copy_data_column(
            &from_table->cls[from_col],
            old_row,
            &to_table->cls[from_col + 1],
            new_row
        );
    }

    finish_migration(world, record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(to_table, k, new_row);
}

void *ecs_migrate_add_many(
    const ecs_world_t *world,
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
    for (uint16_t to_data = 0; to_data < to_table->data_count; to_data++) {
        const uint16_t to_col = to_table->data_columns[to_data];
        const ecs_component_t to_id = to_table->type.ids[to_col];

        while (from_data < from_table->data_count) {
            const uint16_t from_col = from_table->data_columns[from_data];
            const ecs_component_t from_id = from_table->type.ids[from_col];
            if (from_id >= to_id) {
                break;
            }
            from_data++;
        }

        if (from_data < from_table->data_count) {
            const uint16_t from_col = from_table->data_columns[from_data];
            if (from_table->type.ids[from_col] == to_id) {
                copy_data_column(
                    &from_table->cls[from_col],
                    old_row,
                    &to_table->cls[to_col],
                    new_row
                );
                from_data++;
                continue;
            }
        }

        ecs_column_t *column = &to_table->cls[to_col];
        memset((uint8_t *)column->data + (column->size * new_row), 0, column->size);
    }

    finish_migration(world, record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(
        to_table,
        ecs_table_get_column_index(to_table, requested_id),
        new_row
    );
}

void ecs_migrate_remove(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id,
    uint16_t col_idx
) {
    ecs_table_t *to_table = ecs_get_table(world, to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t i = 0;
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= col_idx) {
            break;
        }
        copy_data_column(&from_table->cls[from_col], old_row, &to_table->cls[from_col], new_row);
    }
    if (i < from_table->data_count && from_table->data_columns[i] == col_idx) {
        i++;
    }
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        copy_data_column(
            &from_table->cls[from_col],
            old_row,
            &to_table->cls[from_col - 1],
            new_row
        );
    }

    finish_migration(world, record, entity, from_table, old_row, to_table_id, new_row);
}
