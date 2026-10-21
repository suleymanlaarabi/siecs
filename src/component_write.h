#ifndef SIECS_COMPONENT_WRITE_H
#define SIECS_COMPONENT_WRITE_H

#include "world_internal.h"

typedef enum {
    EcsWriteMove = 1 << 0,
    EcsWriteConstruct = 1 << 1,
    EcsWriteRelationTarget = 1 << 2,
    EcsWriteNotify = 1 << 3,
    EcsWriteCheckAlive = 1 << 4,
} ecs_component_write_flags_t;

/* The caller supplies the already resolved column. Hooks may grow the table
 * index or migrate the entity, so only that path resolves the location again.
 * Relation storage writes the target without overwriting its reverse index. */
static inline bool ecs_component_write(
    ecs_entity_t entity,
    ecs_component_t id,
    const ecs_component_record_t *component,
    ecs_table_t *table,
    uint32_t row,
    uint16_t column,
    void *data,
    unsigned flags
) {
    void *dst = ecs_table_component_at_column(table, column, row);
    if (component->on_set) {
        component->on_set(entity, id, data, dst);
        if ((flags & EcsWriteCheckAlive) && !ecs_is_alive(entity))
            return false;
        const ecs_entity_record_t *record = ecs_get_record(entity);
        table = ecs_get_table(ecs_entity_record_table(record));
        dst = ecs_table_get_component(table, id, record->table_row);
    }
    if (flags & EcsWriteNotify)
        ecs_emit(table, entity, EcsOnSet, id, data);
    if (flags & EcsWriteRelationTarget)
        ((RelationTarget *)dst)->entity = ((const RelationTarget *)data)->entity;
    else
        ecs_value_transfer(
            &component->ops,
            component->info->size,
            dst,
            data,
            1,
            flags & EcsWriteMove,
            flags & EcsWriteConstruct
        );
    return true;
}

#endif
