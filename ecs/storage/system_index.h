#pragma once
#include <stdint.h>
#include "ecs/datastructure/vec.h"
#include "ecs/world.h"

typedef struct {
    ecs_query_id_t qid;
    void (*callback)(ecs_iter_t *);
    ecs_system_id_t after[4];
} ecs_system_t;

typedef struct {
    ecs_vec_t systems;
} ecs_system_index_t;

void ecs_system_index_init(ecs_system_index_t *index);
void ecs_system_index_fini(ecs_system_index_t *index);
ecs_system_id_t ecs_system_index_create(
    ecs_system_index_t *index,
    const ecs_system_desc_t *desc
);
