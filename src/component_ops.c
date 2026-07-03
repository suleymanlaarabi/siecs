#include "compiler.h"
#include "component_require.h"
#include "datastructure/idmap.h"
#include "siecs.h"
#include "storage/component_index.h"
#include "storage/table_index.h"
#include "table.h"
#include "table_migration.h"
#include "type.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ecs_assert_can_be_updated(world, entity, ...)                                              \
    ecs_assert(!ecs_has_cid_owned(world, entity, ecs_id(Abstract)), __VA_ARGS__)

void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);
    ecs_assert_can_be_updated(world, entity, "An abstract entity cannot be updated.");

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);
    uint16_t edge = ecs_table_get_add_edge(table, cid);

    if (ECS_UNLIKELY(edge < table->type.count && table->type.ids[edge] == cid)) {
        return;
    }

    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);

    if (crec->required_count == 0) {
        if (edge == UINT16_MAX) {
            ecs_type_t new_type = ecs_type_with_add(&table->type, cid);
            edge = ecs_table_index_get_or_create(world, new_type);

            table = ecs_get_table(world, from_id);
            ecs_id_map_set(&table->add_edge, cid, edge);
        }

        ecs_table_t *new_table = ecs_get_table(world, edge);
        void *component_data = ecs_migrate_add(world, record, entity, table, new_table, edge, cid);

        if (crec->on_add) {
            crec->on_add(world, entity, cid, component_data);
        }
        ecs_emit(world, new_table, entity, EcsOnAdd, component_data);
        return;
    }

    ecs_add_plan_t plan = { 0 };
    ecs_add_plan_t *add_plan = NULL;

    if (edge == UINT16_MAX) {
        ecs_add_plan_build_type(world, table, cid, crec, &plan);
        add_plan = &plan;
        edge = ecs_table_index_get_or_create(world, plan.type);

        table = ecs_get_table(world, from_id);
        ecs_id_map_set(&table->add_edge, cid, edge);
    }

    ecs_table_t *new_table = ecs_get_table(world, edge);

    if (!add_plan && new_table->type.count > table->type.count + 1) {
        ecs_add_plan_build_added_only(world, table, cid, crec, &plan);
        add_plan = &plan;
    }

    void *component_data =
        add_plan && add_plan->added_count > 1
            ? ecs_migrate_add_many(world, record, entity, table, new_table, edge, cid)
            : ecs_migrate_add(world, record, entity, table, new_table, edge, cid);

    if (add_plan) {
        for (uint16_t i = 0; i < add_plan->added_count; i++) {
            ecs_component_t added = add_plan->added[i];
            const ecs_component_record_t *added_rec =
                ecs_component_index_get(&world->component_index, added);
            void *added_data = ecs_table_component_at_column(
                new_table,
                ecs_table_get_column_index(new_table, added),
                record->table_row
            );
            if (added_rec->on_add) {
                added_rec->on_add(world, entity, added, added_data);
            }
            ecs_emit(world, new_table, entity, EcsOnAdd, added_data);
        }
        ecs_add_plan_fini(add_plan);
    } else if (crec->on_add) {
        crec->on_add(world, entity, cid, component_data);
    }

    ecs_emit(world, new_table, entity, EcsOnAdd, component_data);
}

void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);

    if (ECS_UNLIKELY(col_idx == UINT16_MAX)) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_remove_at(&table->type, col_idx);
        new_table_id = ecs_table_index_get_or_create(world, new_type);
        table = ecs_get_table(world, from_id);
        table->cls[col_idx].remove_edge = new_table_id;
    }

    void *removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);

    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    if (crec->on_remove) {
        crec->on_remove(world, entity, cid, removed_data);
        table = ecs_get_table(world, from_id);
    }
    ecs_emit(world, table, entity, EcsOnRemove, removed_data);

    ecs_migrate_remove(world, record, entity, table, new_table_id, (uint16_t)col_idx);
}

void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    const ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    return ecs_table_component_at_column(
        table,
        ecs_table_get_column_index(table, cid),
        record->table_row
    );
}

void *ecs_try_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    const ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);
    if (col_idx != UINT16_MAX) {
        return ecs_table_component_at_column(table, col_idx, record->table_row);
    }
    return NULL;
}

void ecs_set_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_add_cid(world, entity, cid);
    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    uint16_t col_idx = ecs_table_get_column_index(table, cid);
    void *dst = ecs_table_component_at_column(table, col_idx, record->table_row);

    if (crec->on_set) {
        crec->on_set(world, entity, cid, data, dst);
        record = ecs_get_record(world, entity);
        table = ecs_get_table(world, record->table_id);
    }
    ecs_emit(world, table, entity, EcsOnSet, data);
    if (crec->size != 0) {
        memcpy(dst, data, crec->size);
    }
}

bool ecs_has_cid(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    uint16_t tid = ecs_get_record(world, entity)->table_id;
    return ecs_table_has(world, ecs_get_table(world, tid), id);
}

bool ecs_has_cid_owned(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    uint16_t tid = ecs_get_record(world, entity)->table_id;
    return ecs_table_has_owned(ecs_get_table(world, tid), id);
}

void ecs_with(ecs_world_t *world, ecs_component_t component, ecs_component_t require) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(component);
    ecs_assert_id_valid(require);
    ecs_assert(component != require, "component cannot require itself: %d\n", component);
#ifndef NDEBUG
    ecs_assert(
        !ecs_component_requires(world, require, component),
        "cyclic component requirement: %d requires %d\n",
        component,
        require
    );
#endif

    ecs_component_record_t *record =
        ecs_component_index_get_mut(&world->component_index, component);

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
