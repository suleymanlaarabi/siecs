#include "relation.h"
#include "command_buffer.h"
#include "storage/component_index.h"
#include "storage/table_index.h"
#include "table_migration.h"
#include "type.h"
#include "utils.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

void ecs_relation_index_init(void) {
    sicore_vec_init_w_size(&ecs_world.relation_index.records, sizeof(ecs_relation_record_t), 1);
    sicore_vec_ensure(&ecs_world.relation_index.records, 1, sizeof(ecs_relation_record_t));
}

void ecs_relation_index_fini(void) {
    ecs_relation_index_t *index = &ecs_world.relation_index;
    ecs_relation_record_t *records = index->records.data;
    for (uint32_t r = 1; r < ecs_world.relation_index.records.size; r++) {
        free(records[r].name);
    }
    sicore_vec_fini(&ecs_world.relation_index.records);
}

ecs_relation_id_t
ecs_relation_register(ecs_relation_id_t *id, const char *name, const ecs_relation_desc_t *desc) {
    ecs_assert_not_null(id);
    ecs_assert_not_null(desc);
    ecs_assert(
        desc->storage == EcsRelationDense || desc->storage == EcsRelationByDepth ||
            desc->storage == EcsRelationByTarget,
        "invalid relation storage\n"
    );
    ecs_assert(
        desc->storage != EcsRelationByDepth || desc->acyclic,
        "ByDepth relation must be acyclic\n"
    );

    if (*id) {
        sicore_vec_ensure(
            &ecs_world.relation_index.records,
            (uint32_t)*id + 1,
            sizeof(ecs_relation_record_t)
        );
        ecs_relation_record_t *existing =
            sicore_vec_get_mut(&ecs_world.relation_index.records, *id, ecs_relation_record_t);
        if (existing->storage || existing->component) {
            return *id;
        }
    } else {
        *id = (ecs_relation_id_t)ecs_world.relation_index.records.size;
    }

    sicore_vec_ensure(
        &ecs_world.relation_index.records,
        (uint32_t)*id + 1,
        sizeof(ecs_relation_record_t)
    );
    ecs_component_t component =
        ecs_component_register_relation_internal(name, *id, desc->storage == EcsRelationByTarget);
    *sicore_vec_get_mut(&ecs_world.relation_index.records, *id, ecs_relation_record_t) =
        (ecs_relation_record_t){
            .component = component,
            .storage = desc->storage,
            .on_delete_target = desc->on_delete_target,
            .acyclic = desc->storage == EcsRelationByDepth || desc->acyclic,
            .name = name ? strdup(name) : NULL,
        };
    return *id;
}

ecs_relation_id_t ecs_relation_init(const char *name, const ecs_relation_desc_t *desc) {
    ecs_relation_id_t id = 0;
    return ecs_relation_register(&id, name, desc);
}

static ecs_entity_t
ecs_relation_table_target(const ecs_table_t *table, ecs_relation_id_t relation) {
    uint16_t index = ecs_type_relation_index(&table->type, relation);
    return index == UINT16_MAX ? 0
                               : ecs_relation_key_target(&ecs_type_relations(&table->type)[index]);
}

ecs_entity_t
ecs_relation_target_at_table(const ecs_table_t *table, ecs_relation_id_t relation, uint32_t row) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    if (record->storage == EcsRelationByTarget) {
        return ecs_relation_table_target(table, relation);
    }
    uint16_t column = ecs_table_column_or_invalid(table, record->component);
    if (column == UINT16_MAX) {
        return 0;
    }
    const RelationTarget *value = ecs_table_component_at_column(table, column, row);
    return value->target;
}

uint32_t ecs_relation_table_depth(const ecs_table_t *table, ecs_relation_id_t relation) {
    return (uint32_t)ecs_relation_table_target(table, relation);
}

static uint32_t ecs_relation_depth(ecs_entity_t entity, ecs_relation_id_t relation) {
    const ecs_entity_record_t *entity_record = ecs_get_record(entity);
    const ecs_table_t *table = ecs_get_table(entity_record->table_id);
    return (uint32_t)ecs_relation_table_target(table, relation);
}

static void ecs_emit_relation_event(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_event_t event,
    ecs_entity_t old_target,
    ecs_entity_t new_target
) {
    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    ecs_relation_event_t relation_event = {
        .relation = relation,
        .old_target = old_target,
        .new_target = new_target,
    };
    ecs_emit(table, entity, event, &relation_event);
}

#ifndef NDEBUG
static bool
ecs_relation_would_cycle(ecs_entity_t source, ecs_relation_id_t relation, ecs_entity_t target) {
    while (target) {
        if (target == source) {
            return true;
        }
        target = ecs_target_id(target, relation);
    }
    return false;
}
#endif

static void
ecs_relation_migrate_depth(ecs_entity_t entity, ecs_relation_id_t relation, uint32_t depth) {
    ecs_entity_record_t *entity_record = ecs_get_record(entity);
    uint16_t from_id = entity_record->table_id;
    ecs_table_t *from = ecs_get_table(from_id);
    ecs_type_t type = ecs_type_with_relation(&from->type, relation, depth);
    uint16_t to_id = ecs_table_index_get_or_create(type);
    if (to_id != from_id) {
        from = ecs_get_table(from_id);
        ecs_migrate_same_layout(entity_record, entity, from, to_id);
    }
}

static void ecs_relation_update_children_depth(
    ecs_entity_t parent,
    ecs_relation_id_t relation,
    uint32_t parent_depth
) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    RelationSource *source = ecs_try_get_cid(parent, record->component + 1);
    uint32_t count = source ? source->entities.size : 0;
    for (uint32_t i = 0; i < count; i++) {
        source = ecs_get_cid(parent, record->component + 1);
        ecs_entity_t child = *sicore_vec_get(&source->entities, i, ecs_entity_t);
        ecs_relation_migrate_depth(child, relation, parent_depth + 1);
        ecs_relation_update_children_depth(child, relation, parent_depth + 1);
    }
}

static void ecs_relation_set_dense(
    ecs_entity_t entity,
    const ecs_relation_record_t *record,
    ecs_entity_t target
) {
    RelationTarget value = { .target = target };
    ecs_set_cid(entity, record->component, &value);
}

static void ecs_relation_set_depth(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    const ecs_relation_record_t *relation_record,
    ecs_entity_t target
) {
    ecs_entity_t old_target = ecs_target_id(entity, relation);
    if (old_target == target) {
        return;
    }
    uint32_t depth = ecs_relation_depth(target, relation) + 1;
    ecs_entity_record_t *entity_record = ecs_get_record(entity);
    uint16_t from_id = entity_record->table_id;
    ecs_table_t *from = ecs_get_table(from_id);
    bool had_relation = ecs_table_column_or_invalid(from, relation_record->component) != UINT16_MAX;

    ecs_type_t type;
    if (had_relation) {
        type = ecs_type_with_relation(&from->type, relation, depth);
    } else {
        type = ecs_type_with_component_relation(
            &from->type,
            relation_record->component,
            relation,
            depth
        );
    }
    uint16_t to_id = ecs_table_index_get_or_create(type);
    if (to_id != from_id) {
        from = ecs_get_table(from_id);
        if (had_relation) {
            ecs_migrate_same_layout(entity_record, entity, from, to_id);
        } else {
            ecs_migrate_to_table(entity_record, entity, from, to_id);
        }
    }

    ecs_table_t *table = ecs_get_table(ecs_get_record(entity)->table_id);
    RelationTarget *current = ecs_table_get_component(
        table,
        relation_record->component,
        ecs_get_record(entity)->table_row
    );
    const ecs_component_record_t *component = ecs_component_index_get(relation_record->component);
    RelationTarget value = { .target = target };
    component->on_set(entity, relation_record->component, &value, current);
    *current = value;
    ecs_relation_update_children_depth(entity, relation, depth);
}

static void
ecs_relation_set_target(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    if (!ecs_has_cid_owned(target, record->component)) {
        ecs_add_cid_now(target, record->component);
    }
    ecs_entity_record_t *entity_record = ecs_get_record(entity);
    uint16_t from_id = entity_record->table_id;
    ecs_table_t *from = ecs_get_table(from_id);
    ecs_type_t type = ecs_type_with_relation(&from->type, relation, target);
    uint16_t to_id = ecs_table_index_get_or_create(type);
    if (to_id != from_id) {
        from = ecs_get_table(from_id);
        ecs_migrate_same_layout(entity_record, entity, from, to_id);
    }
}

void ecs_relate_id_now(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target) {
    ecs_assert_entity_valid(entity);
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(entity);
    ecs_assert_is_alive(target);
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_assert(
        !record->acyclic || !ecs_relation_would_cycle(entity, relation, target),
        "cyclic relation\n"
    );

    bool had_relation = ecs_has_relation_id(entity, relation);
    ecs_entity_t old_target = had_relation ? ecs_target_id(entity, relation) : 0;
    if (had_relation && old_target == target) {
        return;
    }

    if (record->storage == EcsRelationDense) {
        ecs_relation_set_dense(entity, record, target);
    } else if (record->storage == EcsRelationByDepth) {
        ecs_relation_set_depth(entity, relation, record, target);
    } else {
        ecs_relation_set_target(entity, relation, target);
    }

    ecs_emit_relation_event(entity, relation, EcsOnRelationSet, old_target, target);
}

void ecs_relate_id(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target) {
    if (ecs_is_deferred()) {
        ecs_command_buffer_relate(entity, relation, target);
        return;
    }
    ecs_relate_id_now(entity, relation, target);
}

static void ecs_relation_remove_depth(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    const ecs_relation_record_t *relation_record
) {
    ecs_entity_record_t *entity_record = ecs_get_record(entity);
    uint16_t from_id = entity_record->table_id;
    ecs_table_t *from = ecs_get_table(from_id);
    uint16_t column = ecs_table_column_or_invalid(from, relation_record->component);
    if (column == UINT16_MAX) {
        return;
    }

    RelationTarget *value = ecs_table_component_at_column(from, column, entity_record->table_row);
    ecs_component_index_get(relation_record->component)
        ->on_remove(entity, relation_record->component, value);

    entity_record = ecs_get_record(entity);
    from_id = entity_record->table_id;
    from = ecs_get_table(from_id);
    column = ecs_table_column_or_invalid(from, relation_record->component);
    ecs_type_t type = ecs_type_without_component_relation(&from->type, column, relation);
    uint16_t to_id = ecs_table_index_get_or_create(type);
    from = ecs_get_table(from_id);
    ecs_migrate_to_table(entity_record, entity, from, to_id);
    ecs_relation_update_children_depth(entity, relation, 0);
}

static void ecs_relation_remove_target(ecs_entity_t entity, ecs_relation_id_t relation) {
    ecs_entity_record_t *entity_record = ecs_get_record(entity);
    uint16_t from_id = entity_record->table_id;
    ecs_table_t *from = ecs_get_table(from_id);
    if (ecs_type_relation_index(&from->type, relation) == UINT16_MAX) {
        return;
    }
    ecs_type_t type = ecs_type_without_relation(&from->type, relation);
    uint16_t to_id = ecs_table_index_get_or_create(type);
    from = ecs_get_table(from_id);
    ecs_migrate_same_layout(entity_record, entity, from, to_id);
}

void ecs_unrelate_id_now(ecs_entity_t entity, ecs_relation_id_t relation) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    bool had_relation = ecs_has_relation_id(entity, relation);
    if (!had_relation) {
        return;
    }
    ecs_entity_t old_target = ecs_target_id(entity, relation);
    ecs_emit_relation_event(entity, relation, EcsOnRelationRemove, old_target, 0);
    if (!ecs_is_alive(entity) || !ecs_has_relation_id(entity, relation) ||
        ecs_target_id(entity, relation) != old_target) {
        return;
    }

    const ecs_relation_record_t *record = ecs_relation_record(relation);
    if (record->storage == EcsRelationDense) {
        ecs_remove_cid(entity, record->component);
    } else if (record->storage == EcsRelationByDepth) {
        ecs_relation_remove_depth(entity, relation, record);
    } else {
        ecs_relation_remove_target(entity, relation);
    }
}

void ecs_unrelate_id(ecs_entity_t entity, ecs_relation_id_t relation) {
    if (ecs_is_deferred()) {
        ecs_command_buffer_relate(entity, relation, 0);
        return;
    }
    ecs_unrelate_id_now(entity, relation);
}

bool ecs_has_relation_id(ecs_entity_t entity, ecs_relation_id_t relation) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    const ecs_table_t *table = ecs_get_table(ecs_get_record(entity)->table_id);
    if (record->storage != EcsRelationByTarget) {
        return ecs_table_column_or_invalid(table, record->component) != UINT16_MAX;
    }
    return ecs_type_relation_index(&table->type, relation) != UINT16_MAX;
}

ecs_entity_t ecs_target_id(ecs_entity_t entity, ecs_relation_id_t relation) {
    const ecs_entity_record_t *entity_record = ecs_get_record(entity);
    const ecs_table_t *table = ecs_get_table(entity_record->table_id);
    return ecs_relation_target_at_table(table, relation, entity_record->table_row);
}

bool ecs_has_relation_to_id(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target) {
    return ecs_target_id(entity, relation) == target;
}

void ecs_relation_target_on_remove(ecs_entity_t target, ecs_component_t component, void *ptr) {
    (void)ptr;
    ecs_relation_id_t relation =
        ECS_COMPONENT_RELATION_ID(ecs_component_index_get(component)->relation_flags);
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_relation_tables_t tables = ecs_table_index_relation_tables(relation, target);
    if (!tables.count) {
        return;
    }

    for (uint16_t i = 0; i < tables.count; i++) {
        ecs_table_t *table = ecs_get_table(tables.ids[i]);
        while (table->entity_count) {
            uint32_t row = table->entity_count - 1;
            ecs_entity_t source = table->entities[row];
            if (source == target) {
                if (row == 0) {
                    break;
                }
                source = table->entities[0];
            }
            if (record->on_delete_target == EcsDeleteSources) {
                ecs_kill_now(source);
            } else {
                ecs_unrelate_id_now(source, relation);
            }
        }
    }
}
