#ifndef SIECS_WORKER_POOL_H
#define SIECS_WORKER_POOL_H

#include "command_buffer.h"
#include "platform.h"
#include <stdatomic.h>
#include <stdint.h>

typedef struct {
    ecs_system_id_t system;
} ecs_worker_job_t;

typedef struct ecs_worker_pool_s ecs_worker_pool_t;

typedef struct {
    ecs_platform_thread_t thread;
    ecs_execution_context_t context;
    ecs_worker_pool_t *pool;
    uint16_t index;
    _Alignas(64) atomic_uint completed;
} ecs_worker_t;

struct ecs_worker_pool_s {
    uint16_t worker_count;
    ecs_worker_t *workers;
    ecs_worker_job_t *jobs;
    uint32_t job_capacity;
    uint32_t job_count;
    _Alignas(64) atomic_uint next_job;
    _Alignas(64) atomic_uint completed_jobs;
    atomic_uint epoch;
    atomic_bool stop;
    ecs_platform_mutex_t mutex;
    ecs_platform_condition_t condition;
};

void ecs_worker_pool_init(ecs_worker_pool_t *pool, uint16_t requested_workers);
void ecs_worker_pool_fini(ecs_worker_pool_t *pool);
bool ecs_worker_pool_enabled(const ecs_worker_pool_t *pool);
void ecs_worker_pool_run_systems(
    ecs_worker_pool_t *pool,
    const ecs_system_id_t *systems,
    uint32_t system_count
);
void ecs_worker_pool_flush(ecs_worker_pool_t *pool);

#endif
