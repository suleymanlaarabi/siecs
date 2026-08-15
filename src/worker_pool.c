#include "worker_pool.h"
#include "siecs.h"
#include "utils.h"
#include "world_internal.h"
#ifdef _WIN32
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>

#define ECS_WORKER_ALIGNMENT 64

static void *ecs_worker_alloc(size_t size) {
#ifdef _WIN32
    void *memory = _aligned_malloc(size, ECS_WORKER_ALIGNMENT);
    if (memory) {
        memset(memory, 0, size);
    }
    return memory;
#else
    void *memory = NULL;
    if (posix_memalign(&memory, ECS_WORKER_ALIGNMENT, size) != 0) {
        return NULL;
    }
    memset(memory, 0, size);
    return memory;
#endif
}

static void ecs_worker_free(void *memory) {
#ifdef _WIN32
    _aligned_free(memory);
#else
    free(memory);
#endif
}

static void ecs_worker_run_job(ecs_worker_pool_t *pool, uint32_t job_index) {
    ecs_worker_job_t *job = &pool->jobs[job_index];
    ecs_run_system(job->system);
    uint32_t completed = atomic_fetch_add_explicit(
        &pool->completed_jobs,
        1,
        memory_order_release
    ) + 1;
    if (completed == pool->job_count) {
        ecs_platform_mutex_lock(&pool->mutex);
        ecs_platform_condition_signal(&pool->condition);
        ecs_platform_mutex_unlock(&pool->mutex);
    }
}

static inline void ecs_worker_run_jobs(ecs_worker_pool_t *pool) {
    for (;;) {
        uint32_t job = atomic_fetch_add_explicit(&pool->next_job, 1, memory_order_relaxed);
        if (job >= pool->job_count) return;
        ecs_worker_run_job(pool, job);
    }
}

#ifdef _WIN32
static DWORD ECS_PLATFORM_THREAD_CALL ecs_worker_loop(void *argument)
#else
static void *ecs_worker_loop(void *argument)
#endif
{
    ecs_worker_t *worker = argument;
    ecs_worker_pool_t *pool = worker->pool;
    uint32_t seen_epoch = 0;
    ecs_execution_context_set(&worker->context);

    for (;;) {
        ecs_platform_mutex_lock(&pool->mutex);
        while (!atomic_load_explicit(&pool->stop, memory_order_acquire) &&
               atomic_load_explicit(&pool->epoch, memory_order_acquire) == seen_epoch) {
            ecs_platform_condition_wait(&pool->condition, &pool->mutex);
        }
        if (atomic_load_explicit(&pool->stop, memory_order_acquire)) {
            ecs_platform_mutex_unlock(&pool->mutex);
            break;
        }
        seen_epoch = atomic_load_explicit(&pool->epoch, memory_order_relaxed);
        ecs_platform_mutex_unlock(&pool->mutex);

        ecs_worker_run_jobs(pool);
    }

    ecs_execution_context_set(NULL);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

void ecs_worker_pool_init(ecs_worker_pool_t *pool, uint16_t requested_workers) {
    *pool = (ecs_worker_pool_t){ 0 };
    if (requested_workers == ECS_WORKERS_AUTO) {
        uint32_t cpu_count = ecs_platform_hardware_thread_count();
        requested_workers = cpu_count > 1 ? (uint16_t)(cpu_count - 1) : 0;
    }
    if (requested_workers == 0) {
        return;
    }

    pool->worker_count = requested_workers;
    pool->workers = ecs_worker_alloc(requested_workers * sizeof(ecs_worker_t));
    ecs_assert_not_null(pool->workers);
    ecs_platform_mutex_init(&pool->mutex);
    ecs_platform_condition_init(&pool->condition);
    atomic_init(&pool->next_job, 0);
    atomic_init(&pool->completed_jobs, 0);
    atomic_init(&pool->epoch, 0);
    atomic_init(&pool->stop, false);

    for (uint16_t i = 0; i < requested_workers; i++) {
        ecs_worker_t *worker = &pool->workers[i];
        worker->pool = pool;
        worker->index = i;
        atomic_init(&worker->completed, 0);
        ecs_execution_context_init(&worker->context);
        bool created = ecs_platform_thread_create(&worker->thread, ecs_worker_loop, worker);
        ecs_assert(created, "failed to create ECS worker thread\n"); (void)created;
    }
}

void ecs_worker_pool_fini(ecs_worker_pool_t *pool) {
    if (!pool->worker_count) {
        return;
    }

    atomic_store_explicit(&pool->stop, true, memory_order_release);
    ecs_platform_mutex_lock(&pool->mutex);
    ecs_platform_condition_broadcast(&pool->condition);
    ecs_platform_mutex_unlock(&pool->mutex);

    for (uint16_t i = 0; i < pool->worker_count; i++) {
        ecs_platform_thread_join(&pool->workers[i].thread);
        ecs_execution_context_fini(&pool->workers[i].context);
    }
    ecs_platform_condition_fini(&pool->condition);
    ecs_platform_mutex_fini(&pool->mutex);
    free(pool->jobs);
    ecs_worker_free(pool->workers);
    *pool = (ecs_worker_pool_t){ 0 };
}

bool ecs_worker_pool_enabled(const ecs_worker_pool_t *pool) {
    return pool->worker_count != 0;
}

void ecs_worker_pool_run_systems(
    ecs_worker_pool_t *pool,
    const ecs_system_id_t *systems,
    uint32_t system_count
) {
    if (system_count > pool->job_capacity) {
        uint32_t capacity = pool->job_capacity ? pool->job_capacity : 4;
        while (capacity < system_count) {
            capacity *= 2;
        }
        pool->jobs = realloc(pool->jobs, capacity * sizeof(ecs_worker_job_t));
        ecs_assert_not_null(pool->jobs);
        pool->job_capacity = capacity;
    }
    for (uint32_t i = 0; i < system_count; i++) {
        pool->jobs[i].system = systems[i];
    }
    pool->job_count = system_count;
    atomic_store_explicit(&pool->next_job, 0, memory_order_relaxed);
    atomic_store_explicit(&pool->completed_jobs, 0, memory_order_relaxed);
    ecs_world.main_context.scheduler_parallel = true;
    for (uint16_t i = 0; i < pool->worker_count; i++) {
        pool->workers[i].context.scheduler_parallel = true;
    }
    ecs_execution_context_set(&ecs_world.main_context);
    ecs_platform_mutex_lock(&pool->mutex);
    atomic_store_explicit(
        &pool->epoch,
        atomic_load_explicit(&pool->epoch, memory_order_relaxed) + 1,
        memory_order_release
    );
    ecs_platform_condition_broadcast(&pool->condition);
    ecs_platform_mutex_unlock(&pool->mutex);

    ecs_worker_run_jobs(pool);

    while (atomic_load_explicit(&pool->completed_jobs, memory_order_acquire) < system_count) {
        ecs_platform_mutex_lock(&pool->mutex);
        if (atomic_load_explicit(&pool->completed_jobs, memory_order_acquire) < system_count) {
            ecs_platform_condition_wait(&pool->condition, &pool->mutex);
        }
        ecs_platform_mutex_unlock(&pool->mutex);
    }
}

void ecs_worker_pool_flush(ecs_worker_pool_t *pool) {
    ecs_execution_context_t *main_context = &ecs_world.main_context;
    main_context->flushing_commands = true;
    ecs_command_buffer_flush_buffer(&main_context->commands);
    for (uint16_t i = 0; i < pool->worker_count; i++) {
        ecs_command_buffer_flush_buffer(&pool->workers[i].context.commands);
    }
    /* Hooks/observers during worker-buffer application can enqueue main-thread
     * commands. Apply that deterministic tail before releasing the barrier. */
    ecs_command_buffer_flush_buffer(&main_context->commands);
    main_context->flushing_commands = false;
    main_context->scheduler_parallel = false;
    for (uint16_t i = 0; i < pool->worker_count; i++) {
        pool->workers[i].context.scheduler_parallel = false;
    }
}
