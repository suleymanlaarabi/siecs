#ifndef SIECS_COMMAND_BUFFER_H
#define SIECS_COMMAND_BUFFER_H

#include "siecs.h"
#include "datastructure/vec.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct ecs_world_s ecs_world_t;

typedef struct {
    ecs_component_t id;
    void *data;
} ecs_deferred_set_t;

typedef struct {
    ecs_entity_t entity;
    bool kill;
    bool has_base;
    ecs_entity_t base;
    ecs_vec_t add_ids;
    ecs_vec_t remove_ids;
    ecs_vec_t sets;
} ecs_entity_command_t;

typedef struct ecs_command_buffer_s {
    ecs_vec_t commands;
    uint32_t *entity_to_command;
    uint32_t entity_capacity;
} ecs_command_buffer_t;

void ecs_command_buffer_init(ecs_command_buffer_t *buffer);
void ecs_command_buffer_fini(ecs_command_buffer_t *buffer);

void ecs_command_buffer_add(ecs_entity_t entity, ecs_component_t id);
void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id);
void ecs_command_buffer_set(
        ecs_entity_t entity,
    ecs_component_t id,
    const void *data
);
void ecs_command_buffer_move(ecs_entity_t entity, ecs_component_t id, void *data);
void ecs_command_buffer_kill(ecs_entity_t entity);
void ecs_command_buffer_set_base(ecs_entity_t entity, ecs_entity_t target);
void ecs_command_buffer_flush();

void ecs_add_cid_now(ecs_entity_t entity, ecs_component_t id);
void ecs_remove_cid_now(ecs_entity_t entity, ecs_component_t id);
void ecs_set_cid_now(ecs_entity_t entity, ecs_component_t id, const void *data);
void ecs_move_cid_now(ecs_entity_t entity, ecs_component_t id, void *data);
void ecs_kill_now(ecs_entity_t entity);
void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target);

#endif
