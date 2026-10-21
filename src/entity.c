#include "command_buffer.h"
#include "event_ops.h"
#include "helper.h"
#include "inheritance.h"
#include "relation.h"
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

void ecs_entity_materialize(ecs_entity_t entity) {
    ecs_entity_record_t *record = ecs_get_record(entity);
    if (record->table_row != UINT32_MAX || ecs_entity_record_table(record) != 0)
        return;
    ecs_table_t *table = ecs_get_table(0);
    record->table_row = table->entity_count;
    ecs_table_add_entity(table, entity);
}

static ecs_entity_t ecs_entity_create(bool reuse) {
    ecs_entity_t entity =
        ecs_entity_index_reserve(reuse, ecs_execution_context_current()->entity_reservation);
    if (ecs_execution_context_current()->scheduler_parallel) {
        ecs_command_buffer_create(entity);
    } else {
        ecs_entity_materialize(entity);
    }
    return entity;
}

ecs_entity_t ecs_new(void) { return ecs_entity_create(true); }

ecs_entity_t ecs_new_no_reuse(void) { return ecs_entity_create(false); }

bool ecs_is_alive(const ecs_entity_t entity) { return ecs_entity_index_is_alive(entity); }

ecs_entity_t ecs_entity_from_index(uint32_t index) {
    if (index == 0 || index >= ecs_entity_index_count()) {
        return 0;
    }

    const ecs_entity_record_t *record = ecs_entity_index_get_record(index);
    return record && atomic_load_explicit(&record->table_id, memory_order_acquire) != UINT16_MAX
               ? ecs_entity(index, record->generation)
               : 0;
}

static inline bool ecs_would_create_base_cycle(const ecs_entity_t entity, ecs_entity_t target) {
    while (target != 0) {
        if (target == entity) {
            return true;
        }
        const ecs_entity_record_t *target_record = ecs_get_record(target);
        const ecs_table_t *target_table = ecs_get_table(ecs_entity_record_table(target_record));
        target = ecs_type_isa_target(&target_table->type);
    }
    return false;
}

bool ecs_is(ecs_entity_t entity, ecs_entity_t target) {
    ecs_entity_t base =
        ecs_type_isa_target(&ecs_get_table(ecs_entity_record_table(ecs_get_record(entity)))->type);
    if (base == target) {
        return true;
    }
    if (base == 0) {
        return false;
    }
    return ecs_is(base, target);
}

ecs_entity_t ecs_entity_base_raw(ecs_entity_t entity) {
    ecs_assert_is_alive(entity);
    return ecs_type_isa_target(
        &ecs_get_table(ecs_entity_record_table(ecs_get_record(entity)))->type
    );
}

ecs_entity_t ecs_entity_base(ecs_entity_t entity) { return ecs_target_id(entity, ecs_rid(IsA)); }

void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target) {
    if (target) {
        ecs_fatal_if(
            ecs_would_create_base_cycle(entity, target),
            "cyclic inheritance: %d inherits from %d\n",
            ecs_first(entity),
            ecs_first(target)
        );
        ecs_abstract_now(target);
    }

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_table_id = ecs_entity_record_table(record);
    ecs_table_t *from_table = ecs_get_table(from_table_id);
    if (ecs_type_isa_target(&from_table->type) == target) {
        return;
    }

    ecs_inheritance_plan_t plan;
    ecs_type_t new_type;
    if (target) {
        new_type = ecs_type_with(
            &from_table->type,
            0,
            (ecs_type_pair_t){ .key = ecs_rid(IsA), .value = target }
        );
    } else {
        ecs_assert(
            ecs_type_pair_index(&from_table->type, ecs_rid(IsA)) != UINT16_MAX,
            "missing IsA pair\n"
        );
        new_type = ecs_type_without(&from_table->type, UINT16_MAX, ecs_rid(IsA));
    }
    new_type = ecs_inheritance_materialize(new_type, target, &plan);
    ecs_table_t *to_table = ecs_table_change_type(record, entity, new_type, false);
    from_table = ecs_get_table(from_table_id);
    ecs_inheritance_plan_copy(&plan, target, to_table, record->table_row);
    ecs_emit_added_components(from_table, to_table, entity, record->table_row);
    ecs_inheritance_plan_fini(&plan);

    if (target) {
        ecs_inheritance_instantiate_children(entity, target);
    }
}

void ecs_is_a(ecs_entity_t entity, ecs_entity_t target) {
    ecs_relate_id(entity, ecs_rid(IsA), target);
}

static inline void ecs_entity_index_kill(uint32_t entity_id) {
    ecs_entity_record_t *record = ecs_entity_index_get_record(entity_id);
    record->generation += 1;
    record->table_row = UINT32_MAX;
    atomic_store_explicit(&record->table_id, UINT16_MAX, memory_order_relaxed);
    sicore_vec_push(&entity_index.available, &entity_id, sizeof entity_id);
}

void ecs_kill_now(ecs_entity_t entity) {
    if (relation_index.virtual_scan_count || ecs_table_index_may_be_pair_target(entity))
        ecs_relation_virtual_target_on_remove(entity);
    if (!ecs_is_alive(entity)) {
        return;
    }

    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *initial_table = ecs_get_table(ecs_entity_record_table(record));
    const ecs_component_t *components = initial_table->type.ids;
    uint16_t component_count = initial_table->type.component_count;
    ecs_table_t *table = initial_table;

    /* No hook or observer can see the removals: skip the per-component pass
     * that would only find nothing to notify. */
    if (!(table->add_edge.aux_flags & EcsTableHasOnRemoveHooks) &&
        !ecs_table_observed(table, EcsOnRemove))
        component_count = 0;

    for (uint16_t i = 0; i < component_count && ecs_is_alive(entity); i++) {
        ecs_component_t component = components[i];
        record = ecs_get_record(entity);
        table = ecs_get_table(ecs_entity_record_table(record));

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
            table = ecs_get_table(ecs_entity_record_table(record));
            col_idx = ecs_table_column_or_invalid(table, component);
            if (col_idx == UINT16_MAX) {
                continue;
            }
            removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);
        }
        ecs_emit(table, entity, EcsOnRemove, component, removed_data);
    }

    if (!ecs_is_alive(entity)) {
        return;
    }

    record = ecs_get_record(entity);
    table = ecs_get_table(ecs_entity_record_table(record));

    // Remove from table
    ecs_table_remove_entity_update_record(table, entity, record->table_row, true);

    if (observer_index.target_keys.size)
        ecs_observer_fini_entity(entity);
    ecs_entity_index_kill(ecs_first(entity));
}

void ecs_kill(ecs_entity_t entity) {
    ecs_assert_entity_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_kill(entity);
        return;
    }

    ecs_kill_now(entity);
}

const char *ecs_entity_name(ecs_entity_t entity) {
#ifdef _MSC_VER
    static __declspec(thread) char buff[32];
#else
    static _Thread_local char buff[32];
#endif
    if (!ecs_name_entity_alive(entity))
        return NULL;
    const char *name = ecs_name_local(entity);
    if (name)
        return name;
    snprintf(buff, sizeof buff, "(%u, %u)", ecs_first(entity), ecs_second(entity));
    return buff;
}
