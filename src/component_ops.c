#include "command_buffer.h"
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
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef ecs_with
#undef ecs_with
#endif

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
        if (ECS_UNLIKELY(ecs_component_default_relations(cid))) {
            ecs_apply_component_default_relations(entity, cid);
        }
        return;
    }

    if (edge == UINT16_MAX) {
        ecs_component_t added[ECS_COMPONENT_REQUIRE_CAPACITY];
        uint16_t count = 0, required = 0;
        bool component_pending = true;
        while (required < crec->required_count || component_pending) {
            ecs_component_t next = required < crec->required_count
                                       ? crec->required[required]
                                       : UINT16_MAX;
            if (component_pending && cid < next) { next = cid; component_pending = false; }
            else if (component_pending && cid == next) component_pending = false;
            else required++;
            if (!ecs_table_has_owned(table, next)) added[count++] = next;
        }
        ecs_type_t new_type = ecs_type_with_added_ids(&table->type, added, count);
        edge = ecs_table_index_get_or_create(new_type);

        table = ecs_get_table(from_id);
        ecs_id_map_set(&table->add_edge, cid, edge);
    }

    ecs_table_t *new_table = ecs_get_table(edge);
    void *component_data = ecs_migrate(record, entity, table, edge, cid);

    if (new_table->type.component_count > table->type.component_count + 1) {
        if (ecs_emit_added_components(table, new_table, entity, record->table_row)) {
            ecs_apply_added_component_default_relations(table, new_table, entity);
        }
        return;
    }
    if (crec->on_add) {
        crec->on_add(entity, cid, component_data);
    }
    ecs_emit(new_table, entity, EcsOnAdd, component_data);
    if (ECS_UNLIKELY(ecs_component_default_relations(cid))) {
        ecs_apply_component_default_relations(entity, cid);
    }
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
    return ecs_get_cid(entity, cid);
}

static inline void ecs_store_cid_now(
    ecs_entity_t entity, ecs_component_t cid, void *data, bool move
) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    bool had_value = move && ecs_has_cid_owned(entity, cid);
    ecs_add_cid_now(entity, cid);
    if (!move) ecs_defer_begin();
    const ecs_component_record_t *crec = ecs_component_index_get(cid);
    entity_edit(entity, table, record);
    void *dst = ecs_table_get_component(table, cid, record->table_row);

    if (crec->on_set) {
        crec->on_set(entity, cid, data, dst);
    }
    ecs_emit(table, entity, EcsOnSet, data);
    if (crec->relation_flags & EcsComponentRelationTarget) {
        ((RelationTarget *)dst)->entity = ((const RelationTarget *)data)->entity;
    } else if (!move) {
        ecs_component_value_copy(crec, dst, data, 1);
    } else if (had_value || crec->ops.ctor) ecs_component_value_move(crec, dst, data, 1);
    else ecs_component_value_move_ctor(crec, dst, data, 1);
    if (!move) ecs_defer_end();
}

void ecs_set_cid_now(ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_store_cid_now(entity, cid, (void *)data, false);
}

static inline void ecs_store_cid(ecs_entity_t entity, ecs_component_t cid, void *data, bool move) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    if (ecs_is_deferred()) {
        if (move) ecs_command_buffer_move(entity, cid, data);
        else ecs_command_buffer_set(entity, cid, data);
        return;
    }
    ecs_store_cid_now(entity, cid, data, move);
}

void ecs_set_cid(ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_store_cid(entity, cid, (void *)data, false);
}

void ecs_move_cid_now(ecs_entity_t entity, ecs_component_t cid, void *data) {
    ecs_store_cid_now(entity, cid, data, true);
}

void ecs_move_cid(ecs_entity_t entity, ecs_component_t cid, void *data) {
    ecs_store_cid(entity, cid, data, true);
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

static uint32_t ecs_required_lower_bound(
    const ecs_component_t *ids, uint32_t count, ecs_component_t id
) {
    uint32_t first = 0;
    while (first < count) {
        uint32_t middle = first + (count - first) / 2;
        if (ids[middle] < id) first = middle + 1;
        else count = middle;
    }
    return first;
}

static void ecs_required_add(ecs_component_record_t *record, ecs_component_t id) {
    uint32_t at = ecs_required_lower_bound(record->required, record->required_count, id);
    if (at < record->required_count && record->required[at] == id) return;
    ecs_assert(record->required_count < ECS_COMPONENT_REQUIRE_CAPACITY - 1,
               "component requirement capacity exceeded\n");
    record->required = realloc(record->required,
                               sizeof *record->required * (record->required_count + 1));
    memmove(record->required + at + 1, record->required + at,
            (record->required_count - at) * sizeof *record->required);
    record->required[at] = id;
    record->required_count++;
}

static inline void ecs_with_impl(ecs_component_t component, ecs_component_t require) {
    ecs_assert_id_valid(component);
    ecs_assert_id_valid(require);
    ecs_assert(component != require, "component cannot require itself: %d\n", component);
    const ecs_component_record_t *required_record = ecs_component_index_get(require);
    uint32_t cycle = ecs_required_lower_bound(
        required_record->required, required_record->required_count, component);
    ecs_assert(
        cycle == required_record->required_count || required_record->required[cycle] != component,
        "cyclic component requirement: %d requires %d\n",
        component,
        require
    ); (void)cycle;

    ecs_component_record_t *records = component_index.components.data;
    for (uint32_t i = 1; i < component_index.components.size; i++) {
        ecs_component_record_t *record = &records[i];
        uint32_t at = ecs_required_lower_bound(record->required, record->required_count, component);
        if (i != component &&
            (at == record->required_count || record->required[at] != component)) continue;
        ecs_assert(record->tables.size == 0, "component already used cannot register requirement");
        ecs_required_add(record, require);
        for (uint32_t r = 0; r < required_record->required_count; r++)
            ecs_required_add(record, required_record->required[r]);
    }
}

void ecs_with_relation_id(ecs_component_t cid, ecs_relation_id_t relation, ecs_entity_t target) {
    ecs_assert_id_valid(cid);
    ecs_assert(
        relation != 0 && relation < ecs_relation_count() && ecs_relation_info(relation),
        "relation must be registered: %u\n",
        relation
    );
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(target);

    ecs_component_record_t *record = ecs_component_index_get(cid);
    ecs_assert(record->tables.size == 0, "component already used cannot register relation default");

    for (uint16_t i = 0; i < record->default_relation_count; i++) {
        const ecs_component_required_relation_t *current = &record->default_relations[i];
        if (current->relation != relation) {
            continue;
        }
        ecs_assert(
            current->target == target,
            "component already has a different default target for relation: %u\n",
            relation
        );
        return;
    }

    record->default_relations = realloc(
        record->default_relations,
        sizeof *record->default_relations * (record->default_relation_count + 1)
    );
    record->default_relations[record->default_relation_count++] =
        (ecs_component_required_relation_t){ .relation = relation, .target = target };
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvarargs"
#endif
void ecs_with_many(ecs_component_t component, ...) {
    va_list args;
    va_start(args, component);

    ecs_component_t require;
    while ((require = (ecs_component_t)va_arg(args, int)) != 0) {
        ecs_with_impl(component, require);
    }

    va_end(args);
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
