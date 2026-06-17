#include "siecs.h"
#include "sihttp.h"
#include "module.h"
#include "storage/system_index.h"
#include "utils.h"
#include "world_internal.h"
#include <string.h>
#include <time.h>

ecs_system_id_t ecs_system_init(ecs_world_t *world, const ecs_system_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->callback);
    ecs_assert(desc->phase < EcsPhaseCount, "invalid system phase: %u\n", desc->phase);

    ecs_system_t sys = {
        .name = desc->name,
        .qid = ecs_query_init(world, &desc->query),
        .callback = desc->callback,
        .phase = desc->phase,
        .enabled = !desc->disabled,
    };

    memcpy(sys.after, desc->after, sizeof(ecs_system_id_t[4]));

    ecs_system_id_t system = ecs_system_index_create(&world->system_index, &sys);
    ecs_module_record_system(world, system);
    return system;
}

void ecs_run_system(ecs_world_t *world, ecs_system_id_t system) {
    ecs_assert_not_null(world);

    ecs_system_t *sys = ecs_system_index_get(&world->system_index, system);
    if (!sys->enabled) {
        return;
    }

    ecs_iter_t it = ecs_query_iter(world, sys->qid);
    while (ecs_iter_next(&it)) {
        sys->callback(&it);
    }
}

void ecs_run_phase(ecs_world_t *world, ecs_phase_t phase) {
    ecs_assert_not_null(world);
    ecs_assert(phase < EcsPhaseCount, "invalid system phase: %u\n", phase);

    if (phase >= EcsPhaseCount) {
        return;
    }

    ecs_system_index_t *index = &world->system_index;
    if (index->plan_dirty) {
        ecs_system_index_build_plan(index);
    }

    ecs_vec_t *order = &index->phase_order[phase];
    for (uint32_t i = 0; i < order->size; i++) {
        ecs_system_id_t system = *ecs_vec_get(order, i, ecs_system_id_t);
        ecs_run_system(world, system);
    }
}

static inline void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

bool ecs_progress(ecs_world_t *world) {
    ecs_assert_not_null(world);

    if (!world->did_start) {
        ecs_run_phase(world, EcsPreStart);
        ecs_run_phase(world, EcsStart);
        ecs_run_phase(world, EcsPostStart);
        world->did_start = true;
    }

    for (ecs_phase_t phase = EcsOnLoad; phase < EcsPhaseCount; phase++) {
        ecs_run_phase(world, phase);
    }
    if (world->features.rest) {
        sihttp_server_poll(world->server);
    }
    sleep_ms(5);

    return !world->exit;
}

void ecs_system_enable(ecs_world_t *world, ecs_system_id_t system) {
    ecs_assert_not_null(world);

    ecs_system_t *sys = ecs_system_index_get(&world->system_index, system);
    if (sys->enabled == true) {
        return;
    }

    sys->enabled = true;
    world->system_index.plan_dirty = true;
}

void ecs_system_disable(ecs_world_t *world, ecs_system_id_t system) {
    ecs_assert_not_null(world);

    ecs_system_t *sys = ecs_system_index_get(&world->system_index, system);
    if (sys->enabled == false) {
        return;
    }

    sys->enabled = false;
    world->system_index.plan_dirty = true;
}
