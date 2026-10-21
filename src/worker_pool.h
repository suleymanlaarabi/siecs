#ifndef SIECS_WORKER_POOL_H
#define SIECS_WORKER_POOL_H

#include "command_buffer.h"
#include "platform.h"
#include <stdint.h>

typedef struct ecs_worker_pool_s ecs_worker_pool_t;
typedef struct ecs_worker_job_s ecs_worker_job_t;

typedef struct {
    _Alignas(64) ecs_platform_thread_t thread;
    ecs_worker_pool_t *pool;
    ecs_entity_reservation_t reservation;
    ecs_worker_job_t *jobs, *free_jobs;
} ecs_worker_t;

typedef struct {
    ecs_system_id_t system;
    sicore_vec_t commands;
    uint32_t pending, running, next_row;
    uint16_t next_table;
    uint32_t next_ordinal;
    uint64_t tick; // change tick of this run, taken when the system starts
    bool started, exhausted, complete, released, queued;
} ecs_worker_node_t;

struct ecs_worker_pool_s {
    uint16_t worker_count;
    ecs_worker_t *workers;
    void *worker_storage;
    ecs_worker_node_t *nodes;
    uint32_t node_count, node_capacity;
    uint32_t *ready, ready_head, ready_count;
    uint32_t *system_to_node, system_capacity;
    uint32_t active, cursor;
    bool stop, in_run, draining, publishing, forced, allocating;
    ecs_platform_mutex_t mutex;
    ecs_platform_condition_t work_condition;
    ecs_platform_condition_t completion_condition;
};

void ecs_worker_pool_init(ecs_worker_pool_t *pool, uint16_t requested_workers);
void ecs_worker_pool_fini(ecs_worker_pool_t *pool);
void ecs_worker_pool_run(
    ecs_worker_pool_t *pool,
    const ecs_system_id_t *systems,
    uint32_t count,
    ecs_phase_t phase,
    bool forced
);

#endif
