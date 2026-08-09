#include "command_buffer.h"
#include "event_ops.h"
#include "helper.h"
#include "inheritance.h"
#include "siecs.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "table.h"
#include "table_migration.h"
#include "type.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline ecs_entity_t ecs_entity_index_create(uint32_t row, bool reuse) {
    ecs_entity_index_t *index = &ecs_world.entity_index;
    uint32_t entity_id;
    uint32_t generation;
    if (reuse && index->first_available != UINT32_MAX) {
        entity_id = index->first_available;
        ecs_entity_record_t *record = ecs_entity_index_get_record(entity_id);
        index->first_available = record->table_row;
        generation = record->generation;
        record->table_id = 0;
        record->table_row = row;
    } else {
        entity_id = index->entities.size;
        generation = 0;
        ecs_entity_record_t *record =
            sicore_vec_push_empty(&index->entities, sizeof(ecs_entity_record_t));
        *record = (ecs_entity_record_t){ .generation = 0, .table_row = row, .table_id = 0 };
    }
    return ecs_entity(entity_id, generation);
}

ecs_entity_t ecs_new(void) {
    ecs_table_t *table = ecs_get_table(0);

    ecs_entity_t entity = ecs_entity_index_create(table->entity_count, true);
    ecs_table_add_entity(table, entity);

    return entity;
}

ecs_entity_t ecs_new_no_reuse(void) {
    ecs_table_t *table = ecs_get_table(0);

    ecs_entity_t entity = ecs_entity_index_create(table->entity_count, false);
    ecs_table_add_entity(table, entity);

    return entity;
}

bool ecs_is_alive(const ecs_entity_t entity) { return ecs_entity_index_is_alive(entity); }

ecs_entity_t ecs_entity_from_index(uint32_t index) {
    if (index == 0 || index >= ecs_world.entity_index.entities.size) {
        return 0;
    }

    const ecs_entity_record_t *record =
        sicore_vec_get(&ecs_world.entity_index.entities, index, ecs_entity_record_t);
    if (record->table_id == UINT16_MAX) {
        return 0;
    }

    return ecs_entity(index, record->generation);
}

#ifndef NDEBUG
static inline bool ecs_would_create_base_cycle(const ecs_entity_t entity, ecs_entity_t target) {
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

ecs_entity_t ecs_entity_base(ecs_entity_t entity) {
    ecs_assert_is_alive(entity);
    return ecs_get_table(ecs_get_record(entity)->table_id)->type.base;
}

ecs_entity_t ecs_lookup(const char *key) {
    uint32_t index = sicore_map_get(&name_map, key);
    if (index == UINT32_MAX) {
        return 0;
    }
    return ecs_entity(index, ecs_entity_index_get_record(index)->generation);
}

void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    if (target) {
        ecs_assert_entity_valid(target);
        ecs_assert_is_alive(target);
        ecs_assert(entity != target, "entity cannot inherit itself: %d\n", ecs_first(entity));
        ecs_assert(
            !ecs_would_create_base_cycle(entity, target),
            "cyclic inheritance: %d inherits from %d\n",
            ecs_first(entity),
            ecs_first(target)
        );
        if (!ecs_has_cid_owned(target, ecs_id(Abstract))) {
            ecs_add_cid_now(target, ecs_id(Abstract));
        }
    }

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_table_id = record->table_id;
    ecs_table_t *from_table = ecs_get_table(from_table_id);
    if (from_table->type.base == target) {
        return;
    }

    ecs_inheritance_plan_t plan;
    ecs_inheritance_plan_build(&from_table->type, target, &plan);
    ecs_type_t new_type = ecs_type_with_added_ids(
        &from_table->type,
        plan.ids,
        plan.count
    );
    new_type.base = target;
    uint16_t to_table_id = ecs_table_index_get_or_create(new_type);
    if (to_table_id == from_table_id) {
        ecs_inheritance_plan_fini(&plan);
        return;
    }

    from_table = ecs_get_table(from_table_id);
    ecs_migrate(record, entity, from_table, to_table_id, 0);
    ecs_table_t *to_table = ecs_get_table(to_table_id);
    ecs_inheritance_plan_copy(&plan, target, to_table, record->table_row);
    ecs_emit_added_components(from_table, to_table, entity, record->table_row);
    ecs_inheritance_plan_fini(&plan);
}

void ecs_is_a(ecs_entity_t entity, ecs_entity_t target) {
    ecs_assert_entity_valid(entity);
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(entity);
    ecs_assert_is_alive(target);

    if (ecs_is_deferred()) {
        if (!ecs_has_cid_owned(target, ecs_id(Abstract))) {
            ecs_add_cid(target, ecs_id(Abstract));
        }
        ecs_command_buffer_set_base(entity, target);
        return;
    }

    ecs_is_a_now(entity, target);
}

static inline void ecs_entity_index_kill(uint32_t entity_id) {
    ecs_entity_index_t *index = &ecs_world.entity_index;
    ecs_entity_record_t *record = ecs_entity_index_get_record(entity_id);
    record->generation += 1;
    record->table_row = index->first_available;
    record->table_id = UINT16_MAX;
    index->first_available = entity_id;
}

void ecs_kill_now(ecs_entity_t entity) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *initial_table = ecs_get_table(record->table_id);
    const ecs_component_t *components = initial_table->type.ids;
    uint16_t component_count = initial_table->type.component_count;
    ecs_table_t *table = initial_table;

    for (uint16_t i = 0; i < component_count && ecs_is_alive(entity); i++) {
        ecs_component_t component = components[i];
        record = ecs_get_record(entity);
        table = ecs_get_table(record->table_id);

        uint16_t col_idx = ecs_table_column_or_invalid(table, component);
        if (col_idx == UINT16_MAX) {
            continue;
        }

        void *removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);
        const ecs_component_record_t *crec = ecs_component_index_get(component);

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

    if (!ecs_is_alive(entity)) {
        return;
    }

    record = ecs_get_record(entity);
    table = ecs_get_table(record->table_id);

    // Remove from table
    ecs_table_remove_entity_update_record(table, entity, record->table_row, true);

    ecs_entity_index_kill(ecs_first(entity));
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

const char *ecs_entity_name(ecs_entity_t entity) {
    static char *buff = NULL;
    if (ecs_has(entity, Name)) {
        return ecs_get(entity, Name)->value;
    }
    if (!buff) {
        buff = calloc(20, sizeof(char));
    }
    sprintf(buff, "(%d, %d)", ecs_first(entity), ecs_second(entity));
    return buff;
}
