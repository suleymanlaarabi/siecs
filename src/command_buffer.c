#include "command_buffer.h"
#include "datastructure/arena.h"
#include "event_ops.h"
#include "inheritance.h"
#include "relation.h"
#include "storage/component_index.h"
#include "storage/table_index.h"
#include "table.h"
#include "table_migration.h"
#include "type.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdlib.h>

#define ECS_COMMAND_NONE UINT32_MAX
#define ECS_COMMAND_TRANSITION_CHANGE_CAPACITY 4

typedef struct {
    const ecs_component_record_t *record;
    const void *source;
    uint16_t column;
} ecs_command_inherited_copy_t;

typedef struct {
    ecs_type_t type;
    ecs_inheritance_plan_t inheritance;
    ecs_command_inherited_copy_t *inherited_copies;
    ecs_entity_t base;
    ecs_component_t ids[ECS_COMMAND_TRANSITION_CHANGE_CAPACITY];
    ecs_deferred_op_t ops[ECS_COMMAND_TRANSITION_CHANGE_CAPACITY];
    uint32_t change_count;
    uint16_t from_table;
    uint16_t to_table;
    bool has_base;
    bool valid;
} ecs_command_transition_t;

#if defined(_MSC_VER)
#define ECS_THREAD_LOCAL __declspec(thread)
#else
#define ECS_THREAD_LOCAL _Thread_local
#endif
static ECS_THREAD_LOCAL ecs_execution_context_t *ecs_tls_context;

ecs_execution_context_t *ecs_execution_context_current(void) {
    return ecs_tls_context ? ecs_tls_context : &ecs_world.main_context;
}

void ecs_execution_context_set(ecs_execution_context_t *context) { ecs_tls_context = context; }

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

static inline ecs_deferred_change_t *command_changes(ecs_entity_command_t *command) {
    return command->changes;
}

static bool command_transition_matches(
    const ecs_command_transition_t *transition,
    const ecs_entity_command_t *command,
    uint16_t from_table
) {
    if (!transition->valid || transition->from_table != from_table ||
        transition->has_base != command->has_base || transition->base != command->base ||
        transition->change_count != command->change_count) {
        return false;
    }
    const ecs_deferred_change_t *changes = command->changes;
    for (uint32_t i = 0; i < command->change_count; i++) {
        if (transition->ids[i] != changes[i].id || transition->ops[i] != changes[i].op) {
            return false;
        }
    }
    return true;
}

static bool
command_shape_equals(const ecs_entity_command_t *left, const ecs_entity_command_t *right) {
    if (left->change_count != right->change_count || left->has_base != right->has_base ||
        left->base != right->base) {
        return false;
    }
    for (uint32_t i = 0; i < left->change_count; i++) {
        if (left->changes[i].id != right->changes[i].id ||
            left->changes[i].op != right->changes[i].op) {
            return false;
        }
    }
    return true;
}

static void command_transition_set(
    ecs_command_transition_t *transition,
    const ecs_entity_command_t *command,
    uint16_t from_table,
    uint16_t to_table,
    const ecs_inheritance_plan_t *inheritance
) {
    ecs_type_fini(&transition->type);
    ecs_inheritance_plan_fini(&transition->inheritance);
    free(transition->inherited_copies);
    transition->inherited_copies = NULL;

    const ecs_table_t *table = ecs_get_table(to_table);
    transition->type =
        ecs_type_with_ids(&table->type, table->type.ids, table->type.component_count);
    if (inheritance->count) {
        transition->inheritance.ids = malloc(inheritance->count * sizeof *inheritance->ids);
        memcpy(
            transition->inheritance.ids,
            inheritance->ids,
            inheritance->count * sizeof *inheritance->ids
        );
        transition->inheritance.count = inheritance->count;
        transition->inherited_copies =
            malloc(inheritance->count * sizeof *transition->inherited_copies);
        for (uint16_t i = 0; i < inheritance->count; i++) {
            ecs_component_t component = inheritance->ids[i];
            transition->inherited_copies[i] = (ecs_command_inherited_copy_t){
                .record = ecs_component_index_get(component),
                .source = ecs_try_get_cid(command->base, component),
                .column = ecs_table_get_column_index(table, component),
            };
        }
    }

    const ecs_deferred_change_t *changes = command->changes;
    for (uint32_t i = 0; i < command->change_count; i++) {
        transition->ids[i] = changes[i].id;
        transition->ops[i] = changes[i].op;
    }
    transition->base = command->base;
    transition->change_count = command->change_count;
    transition->from_table = from_table;
    transition->to_table = to_table;
    transition->has_base = command->has_base;
    transition->valid = true;
}

static void command_transition_fini(ecs_command_transition_t *transition) {
    ecs_type_fini(&transition->type);
    ecs_inheritance_plan_fini(&transition->inheritance);
    free(transition->inherited_copies);
}

static void command_transition_copy_inherited(
    const ecs_command_transition_t *transition,
    ecs_table_t *table,
    uint32_t row
) {
    for (uint16_t i = 0; i < transition->inheritance.count; i++) {
        const ecs_command_inherited_copy_t *copy = &transition->inherited_copies[i];
        void *destination = ecs_table_component_at_column(table, copy->column, row);
        if (copy->record->ops.copy) {
            copy->record->ops.copy(destination, copy->source, 1);
        } else if (copy->record->info->size) {
            memcpy(destination, copy->source, copy->record->info->size);
        }
    }
}

static uint32_t change_lower_bound(const ecs_entity_command_t *command, ecs_component_t id) {
    const ecs_deferred_change_t *items = command->changes;
    uint32_t first = 0, count = command->change_count;
    while (count) {
        uint32_t step = count / 2, middle = first + step;
        if (items[middle].id < id) {
            first = middle + 1;
            count -= step + 1;
        } else
            count = step;
    }
    return first;
}

static inline ecs_deferred_change_t *change_get(
    ecs_entity_command_t *command,
    ecs_arena_t *arena,
    ecs_component_t id,
    ecs_deferred_op_t op,
    bool *inserted
) {
    uint32_t at = change_lower_bound(command, id);
    ecs_deferred_change_t *items = command_changes(command);
    *inserted = at == command->change_count || items[at].id != id;
    if (!*inserted)
        return &items[at];
    if (command->change_count == command->change_capacity) {
        command->change_capacity = command->change_capacity ? command->change_capacity * 2 : 2;
        ecs_deferred_change_t *changes =
            ecs_arena_alloc(arena, command->change_capacity * sizeof *changes);
        if (command->change_count) {
            memcpy(changes, command->changes, command->change_count * sizeof *changes);
        }
        command->changes = changes;
        items = changes;
    }
    memmove(items + at + 1, items + at, (command->change_count - at) * sizeof *items);
    command->change_count++;
    items[at] = (ecs_deferred_change_t){ .id = id, .op = op };
    return &items[at];
}

static inline void command_init(ecs_entity_command_t *command, ecs_entity_t entity) {
    *command = (ecs_entity_command_t){
        .entity = entity,
        .relation_head = ECS_COMMAND_NONE,
    };
}

static inline void command_fini(ecs_entity_command_t *command) {
    ecs_deferred_change_t *changes = command_changes(command);
    for (uint32_t i = 0; i < command->change_count; i++) {
        deferred_change_fini(&changes[i]);
    }
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

static ecs_entity_command_t *command_for_entity(ecs_command_buffer_t *buffer, ecs_entity_t entity) {
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

static inline void
command_buffer_change(ecs_entity_t entity, ecs_component_t id, void *data, ecs_deferred_op_t op) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    bool inserted;
    ecs_deferred_change_t *change = change_get(command, buffer->arena, id, op, &inserted);
    if (!inserted && op == EcsDeferredAdd) {
        if (change->op == EcsDeferredRemove)
            change->op = op;
        return;
    } else if (!inserted) {
        deferred_change_fini(change);
        change->op = op;
    }
    if (op == EcsDeferredRemove || op == EcsDeferredAdd)
        return;
    const ecs_component_record_t *record = ecs_component_index_get(id);
    change->data = ecs_arena_alloc(buffer->arena, record->info->size ? record->info->size : 1);
    if (op == EcsDeferredMove) {
        ecs_component_value_move_ctor(record, change->data, data, 1);
    } else {
        ecs_component_value_copy_ctor(record, change->data, data, 1);
    }
}

void ecs_command_buffer_add(ecs_entity_t entity, ecs_component_t id) {
    command_buffer_change(entity, id, NULL, EcsDeferredAdd);
}

void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id) {
    command_buffer_change(entity, id, NULL, EcsDeferredRemove);
}

void ecs_command_buffer_set(ecs_entity_t entity, ecs_component_t id, const void *data) {
    command_buffer_change(entity, id, (void *)data, EcsDeferredCopy);
}

void ecs_command_buffer_move(ecs_entity_t entity, ecs_component_t id, void *data) {
    command_buffer_change(entity, id, data, EcsDeferredMove);
}

void ecs_command_buffer_kill(ecs_entity_t entity) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    command->kill = true;
    command->has_base = false;
    ecs_deferred_change_t *changes = command_changes(command);
    for (uint32_t i = 0; i < command->change_count; i++) {
        deferred_change_fini(&changes[i]);
    }
    command->change_count = 0;
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

static ecs_type_t command_build_type(
    ecs_command_buffer_t *buffer,
    const ecs_table_t *table,
    ecs_entity_command_t *command
) {
    for (uint32_t i = 0; i < command->change_count; i++) {
        ecs_deferred_change_t *change = &command_changes(command)[i];
        if (change->op == EcsDeferredRemove)
            continue;
        const ecs_component_record_t *record = ecs_component_index_get(change->id);
        for (uint32_t r = 0; r < record->required_count; r++) {
            bool inserted;
            ecs_deferred_change_t *required =
                change_get(command, buffer->arena, record->required[r], EcsDeferredAdd, &inserted);
            if (!inserted && required->op == EcsDeferredRemove)
                required->op = EcsDeferredAdd;
        }
    }

    const ecs_deferred_change_t *changes = command_changes(command);
    ecs_component_t *ids = ecs_arena_alloc(
        buffer->arena,
        sizeof *ids * (table->type.component_count + command->change_count)
    );
    uint16_t ti = 0, ci = 0, count = 0;
    while (ti < table->type.component_count || ci < command->change_count) {
        ecs_component_t table_id =
            ti < table->type.component_count ? table->type.ids[ti] : UINT16_MAX;
        ecs_component_t change_id = ci < command->change_count ? changes[ci].id : UINT16_MAX;
        if (table_id < change_id)
            ids[count++] = table->type.ids[ti++];
        else if (change_id < table_id) {
            if (changes[ci].op != EcsDeferredRemove)
                ids[count++] = change_id;
            ci++;
        } else {
            if (changes[ci].op != EcsDeferredRemove)
                ids[count++] = table_id;
            ti++;
            ci++;
        }
    }
    ecs_type_t type = ecs_type_with_ids(&table->type, ids, count);
    if (!command->has_base) {
        return type;
    }
    ecs_type_t out =
        ecs_type_with(&type, 0, (ecs_type_pair_t){ .key = ecs_rid(IsA), .value = command->base });
    ecs_type_fini(&type);
    return out;
}

static bool command_type_unchanged(const ecs_table_t *table, const ecs_entity_command_t *command) {
    const ecs_deferred_change_t *changes = command_changes((ecs_entity_command_t *)command);
    if (command->has_base && command->base != ecs_type_isa_target(&table->type)) {
        return false;
    }

    for (uint32_t i = 0; i < command->change_count; i++) {
        if (changes[i].op == EcsDeferredRemove || !ecs_table_has_owned(table, changes[i].id)) {
            return false;
        }
    }
    return true;
}

static void command_apply_changes(ecs_entity_command_t *command) {
    ecs_deferred_change_t *changes = command_changes(command);
    for (uint32_t i = 0; i < command->change_count && ecs_is_alive(command->entity); i++) {
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
        ecs_emit(table, command->entity, EcsOnSet, changes[i].id, changes[i].data);
        ecs_component_value_move(record, dst, changes[i].data, 1);
        changes[i].data = NULL;
    }
}

static void command_apply_relations(ecs_entity_command_t *command, const sicore_vec_t *relations) {
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

static void command_finish_base_change(ecs_entity_command_t *command, ecs_entity_t old_base) {
    if (!ecs_is_alive(command->entity)) {
        return;
    }
    if (command->base) {
        ecs_inheritance_instantiate_children(command->entity, command->base);
    }
    ecs_entity_record_t *record = ecs_get_record(command->entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    ecs_relation_event_t event = {
        .relation = ecs_rid(IsA),
        .old_target = old_base,
        .new_target = command->base,
    };
    ecs_emit(table, command->entity, EcsOnRelationSet, 0, &event);
}

static void command_apply(
    ecs_command_buffer_t *buffer,
    ecs_entity_command_t *command,
    const ecs_entity_command_t *next,
    const sicore_vec_t *relations,
    ecs_command_transition_t *transition
) {
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
    ecs_entity_t old_base = ecs_type_isa_target(&old_table->type);
    if (command_type_unchanged(old_table, command)) {
        command_apply_changes(command);
        command_apply_relations(command, relations);
        return;
    }

    ecs_inheritance_plan_t inheritance_plan = { 0 };
    bool base_changed = command->has_base && command->base != ecs_type_isa_target(&old_table->type);
    if (base_changed) {
        if (command->base) {
            ecs_abstract_now(command->base);
            old_table = ecs_get_table(old_table_id);
        }
    }

    bool transition_hit = command->change_count <= ECS_COMMAND_TRANSITION_CHANGE_CAPACITY &&
                          command_transition_matches(transition, command, old_table_id);
    ecs_type_t final_type = { 0 };
    const ecs_type_t *target_type;
    const ecs_inheritance_plan_t *copy_plan;
    uint16_t new_table_id;

    if (transition_hit) {
        target_type = &transition->type;
        copy_plan = &transition->inheritance;
        new_table_id = transition->to_table;
    } else {
        final_type = command_build_type(buffer, old_table, command);
        if (base_changed) {
            ecs_inheritance_plan_build(&final_type, command->base, &inheritance_plan);
            if (inheritance_plan.count != 0) {
                ecs_type_t materialized = ecs_type_with_added_ids(
                    &final_type,
                    inheritance_plan.ids,
                    inheritance_plan.count
                );
                ecs_type_fini(&final_type);
                final_type = materialized;
            }
        }
        target_type = &final_type;
        copy_plan = &inheritance_plan;
    }

    bool type_changed = transition_hit ? old_table_id != new_table_id
                                       : !ecs_type_equals(&old_table->type, target_type);
    if (type_changed) {
        uint32_t old_row = record->table_row;
        ecs_emit_removed_components(old_table, target_type, command->entity, old_row);
        if (!ecs_is_alive(command->entity)) {
            ecs_type_fini(&final_type);
            ecs_inheritance_plan_fini(&inheritance_plan);
            return;
        }

        if (!transition_hit) {
            new_table_id = ecs_table_index_get_or_create(final_type);
            final_type = (ecs_type_t){ 0 };
            if (command->change_count <= ECS_COMMAND_TRANSITION_CHANGE_CAPACITY && next &&
                command_shape_equals(command, next) && ecs_is_alive(next->entity) &&
                ecs_get_record(next->entity)->table_id == old_table_id) {
                command_transition_set(
                    transition,
                    command,
                    old_table_id,
                    new_table_id,
                    &inheritance_plan
                );
            }
        }
        record = ecs_get_record(command->entity);
        old_table = ecs_get_table(old_table_id);
        ecs_migrate(record, command->entity, old_table, new_table_id, 0);
        record = ecs_get_record(command->entity);
        ecs_table_t *new_table = ecs_get_table(record->table_id);
        if (base_changed) {
            if (transition_hit) {
                command_transition_copy_inherited(transition, new_table, record->table_row);
            } else {
                ecs_inheritance_plan_copy(copy_plan, command->base, new_table, record->table_row);
            }
        }
        if (ecs_emit_added_components(old_table, new_table, command->entity, record->table_row)) {
            ecs_apply_added_component_default_relations(old_table, new_table, command->entity);
        }
    }

    ecs_type_fini(&final_type);
    ecs_inheritance_plan_fini(&inheritance_plan);

    command_apply_changes(command);
    command_apply_relations(command, relations);
    if (base_changed) {
        command_finish_base_change(command, old_base);
    }
}

void ecs_command_buffer_flush_buffer(ecs_command_buffer_t *buffer) {
    if (buffer->commands.size == 0) {
        ecs_arena_reset(buffer->arena);
        return;
    }

    ecs_command_transition_t transition = { 0 };
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
            command_apply(
                buffer,
                &items[i],
                i + 1 < commands.size ? &items[i + 1] : NULL,
                &relations,
                &transition
            );
            command_fini(&items[i]);
        }
        sicore_vec_fini(&commands);
        sicore_vec_fini(&relations);
    }
    command_transition_fini(&transition);
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

void ecs_defer_begin(void) { ecs_execution_context_current()->defer_depth++; }

void ecs_defer_end(void) {
    ecs_execution_context_t *context = ecs_execution_context_current();
    ecs_assert(context->defer_depth > 0, "ecs_defer_end called without ecs_defer_begin\n");
    context->defer_depth--;
    if (context->defer_depth == 0 && !context->scheduler_parallel &&
        context->commands.commands.size != 0) {
        ecs_command_buffer_flush();
    }
}
