#include "siecs.h"
#include "sihttp.h"
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

    return ecs_system_index_create(&world->system_index, &sys);
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

#ifdef SIECS_REST
void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

void ecs_progress(ecs_world_t *world) {
    ecs_assert_not_null(world);

    for (ecs_phase_t phase = 0; phase < EcsPhaseCount; phase++) {
        ecs_run_phase(world, phase);
    }
#ifdef SIECS_REST
    if (world->features.rest) {
        sihttp_server_poll(world->server);
    }
    sleep_ms(5);
#endif
}

void ecs_enable_system(ecs_world_t *world, ecs_system_id_t system, bool enabled) {
    ecs_assert_not_null(world);

    ecs_system_t *sys = ecs_system_index_get(&world->system_index, system);
    if (sys->enabled == enabled) {
        return;
    }

    sys->enabled = enabled;
    world->system_index.plan_dirty = true;
}
