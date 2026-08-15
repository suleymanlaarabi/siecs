#include "system_index.h"
#include "../utils.h"
#include "../world_internal.h"
#include <stdint.h>
#include <string.h>

ecs_system_index_t system_index;

static inline bool ecs_system_id_valid(ecs_system_id_t id) {
    return id && id < system_index.systems.size;
}

static inline ecs_system_t *ecs_system_get_unchecked(ecs_system_id_t id) {
    return sicore_vec_get_mut(&system_index.systems, id, ecs_system_t);
}

static uint32_t ecs_phase_order_index(ecs_phase_t phase) {
    const ecs_phase_t *order = system_index.phase_order.data;
    for (uint32_t i = 0; i < system_index.phase_order.size; i++) {
        if (order[i] == phase) return i;
    }
    return UINT32_MAX;
}

ecs_phase_info_t *ecs_system_index_get_phase(ecs_phase_t phase) {
    return phase < system_index.phases.size
               ? sicore_vec_get_mut(&system_index.phases, phase, ecs_phase_info_t)
               : NULL;
}

ecs_phase_t ecs_phase_register(const ecs_phase_desc_t *desc) {
    ecs_system_index_t *index = &system_index;
    ecs_phase_t id = (ecs_phase_t)index->phases.size;
    ecs_phase_t after = desc ? desc->after : ECS_PHASE_NONE;
    ecs_phase_t before = desc ? desc->before : ECS_PHASE_NONE;
    ecs_assert(after == ECS_PHASE_NONE || after < id, "invalid phase dependency\n");
    ecs_assert(before == ECS_PHASE_NONE || before < id, "invalid phase dependency\n");
    bool is_start = (after != ECS_PHASE_NONE &&
                     ecs_phase_order_index(after) < index->start_phase_count) ||
                    (before != ECS_PHASE_NONE &&
                     ecs_phase_order_index(before) < index->start_phase_count);

    if (id >= EcsPhaseCount && after == ECS_PHASE_NONE && before == ECS_PHASE_NONE) {
        after = EcsOnUpdate;
        before = EcsPostUpdate;
    }

    uint32_t end = is_start ? index->start_phase_count : index->phase_order.size;
    uint32_t insert = end;
    if (after != ECS_PHASE_NONE) insert = ecs_phase_order_index(after) + 1;
    if (before != ECS_PHASE_NONE) {
        uint32_t before_index = ecs_phase_order_index(before);
        if (after == ECS_PHASE_NONE || before_index < insert) insert = before_index;
    }
    ecs_assert(insert >= (is_start ? 0 : index->start_phase_count) && insert <= end,
               "phase dependency crosses start boundary\n");

    ecs_phase_info_t info = {
        .id = id,
        .name = desc && desc->name ? desc->name : "unnamed",
    };
    sicore_vec_init(&info.systems, sizeof(ecs_system_id_t));
    sicore_vec_push(&index->phases, &info, sizeof info);
    uint32_t old_count = index->phase_order.size;
    sicore_vec_push(&index->phase_order, &id, sizeof id);
    ecs_phase_t *order = index->phase_order.data;
    memmove(order + insert + 1, order + insert, (old_count - insert) * sizeof *order);
    order[insert] = id;
    if (is_start) index->start_phase_count++;
    index->plan_dirty = true;
    return id;
}

void ecs_system_index_init(void) {
    ecs_system_index_t *index = &system_index;
    sicore_vec_init(&index->systems, sizeof(ecs_system_t));
    sicore_vec_ensure(&index->systems, 1, sizeof(ecs_system_t));
    sicore_vec_init(&index->phases, sizeof(ecs_phase_info_t));
    sicore_vec_init(&index->phase_order, sizeof(ecs_phase_t));
    sicore_vec_init(&index->execution_order, sizeof(ecs_system_id_t));

    static const ecs_phase_desc_t phases[] = {
        { "EcsPreStart", ECS_PHASE_NONE, ECS_PHASE_NONE },
        { "EcsStart", EcsPreStart, ECS_PHASE_NONE },
        { "EcsPostStart", EcsStart, ECS_PHASE_NONE },
        { "EcsOnLoad", ECS_PHASE_NONE, ECS_PHASE_NONE },
        { "EcsPostLoad", EcsOnLoad, ECS_PHASE_NONE },
        { "EcsPreUpdate", EcsPostLoad, ECS_PHASE_NONE },
        { "EcsOnUpdate", EcsPreUpdate, ECS_PHASE_NONE },
        { "EcsPostUpdate", EcsOnUpdate, ECS_PHASE_NONE },
        { "EcsPreRender", EcsPostUpdate, ECS_PHASE_NONE },
        { "EcsOnRender", EcsPreRender, ECS_PHASE_NONE },
        { "EcsPostRender", EcsOnRender, ECS_PHASE_NONE },
    };
    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        ecs_phase_register(&phases[i]);
        if (i == EcsPostStart) index->start_phase_count = 3;
    }
}

static void ecs_access_add(uint32_t *accesses, uint16_t *count, uint16_t id, bool write) {
    uint16_t at = 0;
    while (at < *count && (accesses[at] & UINT16_MAX) < id) at++;
    if (at < *count && (accesses[at] & UINT16_MAX) == id) {
        accesses[at] |= (uint32_t)write << 16;
        return;
    }
    memmove(accesses + at + 1, accesses + at, (*count - at) * sizeof *accesses);
    accesses[at] = id | ((uint32_t)write << 16);
    (*count)++;
}

ecs_system_id_t ecs_system_index_create(const ecs_system_desc_t *desc, ecs_query_id_t qid) {
    ecs_system_index_t *index = &system_index;
    ecs_system_t system = {
        .name = desc->name,
        .qid = qid,
        .callback = desc->callback,
        .user_data = desc->user_data,
        .user_data_dtor = desc->user_data_dtor,
        .phase = desc->phase,
        .next_module = UINT16_MAX,
        .enabled = !desc->disabled,
        .main_thread_only = desc->main_thread_only,
    };
    for (uint16_t i = 0; i < ECS_SYSTEM_AFTER_CAPACITY && desc->after[i]; i++) {
#ifndef NDEBUG
        ecs_assert(ecs_system_id_valid(desc->after[i]), "invalid system dependency: %u\n",
                   desc->after[i]);
        ecs_assert(ecs_system_get_unchecked(desc->after[i])->phase == system.phase,
                   "system dependency must be in the same phase\n");
#endif
        if (desc->after[i] > system.after) system.after = desc->after[i];
    }
    for (uint16_t i = 0; i < ECS_SYSTEM_RESOURCE_CAPACITY && desc->read_resources[i]; i++)
        ecs_access_add(system.resource_accesses, &system.resource_access_count,
                       desc->read_resources[i], false);
    for (uint16_t i = 0; i < ECS_SYSTEM_RESOURCE_CAPACITY && desc->write_resources[i]; i++)
        ecs_access_add(system.resource_accesses, &system.resource_access_count,
                       desc->write_resources[i], true);

    sicore_vec_push(&index->systems, &system, sizeof system);
    ecs_system_id_t id = index->systems.size - 1;
    sicore_vec_push_u16(&ecs_system_index_get_phase(system.phase)->systems, id);
    index->plan_dirty = true;
    return id;
}

ecs_system_t *ecs_system_index_get(ecs_system_id_t system) {
    ecs_assert(ecs_system_id_valid(system), "invalid system id: %u\n", system);
    return ecs_system_get_unchecked(system);
}

static bool ecs_access_conflict(
    const uint32_t *a, uint16_t a_count, const uint32_t *b, uint16_t b_count
) {
    uint16_t ai = 0, bi = 0;
    while (ai < a_count && bi < b_count) {
        uint16_t aid = a[ai], bid = b[bi];
        if (aid < bid) ai++;
        else if (bid < aid) bi++;
        else if ((a[ai] | b[bi]) >> 16) return true;
        else { ai++; bi++; }
    }
    return false;
}

static bool ecs_query_tables_overlap(const ecs_query_cache_t *a, const ecs_query_cache_t *b) {
    const uint16_t *a_ids = a->table_ids.data, *b_ids = b->table_ids.data;
    for (uint32_t ai = 0; ai < a->table_ids.size; ai++)
        for (uint32_t bi = 0; bi < b->table_ids.size; bi++)
            if (a_ids[ai] == b_ids[bi]) return true;
    return false;
}

static bool ecs_system_conflict(const ecs_system_t *a, const ecs_system_t *b) {
    if (a->main_thread_only || b->main_thread_only ||
        ecs_access_conflict(a->resource_accesses, a->resource_access_count,
                            b->resource_accesses, b->resource_access_count)) return true;
    if (a->qid == UINT16_MAX || b->qid == UINT16_MAX) return false;
    const ecs_query_cache_t *aq = sicore_vec_get(&query_index.queries, a->qid, ecs_query_cache_t);
    const ecs_query_cache_t *bq = sicore_vec_get(&query_index.queries, b->qid, ecs_query_cache_t);
    return ecs_access_conflict(aq->accesses, aq->query.access_count,
                               bq->accesses, bq->query.access_count) &&
           ecs_query_tables_overlap(aq, bq);
}

void ecs_system_index_build_plan(void) {
    ecs_system_index_t *index = &system_index;
    sicore_vec_clear(&index->execution_order);
    for (uint32_t p = 0; p < index->phase_order.size; p++) {
        ecs_phase_info_t *phase = ecs_system_index_get_phase(
            *sicore_vec_get(&index->phase_order, p, ecs_phase_t));
        phase->plan_first = index->execution_order.size;
        uint32_t batch_first = phase->plan_first;
        const ecs_system_id_t *systems = phase->systems.data;
        for (uint32_t i = 0; i < phase->systems.size; i++) {
            ecs_system_id_t id = systems[i];
            ecs_system_t *current = ecs_system_index_get(id);
            if (!current->enabled) continue;
            bool blocked = false;
            const ecs_system_id_t *order = index->execution_order.data;
            for (uint32_t j = batch_first; j < index->execution_order.size && !blocked; j++) {
                ecs_system_id_t previous_id = order[j];
                blocked |= current->after == previous_id;
                blocked |= ecs_system_conflict(current, ecs_system_index_get(previous_id));
            }
            if (blocked) {
                sicore_vec_push_u16(&index->execution_order, 0);
                batch_first = index->execution_order.size;
            }
            sicore_vec_push_u16(&index->execution_order, id);
        }
        phase->plan_count = index->execution_order.size - phase->plan_first;
    }
    index->plan_dirty = false;
}

void ecs_system_index_fini(void) {
    ecs_system_index_t *index = &system_index;
    ecs_system_t *systems = index->systems.data;
    for (uint32_t i = 1; i < index->systems.size; i++)
        if (systems[i].user_data_dtor) systems[i].user_data_dtor(systems[i].user_data);
    for (uint32_t i = 0; i < index->phases.size; i++) {
        ecs_phase_info_t *phase = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
        sicore_vec_fini(&phase->systems);
    }
    sicore_vec_fini(&index->execution_order);
    sicore_vec_fini(&index->phase_order);
    sicore_vec_fini(&index->phases);
    sicore_vec_fini(&index->systems);
    *index = (ecs_system_index_t){ 0 };
}
