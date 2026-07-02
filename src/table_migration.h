#ifndef SIECS_TABLE_MIGRATION_H
#define SIECS_TABLE_MIGRATION_H
#include "siecs.h"
#include "storage/entity_index.h"
#include "table.h"
#include <stdint.h>

void ecs_migrate_to_table(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
);

void *ecs_migrate_add(
    const ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    uint16_t to_table_id,
    ecs_component_t added_id
);

void *ecs_migrate_add_many(
    const ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    uint16_t to_table_id,
    ecs_component_t requested_id
);

void ecs_migrate_remove(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id,
    uint16_t col_idx
);

#endif
