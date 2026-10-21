#include "module.h"
#include "platform.h"
#include "siecs.h"
#include "storage/query_index.h"
#include "storage/system_index.h"
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
    ecs_assert(desc->interval >= 0.0, "system interval must be >= 0 seconds\n");
    ecs_assert(
        desc->phase == ECS_PHASE_NONE || ecs_system_index_get_phase(desc->phase) != NULL,
        "invalid system phase: %u\n",
        desc->phase
    );

    const bool iterates_query = desc->query.components[0].id || desc->query.relations[0].id ||
                                desc->query.order_by.func || desc->query.is_a;
    ecs_fatal_if(
        desc->no_defer && iterates_query,
        "no_defer systems cannot iterate entity queries\n"
    );
    ecs_fatal_if(
        desc->parallel && (!iterates_query || desc->main_thread_only || desc->no_defer ||
                           desc->query.order_by.func),
        "parallel requires an unordered entity query without main_thread_only or no_defer\n"
    );
    for (uint8_t i = 0;
         desc->parallel && i < ECS_QUERY_RESOURCE_CAPACITY && desc->query.resources[i].id;
         i++) {
        ecs_access_t access = ecs_access_term_access(desc->query.resources[i]);
        ecs_fatal_if(
            access == EcsOut || access == EcsInOut || access == EcsInOutOptional,
            "parallel systems cannot write resources\n"
        );
    }
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

void ecs_system_run_job(
    ecs_system_id_t system,
    uint16_t table,
    uint32_t offset,
    uint32_t count,
    uint64_t tick
) {
    // Registration may grow the index from an exclusive callback.
    ecs_system_t sys = *ecs_system_index_get(system);
    if (!sys.enabled)
        return;
    if (!sys.no_defer)
        ecs_defer_begin();
    // The scheduler records the run once every portion completed.
    ecs_iter_t it =
        sys.iterates_query ? ecs_query_iter_at(sys.qid, tick, false) : (ecs_iter_t){ .count = 1 };
    it.user_data = sys.user_data;
    it.delta_time = sys.prepared_delta_time;
    if (sys.parallel) {
        it.table_idx = table - 1;
        it.table_count = table + 1;
        it.row_offset = offset;
        it.row_limit = count;
        it.bounded = true;
    }
    if (!sys.iterates_query || sys.callback_iterates_query) {
        sys.callback(&it);
    } else {
        while (ecs_iter_next(&it))
            sys.callback(&it);
    }
    if (!sys.no_defer)
        ecs_defer_end();
}

void ecs_run_system(ecs_system_id_t system) {
    ecs_assert_not_scheduler_parallel("manual system execution");
    ecs_system_t *sys = ecs_system_index_get(system);
    if (!sys->enabled)
        return;
    float previous_delta = sys->prepared_delta_time;
    sys->prepared_delta_time = (float)ecs_world.delta_time;
    ecs_worker_pool_run(&ecs_world.worker_pool, &system, 1, sys->phase, true);
    ecs_system_index_get(system)->prepared_delta_time = previous_delta;
}

static bool ecs_system_prepare_scheduled(ecs_system_t *sys) {
    if (sys->interval == 0.0) {
        sys->prepared_delta_time = (float)ecs_world.delta_time;
        return true;
    }

    sys->interval_elapsed += ecs_world.delta_time;

    if (sys->interval_elapsed < sys->interval) {
        sys->prepared_delta_time = -1.0f;
        return false;
    }

    sys->prepared_delta_time = (float)sys->interval_elapsed;
    sys->interval_elapsed = 0.0;
    return true;
}

static void ecs_run_phase_internal(ecs_phase_t phase, bool respect_interval) {
    ecs_assert_not_scheduler_parallel("phase execution");
    ecs_system_index_t *index = &system_index;
    ecs_phase_info_t *pinfo = ecs_system_index_get_phase(phase);

    if (!pinfo) {
        return;
    }

    if (index->plan_dirty) {
        ecs_system_index_build_plan();
    }

    const ecs_system_id_t *order = index->execution_order.data;
    for (uint32_t i = 0; i < pinfo->plan_count; i++) {
        ecs_system_t *sys = ecs_system_index_get(order[pinfo->plan_first + i]);
        if (respect_interval)
            ecs_system_prepare_scheduled(sys);
        else
            sys->prepared_delta_time = (float)ecs_world.delta_time;
    }
    ecs_worker_pool_run(
        &ecs_world.worker_pool,
        order + pinfo->plan_first,
        pinfo->plan_count,
        phase,
        false
    );
}

void ecs_run_phase(ecs_phase_t phase) { ecs_run_phase_internal(phase, false); }

bool ecs_progress(void) {
    ecs_assert_not_scheduler_parallel("world progression");
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

        ecs_run_phase_internal(phase, true);
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
    ecs_assert_not_scheduler_parallel("system enable/disable");
    ecs_system_t *sys = ecs_system_index_get(system);
    if (sys->enabled != enabled) {
        sys->enabled = enabled;
        sys->interval_elapsed = 0.0;
        sys->prepared_delta_time = -1.0f;
        system_index.plan_dirty = true;
    }
}

void ecs_system_enable(ecs_system_id_t system) { ecs_system_set_enabled(system, true); }
void ecs_system_disable(ecs_system_id_t system) { ecs_system_set_enabled(system, false); }
