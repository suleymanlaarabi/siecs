#include "worker_pool.h"
#include "utils.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

#define ECS_WORKER_BATCH_SIZE 4

struct ecs_worker_job_s {
    ecs_execution_context_t context;
    ecs_worker_job_t *allocated_next, *next;
    ecs_worker_t *owner;
};

typedef struct {
    uint32_t node, count;
    struct {
        uint16_t table;
        uint32_t offset, count, ordinal;
        ecs_worker_job_t *commands;
    } portions[ECS_WORKER_BATCH_SIZE];
} ecs_worker_batch_t;

static void ecs_worker_reservations(ecs_worker_pool_t *pool, bool begin) {
    if (!pool->worker_count || pool->forced)
        return;
    if (begin) {
        ecs_entity_index_begin_parallel();
    } else {
        for (uint32_t i = 0; i <= pool->worker_count; i++)
            ecs_entity_index_return(&pool->workers[i].reservation);
        ecs_entity_index_end_parallel();
    }
    pool->allocating = begin;
}

static void ecs_worker_recycle(ecs_worker_job_t *job) {
    ecs_arena_reset(&job->context.arena);
    job->next = job->owner->free_jobs;
    job->owner->free_jobs = job;
}

static void ecs_worker_enqueue(ecs_worker_pool_t *pool, uint32_t index) {
    ecs_worker_node_t *node = &pool->nodes[index];
    if (!pool->worker_count || node->queued || node->pending || node->exhausted)
        return;
    pool->ready[(pool->ready_head + pool->ready_count++) % pool->node_capacity] = index;
    node->queued = true;
    ecs_platform_condition_signal(&pool->work_condition);
}

static void ecs_worker_release(ecs_worker_pool_t *pool, uint32_t index) {
    ecs_worker_node_t *node = &pool->nodes[index];
    node->released = true;
    const sicore_vec_t *successors = &ecs_system_index_get(node->system)->successors;
    const ecs_system_id_t *ids = successors->data;
    for (uint32_t i = 0; i < successors->size; i++) {
        uint32_t successor = pool->system_to_node[ids[i]];
        if (successor == UINT32_MAX)
            continue;
        ecs_worker_node_t *next = &pool->nodes[successor];
        if (!next->started && next->pending) {
            next->pending--;
            ecs_worker_enqueue(pool, successor);
        }
    }
}

static void ecs_worker_advance(ecs_worker_pool_t *pool) {
    while (pool->cursor < pool->node_count && pool->nodes[pool->cursor].complete) {
        if (pool->nodes[pool->cursor].commands.size) {
            pool->draining = true;
            break;
        }
        pool->cursor++;
    }
    ecs_platform_condition_signal(&pool->completion_condition);
}

static void ecs_worker_complete(ecs_worker_pool_t *pool, uint32_t index) {
    ecs_worker_node_t *node = &pool->nodes[index];
    if (!node->exhausted || node->running)
        return;
    node->complete = true;
    if (!node->commands.size)
        ecs_worker_release(pool, index);
    ecs_worker_advance(pool);
}

static void ecs_worker_skip_empty_tables(ecs_worker_node_t *node, const ecs_system_t *sys) {
    const ecs_query_cache_t *cache = ecs_query_cache(sys->qid);
    while (node->next_table < cache->table_count) {
        const ecs_table_t *table = ecs_get_table(ecs_query_table_id(cache, node->next_table));
        if (node->next_row < table->entity_count)
            return;
        node->next_table++;
        node->next_row = 0;
    }
    node->exhausted = true;
}

/* Claim several portions under one scheduler lock. Small tables keep one
 * portion per claim so all available workers can participate. Contexts and
 * command storage belong to the executing worker and are reused locally. */
static bool
ecs_worker_take(ecs_worker_pool_t *pool, bool calling_thread, ecs_worker_batch_t *batch) {
    if (!pool->in_run || pool->publishing || (!calling_thread && pool->forced))
        return false;
    // Serial callbacks can flush inline, so keep their registration order.
    uint32_t candidates = pool->worker_count ? pool->ready_count : pool->cursor < pool->node_count;
    while (candidates--) {
        uint32_t index = pool->cursor;
        if (pool->worker_count) {
            index = pool->ready[pool->ready_head];
            pool->ready_head = (pool->ready_head + 1) % pool->node_capacity;
            pool->ready_count--;
        }
        ecs_worker_node_t *node = &pool->nodes[index];
        node->queued = false;
        const ecs_system_t *sys = ecs_system_index_get(node->system);
        if ((pool->draining && !node->started) || (sys->main_thread_only && !calling_thread)) {
            pool->ready[(pool->ready_head + pool->ready_count++) % pool->node_capacity] = index;
            node->queued = true;
            continue;
        }
        node->started = true;
        if (!sys->enabled)
            node->exhausted = true;
        if (sys->parallel && !node->exhausted)
            ecs_worker_skip_empty_tables(node, sys);
        if (node->exhausted) {
            ecs_worker_complete(pool, index);
            // An empty query may have readied a calling-thread-only successor.
            candidates = pool->worker_count ? pool->ready_count : pool->cursor < pool->node_count;
            continue;
        }
        *batch = (ecs_worker_batch_t){ .node = index };
        uint32_t limit = 1;
        if (sys->parallel) {
            const ecs_query_cache_t *cache = ecs_query_cache(sys->qid);
            const ecs_table_t *table = ecs_get_table(ecs_query_table_id(cache, node->next_table));
            uint32_t rows = table->entity_count - node->next_row;
            uint32_t chunks = 1 + (rows - 1) / sys->chunk_size;
            limit = chunks / (2 * (pool->worker_count + 1));
            if (!limit)
                limit = 1;
            if (limit > ECS_WORKER_BATCH_SIZE)
                limit = ECS_WORKER_BATCH_SIZE;
        }
        do {
            uint32_t at = batch->count++;
            batch->portions[at].table = node->next_table;
            batch->portions[at].offset = node->next_row;
            batch->portions[at].ordinal = node->next_ordinal++;
            if (sys->parallel) {
                const ecs_query_cache_t *cache = ecs_query_cache(sys->qid);
                const ecs_table_t *table =
                    ecs_get_table(ecs_query_table_id(cache, node->next_table));
                uint32_t rows = table->entity_count - node->next_row;
                batch->portions[at].count = rows < sys->chunk_size ? rows : sys->chunk_size;
                node->next_row += batch->portions[at].count;
                ecs_worker_skip_empty_tables(node, sys);
            } else {
                node->exhausted = true;
            }
        } while (batch->count < limit && !node->exhausted);
        node->running++;
        pool->active++;
        ecs_worker_enqueue(pool, index);
        return true;
    }
    return false;
}

static void ecs_worker_execute(ecs_worker_t *worker, ecs_worker_batch_t *batch) {
    ecs_worker_pool_t *pool = worker->pool;
    ecs_system_id_t system = pool->nodes[batch->node].system;
    const ecs_system_t *sys = ecs_system_index_get(system);
    bool exclusive = sys->main_thread_only;
    bool parallel = !exclusive && (sys->parallel || (pool->worker_count && !pool->forced));
    if (exclusive && pool->allocating)
        ecs_worker_reservations(pool, false);
    ecs_execution_context_t *previous = ecs_tls_context;
    for (uint32_t i = 0; i < batch->count; i++) {
        ecs_worker_job_t *job = worker->free_jobs;
        if (job) {
            worker->free_jobs = job->next;
        } else {
            job = calloc(1, sizeof *job);
            ecs_fatal_if(!job, "worker task allocation failed\n");
            ecs_execution_context_init(&job->context);
            job->owner = worker;
            job->allocated_next = worker->jobs;
            worker->jobs = job;
        }
        job->context.scheduler_parallel = parallel;
        job->context.entity_reservation = pool->allocating ? &worker->reservation : NULL;
        ecs_execution_context_set(&job->context);
        ecs_system_run_job(
            system,
            batch->portions[i].table,
            batch->portions[i].offset,
            batch->portions[i].count
        );
        if (job->context.commands.commands.size)
            batch->portions[i].commands = job;
        else
            ecs_worker_recycle(job);
    }
    ecs_execution_context_set(previous);
    if (exclusive)
        ecs_worker_reservations(pool, true);

    ecs_platform_mutex_lock(&pool->mutex);
    ecs_worker_node_t *node = &pool->nodes[batch->node];
    for (uint32_t i = 0; i < batch->count; i++) {
        ecs_worker_job_t *job = batch->portions[i].commands;
        if (job) {
            uint32_t ordinal = batch->portions[i].ordinal;
            sicore_vec_ensure(&node->commands, ordinal + 1, sizeof job);
            ((ecs_worker_job_t **)node->commands.data)[ordinal] = job;
        }
    }
    node->running--;
    pool->active--;
    if (exclusive && system_index.plan_dirty)
        pool->draining = true;
    ecs_worker_complete(pool, batch->node);
    ecs_platform_condition_signal(&pool->completion_condition);
    ecs_platform_mutex_unlock(&pool->mutex);
    /* No job access after unlocking: publication may now recycle it. */
}

#ifdef _WIN32
static DWORD ECS_PLATFORM_THREAD_CALL ecs_worker_loop(void *argument)
#else
static void *ecs_worker_loop(void *argument)
#endif
{
    ecs_worker_t *worker = argument;
    ecs_worker_pool_t *pool = worker->pool;
    ecs_platform_mutex_lock(&pool->mutex);
    while (!pool->stop) {
        ecs_worker_batch_t batch;
        if (!ecs_worker_take(pool, false, &batch)) {
            ecs_platform_condition_wait(&pool->work_condition, &pool->mutex);
            continue;
        }
        ecs_platform_mutex_unlock(&pool->mutex);
        ecs_worker_execute(worker, &batch);
        ecs_platform_mutex_lock(&pool->mutex);
    }
    ecs_platform_mutex_unlock(&pool->mutex);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

void ecs_worker_pool_init(ecs_worker_pool_t *pool, uint16_t requested_workers) {
    *pool = (ecs_worker_pool_t){ 0 };
    if (requested_workers == ECS_WORKERS_AUTO) {
        uint32_t cpus = ecs_platform_hardware_thread_count();
        requested_workers = cpus > 1 ? (uint16_t)(cpus - 1) : 0;
    }
    pool->worker_count = requested_workers;
    ecs_platform_mutex_init(&pool->mutex);
    ecs_platform_condition_init(&pool->work_condition);
    ecs_platform_condition_init(&pool->completion_condition);
    size_t alignment = _Alignof(ecs_worker_t);
    pool->worker_storage =
        calloc(1, ((size_t)requested_workers + 1) * sizeof *pool->workers + alignment - 1);
    ecs_fatal_if(!pool->worker_storage, "worker allocation failed\n");
    pool->workers = (ecs_worker_t *)(((uintptr_t)pool->worker_storage + alignment - 1) &
                                     ~(uintptr_t)(alignment - 1));
    for (uint32_t i = 0; i <= requested_workers; i++)
        pool->workers[i].pool = pool;
    for (uint16_t i = 0; i < requested_workers; i++) {
        bool created = ecs_platform_thread_create(
            &pool->workers[i].thread,
            ecs_worker_loop,
            &pool->workers[i]
        );
        ecs_fatal_if(!created, "failed to create ECS worker thread\n");
    }
}

void ecs_worker_pool_fini(ecs_worker_pool_t *pool) {
    ecs_platform_mutex_lock(&pool->mutex);
    ecs_fatal_if(pool->in_run, "cannot destroy workers during system execution\n");
    pool->stop = true;
    ecs_platform_condition_broadcast(&pool->work_condition);
    ecs_platform_mutex_unlock(&pool->mutex);
    for (uint16_t i = 0; i < pool->worker_count; i++)
        ecs_platform_thread_join(&pool->workers[i].thread);
    for (uint32_t i = 0; i < pool->node_capacity; i++)
        sicore_vec_fini(&pool->nodes[i].commands);
    for (uint32_t i = 0; i <= pool->worker_count; i++) {
        ecs_worker_job_t *job = pool->workers[i].jobs;
        while (job) {
            ecs_worker_job_t *next = job->allocated_next;
            ecs_execution_context_fini(&job->context);
            free(job);
            job = next;
        }
    }
    free(pool->nodes);
    free(pool->ready);
    free(pool->system_to_node);
    free(pool->worker_storage);
    ecs_platform_condition_fini(&pool->completion_condition);
    ecs_platform_condition_fini(&pool->work_condition);
    ecs_platform_mutex_fini(&pool->mutex);
    *pool = (ecs_worker_pool_t){ 0 };
}

static void ecs_worker_dependencies(ecs_worker_pool_t *pool, ecs_phase_t phase) {
    if (system_index.plan_dirty)
        ecs_system_index_build_plan();
    uint32_t system_count = system_index.systems.size;
    if (pool->system_capacity < system_count) {
        pool->system_to_node = realloc(pool->system_to_node, system_count * sizeof(uint32_t));
        ecs_fatal_if(!pool->system_to_node, "scheduler mapping allocation failed\n");
        pool->system_capacity = system_count;
    }
    memset(pool->system_to_node, 0xff, system_count * sizeof(uint32_t));
    pool->ready_count = pool->ready_head = 0;
    for (uint32_t i = 0; i < pool->node_count; i++) {
        ecs_worker_node_t *node = &pool->nodes[i];
        pool->system_to_node[node->system] = i;
        node->pending = 0;
        node->queued = false;
    }
    const ecs_phase_info_t *info = ecs_system_index_get_phase(phase);
    const ecs_system_edge_t *edges = info->dependencies.data;
    for (uint32_t e = 0; e < info->dependencies.size; e++) {
        uint32_t before = pool->system_to_node[edges[e].before];
        uint32_t after = pool->system_to_node[edges[e].after];
        if (before == UINT32_MAX || after == UINT32_MAX || pool->nodes[before].released ||
            pool->nodes[after].started)
            continue;
        pool->nodes[after].pending++;
    }
    for (uint32_t i = 0; i < pool->node_count; i++)
        ecs_worker_enqueue(pool, i);
}

static bool ecs_worker_quiescent(const ecs_worker_pool_t *pool) {
    if (pool->active)
        return false;
    for (uint32_t i = 0; i < pool->node_count; i++)
        if (pool->nodes[i].started && !pool->nodes[i].complete)
            return false;
    return true;
}

/* Only the calling thread publishes, after all started systems have finished
 * their portions. Commands retain their logical order even when workers finish
 * in a different order. Later completed systems wait for this prefix. */
static void ecs_worker_publish(ecs_worker_pool_t *pool) {
    ecs_execution_context_t *main = &ecs_world.main_context;
    ecs_execution_context_set(main);
    main->flushing_commands = true;
    while (pool->cursor < pool->node_count && pool->nodes[pool->cursor].complete) {
        ecs_worker_node_t *node = &pool->nodes[pool->cursor];
        ecs_worker_job_t **commands = node->commands.data;
        for (uint32_t i = 0; i < node->commands.size; i++) {
            ecs_worker_job_t *job = commands[i];
            if (job) {
                ecs_command_buffer_flush_buffer(&job->context.commands);
                ecs_worker_recycle(job);
            }
        }
        // Hooks can enqueue main-context work; finish that tail before the next system.
        ecs_command_buffer_flush_buffer(&main->commands);
        sicore_vec_clear(&node->commands);
        node->released = true;
        pool->cursor++;
    }
    main->flushing_commands = false;
}

void ecs_worker_pool_run(
    ecs_worker_pool_t *pool,
    const ecs_system_id_t *systems,
    uint32_t count,
    ecs_phase_t phase,
    bool forced
) {
    if (!count)
        return;
    ecs_platform_mutex_lock(&pool->mutex);
    ecs_fatal_if(pool->in_run, "nested scheduler execution is not supported\n");
    if (count > pool->node_capacity) {
        uint32_t old_capacity = pool->node_capacity;
        pool->node_capacity = count;
        pool->nodes = realloc(pool->nodes, count * sizeof *pool->nodes);
        pool->ready = realloc(pool->ready, count * sizeof *pool->ready);
        ecs_fatal_if(!pool->nodes || !pool->ready, "scheduler allocation failed\n");
        memset(pool->nodes + old_capacity, 0, (count - old_capacity) * sizeof *pool->nodes);
        for (uint32_t i = old_capacity; i < count; i++)
            sicore_vec_init(&pool->nodes[i].commands, sizeof(ecs_worker_job_t *));
    }
    pool->node_count = count;
    pool->cursor = 0;
    pool->forced = forced;
    pool->draining = false;
    for (uint32_t i = 0; i < count; i++) {
        sicore_vec_t commands = pool->nodes[i].commands;
        pool->nodes[i] = (ecs_worker_node_t){ .system = systems[i], .commands = commands };
        ecs_system_t *sys = ecs_system_index_get(systems[i]);
        if (!sys->enabled || sys->prepared_delta_time < 0.0f) {
            pool->nodes[i].exhausted = pool->nodes[i].complete = pool->nodes[i].released = true;
        }
    }
    ecs_worker_dependencies(pool, phase);
    ecs_worker_reservations(pool, true);
    pool->in_run = true;
    ecs_worker_advance(pool);
    ecs_platform_condition_broadcast(&pool->work_condition);
    while (pool->cursor < count) {
        if (pool->draining && ecs_worker_quiescent(pool)) {
            // Draining prevents workers from starting any remaining system.
            pool->publishing = true;
            ecs_platform_mutex_unlock(&pool->mutex);
            ecs_worker_reservations(pool, false);
            ecs_worker_publish(pool);
            ecs_worker_reservations(pool, true);
            ecs_platform_mutex_lock(&pool->mutex);
            pool->publishing = false;
            ecs_worker_dependencies(pool, phase);
            pool->draining = false;
            ecs_worker_advance(pool);
            ecs_platform_condition_broadcast(&pool->work_condition);
            continue;
        }
        ecs_worker_batch_t batch;
        if (ecs_worker_take(pool, true, &batch)) {
            ecs_platform_mutex_unlock(&pool->mutex);
            ecs_worker_execute(&pool->workers[pool->worker_count], &batch);
            ecs_platform_mutex_lock(&pool->mutex);
        } else if (pool->cursor < count && !(pool->draining && ecs_worker_quiescent(pool))) {
            ecs_platform_condition_wait(&pool->completion_condition, &pool->mutex);
        }
    }
    ecs_fatal_if(pool->active, "scheduler completed with callbacks still running\n");
    ecs_worker_reservations(pool, false);
    pool->in_run = false;
    ecs_platform_mutex_unlock(&pool->mutex);
    ecs_execution_context_set(&ecs_world.main_context);
}
