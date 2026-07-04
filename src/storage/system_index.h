#ifndef SIECS_STORAGE_SYSTEM_INDEX_H
#define SIECS_STORAGE_SYSTEM_INDEX_H
#include <stdint.h>
#include "../datastructure/vec.h"
#include "siecs.h"

typedef struct {
    const char *name;
    ecs_query_id_t qid;
    void (*callback)(ecs_iter_t *);
    uintptr_t user_data;
    void (*user_data_dtor)(uintptr_t user_data);
    ecs_phase_t phase;
    ecs_system_id_t after[ECS_SYSTEM_AFTER_CAPACITY];
    bool enabled;
} ecs_system_t;

typedef struct {
    ecs_vec_t systems;
    ecs_vec_t phase_order[EcsPhaseCount];
    bool plan_dirty;
} ecs_system_index_t;

void ecs_system_index_init(ecs_system_index_t *index);
void ecs_system_index_fini(ecs_system_index_t *index);

ecs_system_id_t ecs_system_index_create(ecs_system_index_t *index, const ecs_system_t *system);
ecs_system_t *ecs_system_index_get(ecs_system_index_t *index, ecs_system_id_t system);
void ecs_system_index_build_plan(ecs_system_index_t *index);

#endif
