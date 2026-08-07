#include "command_buffer.h"
#include "component_require.h"
#include "datastructure/idmap.h"
#include "event_ops.h"
#include "helper.h"
#include "siecs.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "storage/table_index.h"
#include "table.h"
#include "table_migration.h"
#include "type.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ecs_assert_can_be_updated(entity, ...)                                                     \
    ecs_assert(!ecs_has_cid_owned(entity, ecs_id(Abstract)), __VA_ARGS__)

#define entity_edit(entity, table, record)                                                         \
    ecs_entity_record_t *record = ecs_get_record(entity);                                          \
    ecs_table_t *table = ecs_get_table(record->table_id);

void ecs_add_cid_now(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    ecs_assert_can_be_updated(entity, "An abstract entity cannot be updated.");

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(from_id);
    uint16_t edge = ecs_table_get_add_edge(table, cid);

    if (ECS_UNLIKELY(edge < table->type.component_count && table->type.ids[edge] == cid)) {
        return;
    }

    const ecs_component_record_t *crec = ecs_component_index_get(cid);

    if (crec->required_count == 0) {
        if (edge == UINT16_MAX) {
            ecs_type_t new_type = ecs_type_with(&table->type, cid, (ecs_type_pair_t){ 0 });
            edge = ecs_table_index_get_or_create(new_type);

            table = ecs_get_table(from_id);
            ecs_id_map_set(&table->add_edge, cid, edge);
        }

        ecs_table_t *new_table = ecs_get_table(edge);
        void *component_data = ecs_migrate(record, entity, table, edge, cid);

        if (crec->on_add) {
            crec->on_add(entity, cid, component_data);
        }
        ecs_emit(new_table, entity, EcsOnAdd, component_data);
        return;
    }

    if (edge == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_requirements(table, cid);
        edge = ecs_table_index_get_or_create(new_type);

        table = ecs_get_table(from_id);
        ecs_id_map_set(&table->add_edge, cid, edge);
    }

    ecs_table_t *new_table = ecs_get_table(edge);
    void *component_data = ecs_migrate(record, entity, table, edge, cid);

    if (new_table->type.component_count > table->type.component_count + 1) {
        ecs_emit_added_components(table, new_table, entity, record->table_row);
        return;
    }
    if (crec->on_add) {
        crec->on_add(entity, cid, component_data);
    }

    ecs_emit(new_table, entity, EcsOnAdd, component_data);
}

void ecs_add_cid(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    ecs_assert_can_be_updated(entity, "An abstract entity cannot be updated.");

    if (ecs_is_deferred()) {
        ecs_command_buffer_add(entity, cid);
        return;
    }

    ecs_add_cid_now(entity, cid);
}

void ecs_remove_cid_now(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(from_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);

    if (ECS_UNLIKELY(col_idx == UINT16_MAX)) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_without(&table->type, col_idx, 0);
        new_table_id = ecs_table_index_get_or_create(new_type);
        table = ecs_get_table(from_id);
        table->cls[col_idx].remove_edge = new_table_id;
    }

    void *removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);

    const ecs_component_record_t *crec = ecs_component_index_get(cid);
    if (crec->on_remove) {
        crec->on_remove(entity, cid, removed_data);
    }
    ecs_emit(table, entity, EcsOnRemove, removed_data);

    ecs_migrate(record, entity, table, new_table_id, 0);
}

void ecs_remove_cid(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_remove(entity, cid);
        return;
    }

    ecs_remove_cid_now(entity, cid);
}

/*
 * Resolve an owned or inherited component from a live entity record.
 * The record and every base in its type chain are trusted SIECS invariants;
 * callers perform the public entity validation before entering this helper.
 */
static inline void *
ecs_component_get_from_record(const ecs_entity_record_t *record, ecs_component_t component) {
    ecs_table_t *table = ecs_get_table(record->table_id);
    uint16_t col_idx = ecs_table_column_or_invalid(table, component);
    if (col_idx != UINT16_MAX) {
        return ecs_table_component_at_column(table, col_idx, record->table_row);
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *base_record = ecs_get_record(base);
        ecs_table_t *base_table = ecs_get_table(base_record->table_id);

        col_idx = ecs_table_column_or_invalid(base_table, component);
        if (col_idx != UINT16_MAX) {
            return ecs_table_component_at_column(base_table, col_idx, base_record->table_row);
        }

        base = base_table->type.base;
    }

    return NULL;
}

void *ecs_get_cid(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    return ecs_component_get_from_record(ecs_get_record(entity), cid);
}

void *ecs_try_get_cid(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    return ecs_component_get_from_record(ecs_get_record(entity), cid);
}

void ecs_set_cid_now(ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_add_cid_now(entity, cid);
    ecs_defer_begin();
    const ecs_component_record_t *crec = ecs_component_index_get(cid);
    entity_edit(entity, table, record);
    void *dst = ecs_table_get_component(table, cid, record->table_row);

    if (crec->on_set) {
        crec->on_set(entity, cid, data, dst);
    }
    ecs_emit(table, entity, EcsOnSet, data);
    if (crec->relation_flags & EcsComponentRelationTarget) {
        ((RelationTarget *)dst)->target = ((const RelationTarget *)data)->target;
    } else {
        ecs_component_value_copy(crec, dst, data, 1);
    }
    ecs_defer_end();
}

void ecs_set_cid(ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_set(entity, cid, data);
        return;
    }

    ecs_set_cid_now(entity, cid, data);
}

void ecs_move_cid_now(ecs_entity_t entity, ecs_component_t cid, void *data) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    bool had_value = ecs_has_cid_owned(entity, cid);
    ecs_add_cid_now(entity, cid);
    const ecs_component_record_t *crec = ecs_component_index_get(cid);
    entity_edit(entity, table, record);
    void *dst = ecs_table_get_component(table, cid, record->table_row);

    if (crec->on_set) {
        crec->on_set(entity, cid, data, dst);
    }
    ecs_emit(table, entity, EcsOnSet, data);
    if (crec->relation_flags & EcsComponentRelationTarget) {
        ((RelationTarget *)dst)->target = ((const RelationTarget *)data)->target;
    } else if (had_value || crec->ops.ctor) {
        ecs_component_value_move(crec, dst, data, 1);
    } else {
        ecs_component_value_move_ctor(crec, dst, data, 1);
    }
}

void ecs_move_cid(ecs_entity_t entity, ecs_component_t cid, void *data) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_move(entity, cid, data);
        return;
    }

    ecs_move_cid_now(entity, cid, data);
}

bool ecs_has_cid(const ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    uint16_t tid = ecs_get_record(entity)->table_id;
    return ecs_table_has(ecs_get_table(tid), id);
}

bool ecs_has_cid_owned(const ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    uint16_t tid = ecs_get_record(entity)->table_id;
    return ecs_table_has_owned(ecs_get_table(tid), id);
}

void ecs_with(ecs_component_t component, ecs_component_t require) {
    ecs_assert_id_valid(component);
    ecs_assert_id_valid(require);
    ecs_assert(component != require, "component cannot require itself: %d\n", component);
#ifndef NDEBUG
    ecs_assert(
        !ecs_component_requires(require, component),
        "cyclic component requirement: %d requires %d\n",
        component,
        require
    );
#endif

    ecs_component_record_t *record = ecs_component_index_get_mut(component);

    ecs_assert(record->tables.size == 0, "component already used cannot register requirement");

#ifndef NDEBUG
    for (uint32_t i = 0; i < record->required_count; i++) {
        if (record->required[i] == require) {
            ecs_assert(true, "required component already registered");
        }
    }
#endif

    record->required =
        realloc(record->required, sizeof(ecs_component_t) * (record->required_count + 1));
    record->required[record->required_count++] = require;
}
