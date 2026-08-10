#ifndef SIECS_COMMAND_BUFFER_H
#define SIECS_COMMAND_BUFFER_H

#include "siecs.h"
#include "datastructure/arena.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct ecs_world_s ecs_world_t;

typedef enum {
    EcsDeferredAdd,
    EcsDeferredRemove,
    EcsDeferredCopy,
    EcsDeferredMove,
} ecs_deferred_op_t;

typedef struct {
    ecs_component_t id;
    ecs_deferred_op_t op;
    void *data;
} ecs_deferred_change_t;

typedef struct {
    ecs_entity_t target; /* 0 removes the relation */
    uint32_t next;
    ecs_relation_id_t id;
} ecs_deferred_relation_t;

typedef struct {
    ecs_entity_t entity;
    uint32_t relation_head;
    bool kill;
    bool has_base;
    ecs_entity_t base;
    sicore_vec_t changes;
} ecs_entity_command_t;

typedef struct ecs_command_buffer_s {
    sicore_vec_t commands;
    sicore_vec_t relations;
    uint32_t *entity_to_command;
    uint32_t entity_capacity;
    ecs_arena_t *arena;
} ecs_command_buffer_t;

typedef struct ecs_execution_context_s {
    ecs_command_buffer_t commands;
    ecs_arena_t arena;
    uint32_t defer_depth;
    bool flushing_commands;
    bool scheduler_parallel;
} ecs_execution_context_t;

void ecs_execution_context_init(ecs_execution_context_t *context);
void ecs_execution_context_fini(ecs_execution_context_t *context);
ecs_execution_context_t *ecs_execution_context_current(void);
void ecs_execution_context_set(ecs_execution_context_t *context);

void ecs_command_buffer_init(ecs_command_buffer_t *buffer, ecs_arena_t *arena);
void ecs_command_buffer_fini(ecs_command_buffer_t *buffer);

void ecs_command_buffer_add(ecs_entity_t entity, ecs_component_t id);
void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id);
void ecs_command_buffer_set(ecs_entity_t entity, ecs_component_t id, const void *data);
void ecs_command_buffer_move(ecs_entity_t entity, ecs_component_t id, void *data);
void ecs_command_buffer_kill(ecs_entity_t entity);
void ecs_command_buffer_set_base(ecs_entity_t entity, ecs_entity_t target);
void ecs_command_buffer_relate(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_entity_t target
);
void ecs_command_buffer_flush();
void ecs_command_buffer_flush_buffer(ecs_command_buffer_t *buffer);

void ecs_add_cid_now(ecs_entity_t entity, ecs_component_t id);
void ecs_remove_cid_now(ecs_entity_t entity, ecs_component_t id);
void ecs_set_cid_now(ecs_entity_t entity, ecs_component_t id, const void *data);
void ecs_move_cid_now(ecs_entity_t entity, ecs_component_t id, void *data);
void ecs_kill_now(ecs_entity_t entity);
void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target);

#endif
