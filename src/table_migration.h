#ifndef SIECS_TABLE_MIGRATION_H
#define SIECS_TABLE_MIGRATION_H
#include "siecs.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "table.h"
#include "table_ops.h"
#include <stdint.h>

#define ECS_ADD_PLAN_MAX_COMPONENTS 64

ecs_type_t ecs_type_with_requirements(
        ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec
);

#ifndef NDEBUG
bool ecs_component_requires(
    const     ecs_component_t component,
    ecs_component_t require
);
#endif

static inline void ecs_migrate_same_layout(
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);
    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    for (uint16_t i = 0; i < from_table->type.data_count; i++) {
        uint16_t col = from_table->data_columns[i];
        ecs_table_move_column(from_table, col, old_row, to_table, col, new_row);
    }

    ecs_table_finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
}

void ecs_migrate_to_table(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
);

void *ecs_migrate_add(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    uint16_t to_table_id,
    ecs_component_t added_id
);

void *ecs_migrate_add_many(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    uint16_t to_table_id,
    ecs_component_t requested_id
);

void ecs_migrate_remove(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id,
    uint16_t col_idx
);

#endif
