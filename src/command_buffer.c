#include "command_buffer.h"
#include "datastructure/arena.h"
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

static bool id_vec_contains(const ecs_vec_t *vec, ecs_component_t id) {
    const ecs_component_t *ids = ecs_vec_data(vec, ecs_component_t);
    for (uint32_t i = 0; i < vec->size; i++) {
        if (ids[i] == id) {
            return true;
        }
    }
    return false;
}

static void id_vec_remove(ecs_vec_t *vec, ecs_component_t id) {
    ecs_component_t *ids = ecs_vec_data(vec, ecs_component_t);
    for (uint32_t i = 0; i < vec->size; i++) {
        if (ids[i] == id) {
            ecs_vec_remove_fast(vec, i, sizeof(ecs_component_t));
            return;
        }
    }
}

static void id_vec_push_unique(ecs_vec_t *vec, ecs_component_t id) {
    if (!id_vec_contains(vec, id)) {
        ecs_vec_push(vec, &id, sizeof(ecs_component_t));
    }
}

static void deferred_set_fini(ecs_world_t *world, ecs_deferred_set_t *set) {
    if (!set->data) {
        return;
    }
    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, set->id);
    ecs_component_value_dtor(record, set->data, 1);
    set->data = NULL;
}

static void set_vec_remove(ecs_world_t *world, ecs_vec_t *vec, ecs_component_t id) {
    ecs_deferred_set_t *sets = ecs_vec_data(vec, ecs_deferred_set_t);
    for (uint32_t i = 0; i < vec->size; i++) {
        if (sets[i].id == id) {
            deferred_set_fini(world, &sets[i]);
            ecs_vec_remove_fast(vec, i, sizeof(ecs_deferred_set_t));
            return;
        }
    }
}

static ecs_deferred_set_t *set_vec_find(ecs_vec_t *vec, ecs_component_t id) {
    ecs_deferred_set_t *sets = ecs_vec_data(vec, ecs_deferred_set_t);
    for (uint32_t i = 0; i < vec->size; i++) {
        if (sets[i].id == id) {
            return &sets[i];
        }
    }
    return NULL;
}

static void command_init(ecs_entity_command_t *command, ecs_entity_t entity) {
    *command = (ecs_entity_command_t){ .entity = entity };
    ecs_vec_init(&command->add_ids, sizeof(ecs_component_t));
    ecs_vec_init(&command->remove_ids, sizeof(ecs_component_t));
    ecs_vec_init(&command->sets, sizeof(ecs_deferred_set_t));
}

static void command_fini(ecs_world_t *world, ecs_entity_command_t *command) {
    ecs_deferred_set_t *sets = ecs_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size; i++) {
        deferred_set_fini(world, &sets[i]);
    }
    ecs_vec_fini(&command->add_ids);
    ecs_vec_fini(&command->remove_ids);
    ecs_vec_fini(&command->sets);
}

void ecs_command_buffer_init(ecs_command_buffer_t *buffer) {
    ecs_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));
    buffer->entity_to_command = NULL;
    buffer->entity_capacity = 0;
}

void ecs_command_buffer_fini(ecs_world_t *world, ecs_command_buffer_t *buffer) {
    ecs_entity_command_t *commands = ecs_vec_data(&buffer->commands, ecs_entity_command_t);
    for (uint32_t i = 0; i < buffer->commands.size; i++) {
        command_fini(world, &commands[i]);
    }
    ecs_vec_fini(&buffer->commands);
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

    buffer->entity_to_command =
        realloc(buffer->entity_to_command, sizeof(uint32_t) * new_capacity);
    for (uint32_t i = buffer->entity_capacity; i < new_capacity; i++) {
        buffer->entity_to_command[i] = ECS_COMMAND_NONE;
    }
    buffer->entity_capacity = new_capacity;
}

static ecs_entity_command_t *command_for_entity(ecs_world_t *world, ecs_entity_t entity) {
    ecs_command_buffer_t *buffer = &world->commands;
    uint32_t entity_id = ecs_first(entity);
    command_buffer_ensure_entity(buffer, entity_id);

    uint32_t command_index = buffer->entity_to_command[entity_id];
    if (command_index != ECS_COMMAND_NONE) {
        return ecs_vec_get_mut(&buffer->commands, command_index, ecs_entity_command_t);
    }

    command_index = buffer->commands.size;
    ecs_entity_command_t *command =
        ecs_vec_push_empty(&buffer->commands, sizeof(ecs_entity_command_t));
    command_init(command, entity);
    buffer->entity_to_command[entity_id] = command_index;
    return command;
}

void ecs_command_buffer_add(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_command_t *command = command_for_entity(world, entity);
    id_vec_remove(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);
}

void ecs_command_buffer_remove(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_command_t *command = command_for_entity(world, entity);
    id_vec_remove(&command->add_ids, id);
    set_vec_remove(world, &command->sets, id);
    id_vec_push_unique(&command->remove_ids, id);
}

void ecs_command_buffer_set(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t id,
    const void *data
) {
    ecs_entity_command_t *command = command_for_entity(world, entity);
    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
    void *copy = NULL;

    {
        uint32_t size = record->size ? record->size : 1;
        void *dst = ecs_arena_alloc(&world->arena_allocator, size);
        ecs_component_value_copy_ctor(record, dst, data, 1);
        copy = dst;
    }

    id_vec_remove(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);

    ecs_deferred_set_t *set = set_vec_find(&command->sets, id);
    if (set) {
        deferred_set_fini(world, set);
        set->data = copy;
        return;
    }

    ecs_deferred_set_t new_set = { .id = id, .data = copy };
    ecs_vec_push(&command->sets, &new_set, sizeof(ecs_deferred_set_t));
}

void ecs_command_buffer_move(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t id,
    void *data
) {
    ecs_entity_command_t *command = command_for_entity(world, entity);
    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
    void *copy = NULL;

    {
        uint32_t size = record->size ? record->size : 1;
        void *dst = ecs_arena_alloc(&world->arena_allocator, size);
        ecs_component_value_move_ctor(record, dst, data, 1);
        copy = dst;
    }

    id_vec_remove(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);

    ecs_deferred_set_t *set = set_vec_find(&command->sets, id);
    if (set) {
        deferred_set_fini(world, set);
        set->data = copy;
        return;
    }

    ecs_deferred_set_t new_set = { .id = id, .data = copy };
    ecs_vec_push(&command->sets, &new_set, sizeof(ecs_deferred_set_t));
}

void ecs_command_buffer_kill(ecs_world_t *world, ecs_entity_t entity) {
    ecs_entity_command_t *command = command_for_entity(world, entity);
    command->kill = true;
    command->has_base = false;
    ecs_vec_clear(&command->add_ids);
    ecs_vec_clear(&command->remove_ids);
    ecs_deferred_set_t *sets = ecs_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size; i++) {
        deferred_set_fini(world, &sets[i]);
    }
    ecs_vec_clear(&command->sets);
}

void ecs_command_buffer_set_base(ecs_world_t *world, ecs_entity_t entity, ecs_entity_t target) {
    ecs_entity_command_t *command = command_for_entity(world, entity);
    command->has_base = true;
    command->base = target;
}

static void final_ids_push_sorted(ecs_vec_t *final_ids, ecs_component_t id) {
    ecs_component_t *ids = ecs_vec_data(final_ids, ecs_component_t);
    uint32_t i = 0;
    while (i < final_ids->size && ids[i] < id) {
        i++;
    }
    if (i < final_ids->size && ids[i] == id) {
        return;
    }

    ecs_vec_push_empty(final_ids, sizeof(ecs_component_t));
    ids = ecs_vec_data(final_ids, ecs_component_t);
    for (uint32_t j = final_ids->size - 1; j > i; j--) {
        ids[j] = ids[j - 1];
    }
    ids[i] = id;
}

static bool final_ids_has(const ecs_vec_t *final_ids, ecs_component_t id) {
    return id_vec_contains(final_ids, id);
}

static void final_ids_collect_requirements(
    ecs_world_t *world,
    ecs_vec_t *final_ids,
    ecs_component_t id
) {
    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t required = record->required[i];
        if (final_ids_has(final_ids, required)) {
            continue;
        }
        final_ids_collect_requirements(world, final_ids, required);
        final_ids_push_sorted(final_ids, required);
    }
}

static ecs_type_t command_build_type(
    ecs_world_t *world,
    const ecs_table_t *table,
    const ecs_entity_command_t *command
) {
    ecs_vec_t final_ids;
    ecs_vec_init(&final_ids, sizeof(ecs_component_t));

    for (uint16_t i = 0; i < table->type.count; i++) {
        ecs_component_t id = table->type.ids[i];
        if (!id_vec_contains(&command->remove_ids, id)) {
            final_ids_push_sorted(&final_ids, id);
        }
    }

    const ecs_component_t *adds = ecs_vec_data(&command->add_ids, ecs_component_t);
    for (uint32_t i = 0; i < command->add_ids.size; i++) {
        final_ids_collect_requirements(world, &final_ids, adds[i]);
        final_ids_push_sorted(&final_ids, adds[i]);
    }

    ecs_deferred_set_t *sets = ecs_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size; i++) {
        final_ids_collect_requirements(world, &final_ids, sets[i].id);
        final_ids_push_sorted(&final_ids, sets[i].id);
    }

    return (ecs_type_t){
        .ids = ecs_vec_data(&final_ids, ecs_component_t),
        .count = (uint16_t)final_ids.size,
        .base = command->has_base ? command->base : table->type.base,
    };
}

static void command_emit_removed(
    ecs_world_t *world,
    ecs_table_t *table,
    ecs_entity_t entity,
    uint32_t row,
    const ecs_type_t *final_type
) {
    for (uint16_t i = 0; i < table->type.count; i++) {
        ecs_component_t id = table->type.ids[i];
        if (ecs_type_find(final_type, id) != -1) {
            continue;
        }

        void *data = ecs_table_component_at_column(table, i, row);
        const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
        if (record->on_remove) {
            record->on_remove(world, entity, id, data);
        }
        ecs_emit(world, table, entity, EcsOnRemove, data);
    }
}

static void command_emit_added(
    ecs_world_t *world,
    const ecs_table_t *old_table,
    ecs_table_t *new_table,
    ecs_entity_t entity,
    uint32_t row
) {
    uint16_t old_i = 0;
    for (uint16_t new_i = 0; new_i < new_table->type.count; new_i++) {
        ecs_component_t id = new_table->type.ids[new_i];
        while (old_i < old_table->type.count && old_table->type.ids[old_i] < id) {
            old_i++;
        }
        if (old_i < old_table->type.count && old_table->type.ids[old_i] == id) {
            continue;
        }

        void *data = ecs_table_component_at_column(new_table, new_i, row);
        const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
        if (record->on_add) {
            record->on_add(world, entity, id, data);
        }
        ecs_emit(world, new_table, entity, EcsOnAdd, data);
    }
}

static void command_apply_sets(ecs_world_t *world, ecs_entity_command_t *command) {
    ecs_deferred_set_t *sets = ecs_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size && ecs_is_alive(world, command->entity); i++) {
        ecs_component_t id = sets[i].id;
        const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
        ecs_entity_record_t *entity_record = ecs_get_record(world, command->entity);
        ecs_table_t *table = ecs_get_table(world, entity_record->table_id);
        uint16_t column = ecs_table_get_column_index(table, id);
        void *dst = ecs_table_component_at_column(table, column, entity_record->table_row);

        if (record->on_set) {
            record->on_set(world, command->entity, id, sets[i].data, dst);
            if (!ecs_is_alive(world, command->entity)) {
                return;
            }
            entity_record = ecs_get_record(world, command->entity);
            table = ecs_get_table(world, entity_record->table_id);
            column = ecs_table_get_column_index(table, id);
            dst = ecs_table_component_at_column(table, column, entity_record->table_row);
        }
        ecs_emit(world, table, command->entity, EcsOnSet, sets[i].data);
        ecs_component_value_move(record, dst, sets[i].data, 1);
        sets[i].data = NULL;
    }
}

static void command_apply(ecs_world_t *world, ecs_entity_command_t *command) {
    if (!ecs_is_alive(world, command->entity)) {
        return;
    }

    if (command->kill) {
        ecs_kill_now(world, command->entity);
        return;
    }

    ecs_entity_record_t *record = ecs_get_record(world, command->entity);
    uint16_t old_table_id = record->table_id;
    ecs_table_t *old_table = ecs_get_table(world, old_table_id);
    ecs_type_t final_type = command_build_type(world, old_table, command);

    if (!ecs_type_equals(&old_table->type, &final_type)) {
        uint32_t old_row = record->table_row;
        command_emit_removed(world, old_table, command->entity, old_row, &final_type);
        if (!ecs_is_alive(world, command->entity)) {
            ecs_type_fini(&final_type);
            return;
        }

        uint16_t new_table_id = ecs_table_index_get_or_create(world, final_type);
        record = ecs_get_record(world, command->entity);
        old_table = ecs_get_table(world, old_table_id);
        const ecs_table_t *emit_old_table = old_table;
        ecs_migrate_to_table(world, record, command->entity, old_table, new_table_id);
        record = ecs_get_record(world, command->entity);
        ecs_table_t *new_table = ecs_get_table(world, record->table_id);
        command_emit_added(world, emit_old_table, new_table, command->entity, record->table_row);
    } else {
        ecs_type_fini(&final_type);
    }

    command_apply_sets(world, command);
}

void ecs_command_buffer_flush(ecs_world_t *world) {
    ecs_command_buffer_t *buffer = &world->commands;
    if (buffer->commands.size == 0) {
        ecs_arena_reset(&world->arena_allocator);
        return;
    }

    world->flushing_commands = true;
    while (buffer->commands.size != 0) {
        ecs_vec_t commands = buffer->commands;
        ecs_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));

        ecs_entity_command_t *items = ecs_vec_data(&commands, ecs_entity_command_t);
        for (uint32_t i = 0; i < commands.size; i++) {
            uint32_t entity_id = ecs_first(items[i].entity);
            if (entity_id < buffer->entity_capacity) {
                buffer->entity_to_command[entity_id] = ECS_COMMAND_NONE;
            }
        }

        for (uint32_t i = 0; i < commands.size; i++) {
            command_apply(world, &items[i]);
        }

        for (uint32_t i = 0; i < commands.size; i++) {
            command_fini(world, &items[i]);
        }
        ecs_vec_fini(&commands);
    }
    world->flushing_commands = false;
    ecs_arena_reset(&world->arena_allocator);
}

void ecs_defer_begin(ecs_world_t *world) {
    ecs_assert_not_null(world);
    world->defer_depth++;
}

void ecs_defer_end(ecs_world_t *world) {
    ecs_assert_not_null(world);
    ecs_assert(world->defer_depth > 0, "ecs_defer_end called without ecs_defer_begin\n");
    world->defer_depth--;
    if (world->defer_depth == 0) {
        ecs_command_buffer_flush(world);
    }
}

bool ecs_is_deferred(const ecs_world_t *world) {
    ecs_assert_not_null(world);
    return world->defer_depth != 0 || world->flushing_commands;
}
