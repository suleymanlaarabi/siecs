#ifndef SIECS_STORAGE_SYSTEM_INDEX_H
#define SIECS_STORAGE_SYSTEM_INDEX_H
#include "siecs.h"
#include <stdint.h>

typedef struct {
    uint32_t first;
    uint32_t count;
} ecs_system_batch_t;

typedef struct {
    const char *name;
    ecs_query_id_t qid;
    void (*callback)(ecs_iter_t *);
    uintptr_t user_data;
    void (*user_data_dtor)(uintptr_t user_data);
    ecs_phase_t phase;
    ecs_system_id_t after[ECS_SYSTEM_AFTER_CAPACITY];
    uint32_t resource_accesses[ECS_SYSTEM_RESOURCE_CAPACITY * 2];
    uint16_t resource_access_count;
    bool enabled;
    bool main_thread_only;
} ecs_system_t;

typedef struct {
    ecs_phase_t id;
    const char *name;
    sicore_vec_t systems;
    sicore_vec_t batches;
} ecs_phase_info_t;

typedef struct {
    sicore_vec_t systems;
    sicore_vec_t phases;
    sicore_vec_t phase_order;
    sicore_vec_t execution_order;
    uint32_t start_phase_count;
    bool plan_dirty;
} ecs_system_index_t;

extern ecs_system_index_t system_index;

void ecs_system_index_init(void);
void ecs_system_index_fini(void);

ecs_phase_t ecs_phase_register(const ecs_phase_desc_t *desc);
ecs_phase_info_t *ecs_system_index_get_phase(ecs_phase_t phase);

ecs_system_id_t ecs_system_index_create(const ecs_system_desc_t *desc, ecs_query_id_t qid);
ecs_system_t *ecs_system_index_get(ecs_system_id_t system);
void ecs_system_index_build_plan(void);

#endif
