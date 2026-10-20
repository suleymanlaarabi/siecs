#ifndef SIECS_TABLE_MIGRATION_H
#define SIECS_TABLE_MIGRATION_H
#include "siecs.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "storage/table_index.h"
#include "table.h"
#include "table_ops.h"
#include <stdint.h>

static inline void ecs_migrate_same_layout(
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);
    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    for (uint16_t i = 0; i < from_table->add_edge.aux; i++) {
        uint16_t col = from_table->data_columns[i];
        ecs_table_move_column(from_table, col, old_row, to_table, col, new_row);
    }

    ecs_table_finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
}

void *ecs_migrate(
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id,
    ecs_component_t requested_id
);

/* Type registration may relocate the table index. Reacquire the source only
 * afterwards; same-layout pair edits keep their specialized column loop. */
static inline ecs_table_t *ecs_table_change_type(
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_type_t type,
    bool same_layout
) {
    uint16_t from_id = record->table_id;
    uint16_t to_id = ecs_table_index_get_or_create(type);
    if (to_id != from_id) {
        ecs_table_t *from = ecs_get_table(from_id);
        if (same_layout)
            ecs_migrate_same_layout(record, entity, from, to_id);
        else
            ecs_migrate(record, entity, from, to_id, 0);
    }
    return ecs_get_table(record->table_id);
}

#endif
