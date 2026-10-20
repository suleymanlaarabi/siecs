#include "table_migration.h"
#include "world_internal.h"
#include <stdint.h>

static inline void *ecs_migrate_impl(
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_table_id,
    const ecs_component_t requested_id,
    const ecs_component_t unconstructed_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    /* Both column lists are sorted by id: one merge pass moves shared values,
     * destroys dropped ones and constructs added ones. */
    const ecs_data_column_t *from = from_table->data_columns;
    const ecs_data_column_t *from_end = from + from_table->add_edge.aux;
    const ecs_data_column_t *to = to_table->data_columns;
    const ecs_data_column_t *to_end = to + to_table->add_edge.aux;
    while (from < from_end && to < to_end) {
        if (from->id == to->id) {
            ecs_data_column_move(from++, old_row, to++, new_row);
        } else if (from->id < to->id) {
            ecs_data_column_dtor(from++, old_row);
        } else {
            if (to->id != unconstructed_id)
                ecs_data_column_ctor(to, new_row);
            to++;
        }
    }
    while (from < from_end) {
        ecs_data_column_dtor(from++, old_row);
    }
    for (; to < to_end; to++) {
        if (to->id != unconstructed_id)
            ecs_data_column_ctor(to, new_row);
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

void *ecs_migrate(
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_table_id,
    const ecs_component_t requested_id
) {
    return ecs_migrate_impl(record, entity, from_table, to_table_id, requested_id, 0);
}

void *ecs_migrate_add(
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_table_id,
    const ecs_component_t added_id
) {
    return ecs_migrate_impl(record, entity, from_table, to_table_id, added_id, added_id);
}
