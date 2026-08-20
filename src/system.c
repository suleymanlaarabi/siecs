#include "module.h"
#include "platform.h"
#include "siecs.h"
#include "storage/system_index.h"
#include "storage/query_index.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ECS_SYSTEM_NO_QUERY UINT16_MAX

ecs_phase_t ecs_phase_init(const ecs_phase_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("phase registration");
    return ecs_phase_register(desc);
}

const char *ecs_phase_name(ecs_phase_t phase) {
    ecs_phase_info_t *pinfo = ecs_system_index_get_phase(phase);
    return pinfo ? pinfo->name : NULL;
}

ecs_system_id_t ecs_system_init(const ecs_system_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("system registration");
    ecs_assert_not_null(desc);
    ecs_assert(desc->callback, "system requires callback function\n");
    ecs_assert(
        ecs_system_index_get_phase(desc->phase) != NULL,
        "invalid system phase: %u\n",
        desc->phase
    );

    const bool iterates_query = ecs_query_desc_tracks_tables(&desc->query);
    const bool has_query = iterates_query || desc->query.resources[0].id;
    ecs_system_id_t system = ecs_system_index_create(
        desc,
        has_query ? ecs_query_init(&desc->query) : ECS_SYSTEM_NO_QUERY,
        iterates_query
    );
    ecs_module_record_system(system);
    return system;
}

const char *ecs_system_name(ecs_system_id_t system) { return ecs_system_index_get(system)->name; }

void ecs_run_system(ecs_system_id_t system) {

    ecs_system_t *sys = ecs_system_index_get(system);
    if (!sys->enabled) {
        return;
    }

    ecs_defer_begin();
    if (sys->iterates_query) {
        ecs_iter_t it = ecs_query_iter(sys->qid);
        it.user_data = sys->user_data;
        it.delta_time = ecs_world.delta_time;
        while (ecs_iter_next(&it)) {
            sys->callback(&it);
        }
    } else {
        ecs_iter_t it = {
            .count = 1,
            .user_data = sys->user_data,
            .delta_time = ecs_world.delta_time,
        };
        sys->callback(&it);
    }
    ecs_defer_end();
}

void ecs_run_phase(ecs_phase_t phase) {
    ecs_system_index_t *index = &system_index;
    ecs_phase_info_t *pinfo = ecs_system_index_get_phase(phase);
    ecs_assert(pinfo != NULL, "invalid system phase: %u\n", phase);

    if (index->plan_dirty) {
        ecs_system_index_build_plan();
    }

    pinfo = ecs_system_index_get_phase(phase);
    if (!pinfo)
        return;

    const ecs_system_id_t *order = index->execution_order.data;
    uint32_t at = pinfo->plan_first, end = at + pinfo->plan_count;
    while (at < end) {
        uint32_t first = at;
        while (at < end && order[at])
            at++;
        uint32_t count = at - first;
        const ecs_system_id_t *systems = order + first;
        if (!ecs_worker_pool_enabled(&ecs_world.worker_pool) || count == 1) {
            ecs_world.main_context.scheduler_parallel = false;
            ecs_execution_context_set(&ecs_world.main_context);
            for (uint32_t j = 0; j < count; j++) {
                ecs_run_system(systems[j]);
            }
        } else {
            ecs_worker_pool_run_systems(&ecs_world.worker_pool, systems, count);
            ecs_worker_pool_flush(&ecs_world.worker_pool);
        }
        at++;
    }
}

bool ecs_progress(void) {
    double frame_start = ecs_platform_time_now_sec();

    if (ecs_world.last_time == 0.0) {
        ecs_world.delta_time = 0.0;
    } else {
        ecs_world.delta_time = frame_start - ecs_world.last_time;
    }

    ecs_set_resource(DeltaTime, { .value = (float)ecs_world.delta_time });

    ecs_world.last_time = frame_start;

    ecs_system_index_t *index = &system_index;
    if (index->plan_dirty) {
        ecs_system_index_build_plan();
    }

    if (!ecs_world.did_start) {
        for (uint32_t i = 0; i < index->start_phase_count; i++) {
            ecs_phase_t phase = *sicore_vec_get(&index->phase_order, i, ecs_phase_t);
            ecs_run_phase(phase);
        }
        ecs_world.did_start = true;
    }

    for (uint32_t i = index->start_phase_count; i < index->phase_order.size; i++) {
        ecs_phase_t phase = *sicore_vec_get(&index->phase_order, i, ecs_phase_t);
        ecs_run_phase(phase);
    }

    if (ecs_world.features.target_fps) {
        double target_dt = 1.0 / (double)ecs_world.features.target_fps;
        double elapsed = ecs_platform_time_now_sec() - frame_start;
        double remaining = target_dt - elapsed;

        ecs_platform_time_sleep_sec(remaining);
    }

    return !ecs_world.exit;
}

void ecs_run(void) {
    while (ecs_progress()) {
    }
    ecs_fini();
}

static void ecs_system_set_enabled(ecs_system_id_t system, bool enabled) {
    ecs_system_t *sys = ecs_system_index_get(system);
    if (sys->enabled != enabled) {
        sys->enabled = enabled;
        system_index.plan_dirty = true;
    }
}

void ecs_system_enable(ecs_system_id_t system) { ecs_system_set_enabled(system, true); }
void ecs_system_disable(ecs_system_id_t system) { ecs_system_set_enabled(system, false); }
