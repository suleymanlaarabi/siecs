#include "command_buffer.h"
#include "component_require.h"
#include "datastructure/arena.h"
#include "event_ops.h"
#include "inheritance.h"
#include "storage/component_index.h"
#include "storage/table_index.h"
#include "relation.h"
#include "table.h"
#include "table_migration.h"
#include "type.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ECS_COMMAND_NONE UINT32_MAX

#if defined(_MSC_VER)
#define ECS_THREAD_LOCAL __declspec(thread)
#else
#define ECS_THREAD_LOCAL _Thread_local
#endif
static ECS_THREAD_LOCAL ecs_execution_context_t *ecs_tls_context;

ecs_execution_context_t *ecs_execution_context_current(void) {
    return ecs_tls_context ? ecs_tls_context : &ecs_world.main_context;
}

void ecs_execution_context_set(ecs_execution_context_t *context) {
    ecs_tls_context = context;
}

static inline void deferred_change_fini(ecs_deferred_change_t *change) {
    if (!change->data) {
        return;
    }
    const ecs_component_record_t *record = ecs_component_index_get(change->id);
    if (record->info->size && record->ops.dtor) {
        record->ops.dtor(change->data, 1);
    }
    change->data = NULL;
}

static inline ecs_deferred_change_t *change_find(
    sicore_vec_t *changes,
    ecs_component_t id
) {
    ecs_deferred_change_t *items = sicore_vec_data(changes, ecs_deferred_change_t);
    for (uint32_t i = 0; i < changes->size; i++) {
        if (items[i].id == id) {
            return &items[i];
        }
    }
    return NULL;
}

static inline const ecs_deferred_change_t *change_find_const(
    const sicore_vec_t *changes,
    ecs_component_t id
) {
    const ecs_deferred_change_t *items =
        sicore_vec_data(changes, ecs_deferred_change_t);
    for (uint32_t i = 0; i < changes->size; i++) {
        if (items[i].id == id) {
            return &items[i];
        }
    }
    return NULL;
}

static inline ecs_deferred_change_t *change_add(
    ecs_entity_command_t *command,
    ecs_component_t id,
    ecs_deferred_op_t op
) {
    ecs_deferred_change_t change = { .id = id, .op = op };
    sicore_vec_push(&command->changes, &change, sizeof(change));
    return sicore_vec_get_mut(
        &command->changes,
        command->changes.size - 1,
        ecs_deferred_change_t
    );
}

static inline void command_init(ecs_entity_command_t *command, ecs_entity_t entity) {
    *command = (ecs_entity_command_t){ .entity = entity, .relation_head = ECS_COMMAND_NONE };
    sicore_vec_init(&command->changes, sizeof(ecs_deferred_change_t));
}

static inline void command_fini(ecs_entity_command_t *command) {
    ecs_deferred_change_t *changes =
        sicore_vec_data(&command->changes, ecs_deferred_change_t);
    for (uint32_t i = 0; i < command->changes.size; i++) {
        deferred_change_fini(&changes[i]);
    }
    sicore_vec_fini(&command->changes);
}

void ecs_command_buffer_init(ecs_command_buffer_t *buffer, ecs_arena_t *arena) {
    sicore_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));
    sicore_vec_init(&buffer->relations, sizeof(ecs_deferred_relation_t));
    buffer->entity_to_command = NULL;
    buffer->entity_capacity = 0;
    buffer->arena = arena;
}

void ecs_command_buffer_fini(ecs_command_buffer_t *buffer) {
    ecs_entity_command_t *commands = sicore_vec_data(&buffer->commands, ecs_entity_command_t);
    for (uint32_t i = 0; i < buffer->commands.size; i++) {
        command_fini(&commands[i]);
    }
    sicore_vec_fini(&buffer->commands);
    sicore_vec_fini(&buffer->relations);
    free(buffer->entity_to_command);
}

void ecs_execution_context_init(ecs_execution_context_t *context) {
    *context = (ecs_execution_context_t){ 0 };
    ecs_arena_init(&context->arena);
    ecs_command_buffer_init(&context->commands, &context->arena);
}

void ecs_execution_context_fini(ecs_execution_context_t *context) {
    ecs_command_buffer_fini(&context->commands);
    ecs_arena_fini(&context->arena);
}

static void command_buffer_ensure_entity(ecs_command_buffer_t *buffer, uint32_t entity_id) {
    if (entity_id < buffer->entity_capacity) {
        return;
    }

    uint32_t new_capacity = buffer->entity_capacity ? buffer->entity_capacity : 256;
    while (new_capacity <= entity_id) {
        new_capacity *= 2;
    }

    buffer->entity_to_command = realloc(buffer->entity_to_command, sizeof(uint32_t) * new_capacity);
    for (uint32_t i = buffer->entity_capacity; i < new_capacity; i++) {
        buffer->entity_to_command[i] = ECS_COMMAND_NONE;
    }
    buffer->entity_capacity = new_capacity;
}

static ecs_entity_command_t *command_for_entity(
    ecs_command_buffer_t *buffer,
    ecs_entity_t entity
) {
    uint32_t entity_id = ecs_first(entity);
    command_buffer_ensure_entity(buffer, entity_id);

    uint32_t command_index = buffer->entity_to_command[entity_id];
    if (command_index != ECS_COMMAND_NONE) {
        return sicore_vec_get_mut(&buffer->commands, command_index, ecs_entity_command_t);
    }

    command_index = buffer->commands.size;
    ecs_entity_command_t *command =
        sicore_vec_push_empty(&buffer->commands, sizeof(ecs_entity_command_t));
    command_init(command, entity);
    buffer->entity_to_command[entity_id] = command_index;
    return command;
}

void ecs_command_buffer_add(ecs_entity_t entity, ecs_component_t id) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    ecs_deferred_change_t *change = change_find(&command->changes, id);
    if (!change) {
        change_add(command, id, EcsDeferredAdd);
    } else if (change->op == EcsDeferredRemove) {
        change->op = EcsDeferredAdd;
    }
}

void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    ecs_deferred_change_t *change = change_find(&command->changes, id);
    if (!change) {
        change_add(command, id, EcsDeferredRemove);
        return;
    }
    deferred_change_fini(change);
    change->op = EcsDeferredRemove;
}

void ecs_command_buffer_set(ecs_entity_t entity, ecs_component_t id, const void *data) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    const ecs_component_record_t *record = ecs_component_index_get(id);

    uint32_t size = record->info->size ? record->info->size : 1;
    ecs_deferred_change_t *change = change_find(&command->changes, id);
    if (!change) {
        change = change_add(command, id, EcsDeferredCopy);
    } else {
        deferred_change_fini(change);
        change->op = EcsDeferredCopy;
    }
    change->data = ecs_arena_alloc(buffer->arena, size);
    if (record->info->size) {
        if (record->ops.copy_ctor) {
            record->ops.copy_ctor(change->data, data, 1);
        } else {
            memcpy(change->data, data, record->info->size);
        }
    }
}

void ecs_command_buffer_move(ecs_entity_t entity, ecs_component_t id, void *data) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    const ecs_component_record_t *record = ecs_component_index_get(id);

    uint32_t size = record->info->size ? record->info->size : 1;
    ecs_deferred_change_t *change = change_find(&command->changes, id);
    if (!change) {
        change = change_add(command, id, EcsDeferredMove);
    } else {
        deferred_change_fini(change);
        change->op = EcsDeferredMove;
    }
    change->data = ecs_arena_alloc(buffer->arena, size);
    if (record->info->size) {
        if (record->ops.move_ctor) {
            record->ops.move_ctor(change->data, data, 1);
        } else if (record->ops.copy_ctor) {
            record->ops.copy_ctor(change->data, data, 1);
            if (record->ops.dtor) {
                record->ops.dtor(data, 1);
            }
        } else {
            memcpy(change->data, data, record->info->size);
        }
    }
}

void ecs_command_buffer_kill(ecs_entity_t entity) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    command->kill = true;
    command->has_base = false;
    ecs_deferred_change_t *changes =
        sicore_vec_data(&command->changes, ecs_deferred_change_t);
    for (uint32_t i = 0; i < command->changes.size; i++) {
        deferred_change_fini(&changes[i]);
    }
    sicore_vec_clear(&command->changes);
    command->relation_head = ECS_COMMAND_NONE;
}

void ecs_command_buffer_set_base(ecs_entity_t entity, ecs_entity_t target) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    command->has_base = true;
    command->base = target;
}

void ecs_command_buffer_relate(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_entity_t target
) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    uint32_t index = command->relation_head;
    while (index != ECS_COMMAND_NONE) {
        ecs_deferred_relation_t *entry =
            sicore_vec_get_mut(&buffer->relations, index, ecs_deferred_relation_t);
        if (entry->id == relation) {
            entry->target = target;
            return;
        }
        index = entry->next;
    }
    ecs_deferred_relation_t value = {
        .target = target,
        .next = command->relation_head,
        .id = relation,
    };
    command->relation_head = buffer->relations.size;
    sicore_vec_push(&buffer->relations, &value, sizeof value);
}

static void final_ids_push_sorted(sicore_vec_t *final_ids, ecs_component_t id) {
    ecs_component_t *ids = sicore_vec_data(final_ids, ecs_component_t);
    uint32_t i = 0;
    while (i < final_ids->size && ids[i] < id) {
        i++;
    }
    if (i < final_ids->size && ids[i] == id) {
        return;
    }

    sicore_vec_push_empty(final_ids, sizeof(ecs_component_t));
    ids = sicore_vec_data(final_ids, ecs_component_t);
    for (uint32_t j = final_ids->size - 1; j > i; j--) {
        ids[j] = ids[j - 1];
    }
    ids[i] = id;
}

static ecs_type_t
command_build_type(
    const ecs_table_t *table,
    const ecs_entity_command_t *command
) {
    sicore_vec_t final_ids;
    sicore_vec_init(&final_ids, sizeof(ecs_component_t));
    const ecs_deferred_change_t *changes =
        sicore_vec_data(&command->changes, ecs_deferred_change_t);

    for (uint16_t i = 0; i < table->type.component_count; i++) {
        ecs_component_t id = table->type.ids[i];
        const ecs_deferred_change_t *change = change_find_const(&command->changes, id);
        if (!change || change->op != EcsDeferredRemove) {
            sicore_vec_push_u16(&final_ids, id);
        }
    }

    for (uint32_t i = 0; i < command->changes.size; i++) {
        if (changes[i].op == EcsDeferredRemove) {
            continue;
        }
        ecs_component_t required[ECS_ADD_PLAN_MAX_COMPONENTS];
        uint16_t required_count = 0;
        ecs_collect_required_components(
            table,
            changes[i].id,
            required,
            &required_count,
            ECS_ADD_PLAN_MAX_COMPONENTS
        );
        for (uint16_t r = 0; r < required_count; r++) {
            final_ids_push_sorted(&final_ids, required[r]);
        }
        final_ids_push_sorted(&final_ids, changes[i].id);
    }

    ecs_type_t type = ecs_type_with_ids(
        &table->type,
        sicore_vec_data(&final_ids, ecs_component_t),
        (uint16_t)final_ids.size
    );
    sicore_vec_fini(&final_ids);
    type.base = command->has_base ? command->base : table->type.base;
    return type;
}

static bool command_type_unchanged(const ecs_table_t *table, const ecs_entity_command_t *command) {
    const ecs_deferred_change_t *changes =
        sicore_vec_data(&command->changes, ecs_deferred_change_t);
    if (command->has_base && command->base != table->type.base) {
        return false;
    }

    for (uint32_t i = 0; i < command->changes.size; i++) {
        if (changes[i].op == EcsDeferredRemove ||
            !ecs_table_has_owned(table, changes[i].id)) {
            return false;
        }
    }
    return true;
}

static void command_apply_changes(ecs_entity_command_t *command) {
    ecs_deferred_change_t *changes =
        sicore_vec_data(&command->changes, ecs_deferred_change_t);
    for (uint32_t i = 0; i < command->changes.size && ecs_is_alive(command->entity); i++) {
        if (changes[i].op != EcsDeferredCopy && changes[i].op != EcsDeferredMove) {
            continue;
        }
        ecs_component_t id = changes[i].id;
        const ecs_component_record_t *record = ecs_component_index_get(id);
        ecs_entity_record_t *entity_record = ecs_get_record(command->entity);
        ecs_table_t *table = ecs_get_table(entity_record->table_id);
        uint16_t column = ecs_table_get_column_index(table, id);
        void *dst = ecs_table_component_at_column(table, column, entity_record->table_row);

        if (record->on_set) {
            record->on_set(command->entity, id, changes[i].data, dst);
            if (!ecs_is_alive(command->entity)) {
                return;
            }
            entity_record = ecs_get_record(command->entity);
            table = ecs_get_table(entity_record->table_id);
            column = ecs_table_get_column_index(table, id);
            dst = ecs_table_component_at_column(table, column, entity_record->table_row);
        }
        ecs_emit(table, command->entity, EcsOnSet, changes[i].data);
        if (record->info->size) {
            if (record->ops.move) {
                record->ops.move(dst, changes[i].data, 1);
            } else if (record->ops.copy) {
                record->ops.copy(dst, changes[i].data, 1);
                if (record->ops.dtor) {
                    record->ops.dtor(changes[i].data, 1);
                }
            } else {
                memcpy(dst, changes[i].data, record->info->size);
            }
        }
        changes[i].data = NULL;
    }
}

static void command_apply_relations(
    ecs_entity_command_t *command,
    const sicore_vec_t *relations
) {
    uint32_t index = command->relation_head;
    while (index != ECS_COMMAND_NONE && ecs_is_alive(command->entity)) {
        const ecs_deferred_relation_t *entry =
            sicore_vec_get(relations, index, ecs_deferred_relation_t);
        if (entry->target) {
            ecs_relate_id_now(command->entity, entry->id, entry->target);
        } else {
            ecs_unrelate_id_now(command->entity, entry->id);
        }
        index = entry->next;
    }
}

static void command_apply(ecs_entity_command_t *command, const sicore_vec_t *relations) {
    if (!ecs_is_alive(command->entity)) {
        return;
    }

    if (command->kill) {
        ecs_kill_now(command->entity);
        return;
    }

    ecs_entity_record_t *record = ecs_get_record(command->entity);
    uint16_t old_table_id = record->table_id;
    ecs_table_t *old_table = ecs_get_table(old_table_id);
    if (command_type_unchanged(old_table, command)) {
        command_apply_changes(command);
        command_apply_relations(command, relations);
        return;
    }

    ecs_type_t final_type = command_build_type(old_table, command);
    ecs_inheritance_plan_t inheritance_plan = { 0 };
    bool base_changed = command->has_base && command->base != old_table->type.base;
    if (base_changed) {
        ecs_inheritance_plan_build(&final_type, command->base, &inheritance_plan);
        if (inheritance_plan.count != 0) {
            ecs_type_t materialized = ecs_type_with_added_ids(
                &final_type,
                inheritance_plan.ids,
                inheritance_plan.count
            );
            materialized.base = final_type.base;
            ecs_type_fini(&final_type);
            final_type = materialized;
        }
    }

    if (!ecs_type_equals(&old_table->type, &final_type)) {
        uint32_t old_row = record->table_row;
        ecs_emit_removed_components(old_table, &final_type, command->entity, old_row);
        if (!ecs_is_alive(command->entity)) {
            ecs_type_fini(&final_type);
            ecs_inheritance_plan_fini(&inheritance_plan);
            return;
        }

        uint16_t new_table_id = ecs_table_index_get_or_create(final_type);
        record = ecs_get_record(command->entity);
        old_table = ecs_get_table(old_table_id);
        ecs_migrate(record, command->entity, old_table, new_table_id, 0);
        record = ecs_get_record(command->entity);
        ecs_table_t *new_table = ecs_get_table(record->table_id);
        if (base_changed) {
            ecs_inheritance_plan_copy(
                &inheritance_plan,
                command->base,
                new_table,
                record->table_row
            );
        }
        if (ecs_emit_added_components(old_table, new_table, command->entity, record->table_row)) {
            ecs_apply_added_component_default_relations(
                old_table,
                new_table,
                command->entity
            );
        }
    } else {
        ecs_type_fini(&final_type);
    }

    ecs_inheritance_plan_fini(&inheritance_plan);

    command_apply_changes(command);
    command_apply_relations(command, relations);
}

void ecs_command_buffer_flush_buffer(ecs_command_buffer_t *buffer) {
    if (buffer->commands.size == 0) {
        ecs_arena_reset(buffer->arena);
        return;
    }

    while (buffer->commands.size != 0) {
        sicore_vec_t commands = buffer->commands;
        sicore_vec_t relations = buffer->relations;
        sicore_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));
        sicore_vec_init(&buffer->relations, sizeof(ecs_deferred_relation_t));

        ecs_entity_command_t *items = sicore_vec_data(&commands, ecs_entity_command_t);
        for (uint32_t i = 0; i < commands.size; i++) {
            uint32_t entity_id = ecs_first(items[i].entity);
            buffer->entity_to_command[entity_id] = ECS_COMMAND_NONE;
        }

        for (uint32_t i = 0; i < commands.size; i++) {
            command_apply(&items[i], &relations);
            command_fini(&items[i]);
        }
        sicore_vec_fini(&commands);
        sicore_vec_fini(&relations);
    }
    ecs_arena_reset(buffer->arena);
}

void ecs_command_buffer_flush() {
    ecs_execution_context_t *context = ecs_execution_context_current();
    if (context->flushing_commands) {
        ecs_command_buffer_flush_buffer(&context->commands);
        return;
    }
    context->flushing_commands = true;
    ecs_command_buffer_flush_buffer(&context->commands);
    context->flushing_commands = false;
}

void ecs_defer_begin(void) {
    ecs_execution_context_current()->defer_depth++;
}

void ecs_defer_end(void) {
    ecs_execution_context_t *context = ecs_execution_context_current();
    ecs_assert(context->defer_depth > 0, "ecs_defer_end called without ecs_defer_begin\n");
    context->defer_depth--;
    if (context->defer_depth == 0 && !context->scheduler_parallel) {
        ecs_command_buffer_flush();
    }
}
