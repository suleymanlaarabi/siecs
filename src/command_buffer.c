#include "command_buffer.h"
#include "datastructure/arena.h"
#include "event_ops.h"
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

#define ECS_COMMAND_NONE UINT32_MAX

static inline void id_vec_push_unique(sicore_vec_t *vec, ecs_component_t id) {
    if (!sicore_vec_contains_u16(vec, id)) {
        sicore_vec_push_u16(vec, id);
    }
}

static inline void deferred_set_fini(ecs_deferred_set_t *set) {
    if (!set->data) {
        return;
    }
    const ecs_component_record_t *record = ecs_component_index_get(set->id);
    ecs_component_value_dtor(record, set->data, 1);
    set->data = NULL;
}

static inline void set_vec_remove(sicore_vec_t *vec, ecs_component_t id) {
    ecs_deferred_set_t *sets = sicore_vec_data(vec, ecs_deferred_set_t);
    for (uint32_t i = 0; i < vec->size; i++) {
        if (sets[i].id == id) {
            deferred_set_fini(&sets[i]);
            sicore_vec_remove_fast(vec, i, sizeof(ecs_deferred_set_t));
            return;
        }
    }
}

static inline ecs_deferred_set_t *set_vec_find(sicore_vec_t *vec, ecs_component_t id) {
    ecs_deferred_set_t *sets = sicore_vec_data(vec, ecs_deferred_set_t);
    for (uint32_t i = 0; i < vec->size; i++) {
        if (sets[i].id == id) {
            return &sets[i];
        }
    }
    return NULL;
}

static inline void command_init(ecs_entity_command_t *command, ecs_entity_t entity) {
    *command = (ecs_entity_command_t){ .entity = entity };
    sicore_vec_init(&command->add_ids, sizeof(ecs_component_t));
    sicore_vec_init(&command->remove_ids, sizeof(ecs_component_t));
    sicore_vec_init(&command->sets, sizeof(ecs_deferred_set_t));
}

static inline void command_fini(ecs_entity_command_t *command) {
    ecs_deferred_set_t *sets = sicore_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size; i++) {
        deferred_set_fini(&sets[i]);
    }
    sicore_vec_fini(&command->add_ids);
    sicore_vec_fini(&command->remove_ids);
    sicore_vec_fini(&command->sets);
}

void ecs_command_buffer_init() {
    ecs_command_buffer_t *buffer = &ecs_world.commands;
    sicore_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));
    buffer->entity_to_command = NULL;
    buffer->entity_capacity = 0;
}

void ecs_command_buffer_fini() {
    ecs_command_buffer_t *buffer = &ecs_world.commands;
    ecs_entity_command_t *commands = sicore_vec_data(&buffer->commands, ecs_entity_command_t);
    for (uint32_t i = 0; i < buffer->commands.size; i++) {
        command_fini(&commands[i]);
    }
    sicore_vec_fini(&buffer->commands);
    free(buffer->entity_to_command);
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

static ecs_entity_command_t *command_for_entity(ecs_entity_t entity) {
    ecs_command_buffer_t *buffer = &ecs_world.commands;
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
    ecs_entity_command_t *command = command_for_entity(entity);
    sicore_vec_remove_u16(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);
}

void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_command_t *command = command_for_entity(entity);
    sicore_vec_remove_u16(&command->add_ids, id);
    set_vec_remove(&command->sets, id);
    id_vec_push_unique(&command->remove_ids, id);
}

void ecs_command_buffer_set(ecs_entity_t entity, ecs_component_t id, const void *data) {
    ecs_entity_command_t *command = command_for_entity(entity);
    const ecs_component_record_t *record = ecs_component_index_get(id);

    sicore_vec_remove_u16(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);

    ecs_deferred_set_t *set = set_vec_find(&command->sets, id);
    if (set) {
        ecs_component_value_dtor(record, set->data, 1);
        ecs_component_value_copy_ctor(record, set->data, data, 1);
        return;
    }

    uint32_t size = record->size ? record->size : 1;
    void *copy = ecs_arena_alloc(&ecs_world.arena_allocator, size);
    ecs_component_value_copy_ctor(record, copy, data, 1);
    ecs_deferred_set_t new_set = { .id = id, .data = copy };
    sicore_vec_push(&command->sets, &new_set, sizeof(ecs_deferred_set_t));
}

void ecs_command_buffer_move(ecs_entity_t entity, ecs_component_t id, void *data) {
    ecs_entity_command_t *command = command_for_entity(entity);
    const ecs_component_record_t *record = ecs_component_index_get(id);

    sicore_vec_remove_u16(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);

    ecs_deferred_set_t *set = set_vec_find(&command->sets, id);
    if (set) {
        ecs_component_value_dtor(record, set->data, 1);
        ecs_component_value_move_ctor(record, set->data, data, 1);
        return;
    }

    uint32_t size = record->size ? record->size : 1;
    void *copy = ecs_arena_alloc(&ecs_world.arena_allocator, size);
    ecs_component_value_move_ctor(record, copy, data, 1);
    ecs_deferred_set_t new_set = { .id = id, .data = copy };
    sicore_vec_push(&command->sets, &new_set, sizeof(ecs_deferred_set_t));
}

void ecs_command_buffer_kill(ecs_entity_t entity) {
    ecs_entity_command_t *command = command_for_entity(entity);
    command->kill = true;
    command->has_base = false;
    sicore_vec_clear(&command->add_ids);
    sicore_vec_clear(&command->remove_ids);
    ecs_deferred_set_t *sets = sicore_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size; i++) {
        deferred_set_fini(&sets[i]);
    }
    sicore_vec_clear(&command->sets);
}

void ecs_command_buffer_set_base(ecs_entity_t entity, ecs_entity_t target) {
    ecs_entity_command_t *command = command_for_entity(entity);
    command->has_base = true;
    command->base = target;
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

static void final_ids_collect_requirements(sicore_vec_t *final_ids, ecs_component_t id) {
    const ecs_component_record_t *record = ecs_component_index_get(id);
    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t required = record->required[i];
        if (sicore_vec_contains_u16(final_ids, required)) {
            continue;
        }
        final_ids_collect_requirements(final_ids, required);
        final_ids_push_sorted(final_ids, required);
    }
}

static ecs_type_t
command_build_type(const ecs_table_t *table, const ecs_entity_command_t *command) {
    sicore_vec_t final_ids;
    sicore_vec_init(&final_ids, sizeof(ecs_component_t));

    for (uint16_t i = 0; i < table->type.count; i++) {
        ecs_component_t id = table->type.ids[i];
        if (!sicore_vec_contains_u16(&command->remove_ids, id)) {
            sicore_vec_push_u16(&final_ids, id);
        }
    }

    const ecs_component_t *adds = sicore_vec_data(&command->add_ids, ecs_component_t);
    for (uint32_t i = 0; i < command->add_ids.size; i++) {
        final_ids_collect_requirements(&final_ids, adds[i]);
        final_ids_push_sorted(&final_ids, adds[i]);
    }

    return (ecs_type_t){
        .ids = sicore_vec_data(&final_ids, ecs_component_t),
        .count = (uint16_t)final_ids.size,
        .base = command->has_base ? command->base : table->type.base,
    };
}

static void command_emit_removed(
    ecs_table_t *table,
    ecs_entity_t entity,
    uint32_t row,
    const ecs_type_t *final_type
) {
    uint16_t final_i = 0;
    for (uint16_t i = 0; i < table->type.count; i++) {
        ecs_component_t id = table->type.ids[i];
        while (final_i < final_type->count && final_type->ids[final_i] < id) {
            final_i++;
        }
        if (final_i < final_type->count && final_type->ids[final_i] == id) {
            continue;
        }

        void *data = ecs_table_component_at_column(table, i, row);
        const ecs_component_record_t *record = ecs_component_index_get(id);
        if (record->on_remove) {
            record->on_remove(entity, id, data);
        }
        ecs_emit(table, entity, EcsOnRemove, data);
    }
}

static bool command_type_unchanged(const ecs_table_t *table, const ecs_entity_command_t *command) {
    if (command->remove_ids.size != 0) {
        return false;
    }
    if (command->has_base && command->base != table->type.base) {
        return false;
    }

    const ecs_component_t *adds = sicore_vec_data(&command->add_ids, ecs_component_t);
    for (uint32_t i = 0; i < command->add_ids.size; i++) {
        if (!ecs_table_has_owned(table, adds[i])) {
            return false;
        }
    }
    return true;
}

static void command_apply_sets(ecs_entity_command_t *command) {
    ecs_deferred_set_t *sets = sicore_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size && ecs_is_alive(command->entity); i++) {
        ecs_component_t id = sets[i].id;
        const ecs_component_record_t *record = ecs_component_index_get(id);
        ecs_entity_record_t *entity_record = ecs_get_record(command->entity);
        ecs_table_t *table = ecs_get_table(entity_record->table_id);
        uint16_t column = ecs_table_get_column_index(table, id);
        void *dst = ecs_table_component_at_column(table, column, entity_record->table_row);

        if (record->on_set) {
            record->on_set(command->entity, id, sets[i].data, dst);
            if (!ecs_is_alive(command->entity)) {
                return;
            }
            entity_record = ecs_get_record(command->entity);
            table = ecs_get_table(entity_record->table_id);
            column = ecs_table_get_column_index(table, id);
            dst = ecs_table_component_at_column(table, column, entity_record->table_row);
        }
        ecs_emit(table, command->entity, EcsOnSet, sets[i].data);
        ecs_component_value_move(record, dst, sets[i].data, 1);
        sets[i].data = NULL;
    }
}

static void command_apply(ecs_entity_command_t *command) {
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
        command_apply_sets(command);
        return;
    }

    ecs_type_t final_type = command_build_type(old_table, command);

    if (!ecs_type_equals(&old_table->type, &final_type)) {
        uint32_t old_row = record->table_row;
        command_emit_removed(old_table, command->entity, old_row, &final_type);
        if (!ecs_is_alive(command->entity)) {
            ecs_type_fini(&final_type);
            return;
        }

        uint16_t new_table_id = ecs_table_index_get_or_create(final_type);
        record = ecs_get_record(command->entity);
        old_table = ecs_get_table(old_table_id);
        const ecs_table_t *emit_old_table = old_table;
        ecs_migrate_to_table(record, command->entity, old_table, new_table_id);
        record = ecs_get_record(command->entity);
        ecs_table_t *new_table = ecs_get_table(record->table_id);
        ecs_emit_added_components(emit_old_table, new_table, command->entity, record->table_row);
    } else {
        ecs_type_fini(&final_type);
    }

    command_apply_sets(command);
}

void ecs_command_buffer_flush() {
    ecs_command_buffer_t *buffer = &ecs_world.commands;
    if (buffer->commands.size == 0) {
        ecs_arena_reset(&ecs_world.arena_allocator);
        return;
    }

    ecs_world.flushing_commands = true;
    while (buffer->commands.size != 0) {
        sicore_vec_t commands = buffer->commands;
        sicore_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));

        ecs_entity_command_t *items = sicore_vec_data(&commands, ecs_entity_command_t);
        for (uint32_t i = 0; i < commands.size; i++) {
            uint32_t entity_id = ecs_first(items[i].entity);
            buffer->entity_to_command[entity_id] = ECS_COMMAND_NONE;
        }

        for (uint32_t i = 0; i < commands.size; i++) {
            command_apply(&items[i]);
            command_fini(&items[i]);
        }
        sicore_vec_fini(&commands);
    }
    ecs_world.flushing_commands = false;
    ecs_arena_reset(&ecs_world.arena_allocator);
}

void ecs_defer_begin(void) { ecs_world.defer_depth++; }

void ecs_defer_end(void) {
    ecs_assert(ecs_world.defer_depth > 0, "ecs_defer_end called without ecs_defer_begin\n");
    ecs_world.defer_depth--;
    if (ecs_world.defer_depth == 0) {
        ecs_command_buffer_flush();
    }
}
