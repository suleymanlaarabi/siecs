#include "datastructure/vec.h"
#include "siecs.h"
#include "storage/component_index.h"
#include "table.h"
#include "type.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <string.h>

ecs_entity_t ecs_new(ecs_world_t *world) {
    ecs_assert_not_null(world);
    ecs_table_t *table = ecs_get_table(world, 0);

    ecs_entity_t entity = ecs_entity_index_create(&world->entity_index, table->entity_count);
    ecs_table_add_entity(table, entity);

    return entity;
}

bool ecs_is_alive(const ecs_world_t *world, ecs_entity_t entity) {
    return ecs_entity_index_is_alive(&world->entity_index, entity);
}

static inline void copy_data_column(
    const ecs_column_t *restrict from,
    uint32_t from_row,
    ecs_column_t *restrict to,
    uint32_t to_row
) {
    memcpy(
        (uint8_t *)to->data + (from->size * to_row),
        (uint8_t *)from->data + (from->size * from_row),
        from->size
    );
}

#ifndef NDEBUG
static inline bool
ecs_would_create_base_cycle(const ecs_world_t *world, ecs_entity_t entity, ecs_entity_t target) {
    while (target != 0) {
        if (target == entity) {
            return true;
        }
        const ecs_entity_record_t *target_record = ecs_get_record(world, target);
        const ecs_table_t *target_table = ecs_get_table(world, target_record->table_id);
        target = target_table->type.base;
    }
    return false;
}
#endif

static inline void ecs_entity_rebase(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(world, to_table_id);
    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    for (uint16_t i = 0; i < from_table->data_count; i++) {
        uint16_t col = from_table->data_columns[i];
        copy_data_column(&from_table->cls[col], old_row, &to_table->cls[col], new_row);
    }

    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity) {
        ecs_get_record(world, moved)->table_row = old_row;
    }

    record->table_id = to_table_id;
    record->table_row = new_row;
}

void ecs_is_a(ecs_world_t *world, ecs_entity_t entity, ecs_entity_t target) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(world, entity);
    ecs_assert_is_alive(world, target);
    ecs_assert(entity != target, "entity cannot inherit itself: %d\n", ecs_first(entity));
    ecs_assert(
        !ecs_would_create_base_cycle(world, entity, target),
        "cyclic inheritance: %d inherits from %d\n",
        ecs_first(entity),
        ecs_first(target)
    );

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_table_id = record->table_id;
    ecs_table_t *from_table = ecs_get_table(world, from_table_id);
    if (from_table->type.base == target) {
        return;
    }

    ecs_type_t new_type = ecs_type_with_base(&from_table->type, target);
    uint16_t to_table_id = ecs_table_index_get_or_create(world, new_type);
    if (to_table_id == from_table_id) {
        return;
    }

    from_table = ecs_get_table(world, from_table_id);
    ecs_entity_rebase(world, record, entity, from_table, to_table_id);
}

void ecs_kill(ecs_world_t *world, ecs_entity_t entity) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    for (int i = 0; i < table->type.count; i++) {
        const ecs_component_record_t *crec =
            ecs_component_index_get(&world->component_index, table->type.ids[i]);

        if (crec->on_remove) {
            crec->on_remove(
                world,
                entity,
                table->type.ids[i],
                ecs_table_component_at_column(table, (uint16_t)i, record->table_row)
            );
        }
    }

    // Remove from table
    ecs_entity_t moved = ecs_table_remove_entity(table, record->table_row);
    if (moved != entity) {
        ecs_get_record(world, moved)->table_row = record->table_row;
    }

    ecs_entity_index_kill(&world->entity_index, ecs_first(entity));
}
