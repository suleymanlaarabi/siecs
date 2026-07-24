#include "command_buffer.h"
#include "datastructure/vec.h"
#include "siecs.h"
#include "storage/component_index.h"
#include "table.h"
#include "table_migration.h"
#include "type.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ecs_entity_t ecs_new(void) {
        ecs_table_t *table = ecs_get_table(0);

    ecs_entity_t entity = ecs_entity_index_create(&ecs_world.entity_index, table->entity_count);
    ecs_table_add_entity(table, entity);

    return entity;
}

bool ecs_is_alive(const ecs_entity_t entity) {
    return ecs_entity_index_is_alive(&ecs_world.entity_index, entity);
}

#ifndef NDEBUG
static inline bool
ecs_would_create_base_cycle(const ecs_entity_t entity, ecs_entity_t target) {
    while (target != 0) {
        if (target == entity) {
            return true;
        }
        const ecs_entity_record_t *target_record = ecs_get_record(target);
        const ecs_table_t *target_table = ecs_get_table(target_record->table_id);
        target = target_table->type.base;
    }
    return false;
}
#endif

static inline void ecs_entity_rebase(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);
    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    for (uint16_t i = 0; i < from_table->data_count; i++) {
        uint16_t col = from_table->data_columns[i];
        ecs_component_t component = from_table->type.ids[col];
        const ecs_component_record_t *crec =
            ecs_component_index_get(&ecs_world.component_index, component);
        void *src = ecs_table_component_at_column(from_table, col, old_row);
        void *dst = ecs_table_component_at_column(to_table, col, new_row);
        ecs_component_value_move_ctor(crec, dst, src, 1);
    }

    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row, false);
    if (moved != entity) {
        ecs_get_record(moved)->table_row = old_row;
    }

    record->table_id = to_table_id;
    record->table_row = new_row;
}

bool ecs_is(ecs_entity_t entity, ecs_entity_t target) {
    ecs_entity_t base = ecs_get_table(ecs_get_record(entity)->table_id)->type.base;
    if (base == target) {
        return true;
    }
    if (base == 0) {
        return false;
    }
    return ecs_is(base, target);
}

void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target) {
        ecs_assert_entity_valid(entity);
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(entity);
    ecs_assert_is_alive(target);
    ecs_assert(entity != target, "entity cannot inherit itself: %d\n", ecs_first(entity));
    ecs_assert(
        !ecs_would_create_base_cycle(entity, target),
        "cyclic inheritance: %d inherits from %d\n",
        ecs_first(entity),
        ecs_first(target)
    );
    ecs_assert(
        ecs_has_cid_owned(target, ecs_id(Abstract)),
        "An entity can only inherit from an abstract entity."
    );

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_table_id = record->table_id;
    ecs_table_t *from_table = ecs_get_table(from_table_id);
    if (from_table->type.base == target) {
        return;
    }

    ecs_type_t new_type = ecs_type_with_base(&from_table->type, target);
    uint16_t to_table_id = ecs_table_index_get_or_create(new_type);
    if (to_table_id == from_table_id) {
        return;
    }

    from_table = ecs_get_table(from_table_id);
    ecs_entity_rebase(record, entity, from_table, to_table_id);
}

void ecs_is_a(ecs_entity_t entity, ecs_entity_t target) {
        ecs_assert_entity_valid(entity);
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(entity);
    ecs_assert_is_alive(target);

    if (ecs_is_deferred()) {
        ecs_command_buffer_set_base(entity, target);
        return;
    }

    ecs_is_a_now(entity, target);
}

void ecs_kill_now(ecs_entity_t entity) {
        ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    uint16_t component_count = table->type.count;
    ecs_component_t *components = NULL;

    if (component_count != 0) {
        components = malloc(sizeof(ecs_component_t) * component_count);
        ecs_assert_not_null(components);
        for (uint16_t i = 0; i < component_count; i++) {
            components[i] = table->type.ids[i];
        }
    }

    for (uint16_t i = 0; i < component_count && ecs_is_alive(entity); i++) {
        ecs_component_t component = components[i];
        record = ecs_get_record(entity);
        table = ecs_get_table(record->table_id);

        uint16_t col_idx = ecs_table_column_or_invalid(table, component);
        if (col_idx == UINT16_MAX) {
            continue;
        }

        void *removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);
        const ecs_component_record_t *crec =
            ecs_component_index_get(&ecs_world.component_index, component);

        if (crec->on_remove) {
            crec->on_remove(entity, component, removed_data);
            if (!ecs_is_alive(entity)) {
                break;
            }
            record = ecs_get_record(entity);
            table = ecs_get_table(record->table_id);
            col_idx = ecs_table_column_or_invalid(table, component);
            if (col_idx == UINT16_MAX) {
                continue;
            }
            removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);
        }
        ecs_emit(table, entity, EcsOnRemove, removed_data);
    }

    free(components);
    if (!ecs_is_alive(entity)) {
        return;
    }

    record = ecs_get_record(entity);
    table = ecs_get_table(record->table_id);

    // Remove from table
    ecs_entity_t moved = ecs_table_remove_entity(table, record->table_row, true);
    if (moved != entity) {
        ecs_get_record(moved)->table_row = record->table_row;
    }

    ecs_entity_index_kill(&ecs_world.entity_index, ecs_first(entity));
}

void ecs_kill(ecs_entity_t entity) {
        ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_kill(entity);
        return;
    }

    ecs_kill_now(entity);
}
