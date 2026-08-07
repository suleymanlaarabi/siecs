#include "table_migration.h"
#include "world_internal.h"
#include <stdint.h>

void *ecs_migrate(
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_table_id,
    const ecs_component_t requested_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t from_data = 0;
    uint16_t to_data = 0;
    while (from_data < from_table->add_edge.aux && to_data < to_table->add_edge.aux) {
        uint16_t from_col = from_table->data_columns[from_data];
        uint16_t to_col = to_table->data_columns[to_data];
        ecs_component_t from_id = from_table->type.ids[from_col];
        ecs_component_t to_id = to_table->type.ids[to_col];
        if (from_id == to_id) {
            ecs_table_move_column(from_table, from_col, old_row, to_table, to_col, new_row);
            from_data++;
            to_data++;
        } else if (from_id < to_id) {
            ecs_table_dtor_column(from_table, from_col, old_row);
            from_data++;
        } else {
            ecs_table_ctor_column(to_table, to_col, new_row);
            to_data++;
        }
    }
    while (from_data < from_table->add_edge.aux) {
        ecs_table_dtor_column(from_table, from_table->data_columns[from_data], old_row);
        from_data++;
    }
    while (to_data < to_table->add_edge.aux) {
        ecs_table_ctor_column(to_table, to_table->data_columns[to_data], new_row);
        to_data++;
    }

    ecs_table_finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
    if (!requested_id) {
        return NULL;
    }
    return ecs_table_component_at_column(
        to_table,
        ecs_table_get_column_index(to_table, requested_id),
        new_row
    );
}
