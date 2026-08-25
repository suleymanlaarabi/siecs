#include "system_index.h"
#include "../helper.h"
#include "query_index.h"
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

ecs_system_id_t ecs_system_index_create(const ecs_system_desc_t *desc,
                                        ecs_query_id_t qid,
                                        bool iterates_query) {
    ecs_system_index_t *index = &system_index;
    ecs_system_t system = {
        .name = desc->name,
        .qid = qid,
        .iterates_query = iterates_query,
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

static bool ecs_query_tables_overlap(const ecs_query_cache_t *a, const ecs_query_cache_t *b) {
    for (uint16_t ai = 0; ai < a->table_count; ai++)
        for (uint16_t bi = 0; bi < b->table_count; bi++)
            if (ecs_query_table_id(a, ai) == ecs_query_table_id(b, bi)) return true;
    return false;
}

static inline bool ecs_query_access_writes(ecs_access_t access) {
    return access == EcsOut || access == EcsInOut || access == EcsInOutOptional;
}

static bool ecs_query_terms_conflict(
    const ecs_access_term_t *a, uint8_t a_count,
    const ecs_access_term_t *b, uint8_t b_count
) {
    for (uint8_t ai = 0; ai < a_count; ai++) {
        for (uint8_t bi = 0; bi < b_count; bi++) {
            if (a[ai].id == b[bi].id &&
                (ecs_query_access_writes(ecs_access_term_access(a[ai])) ||
                 ecs_query_access_writes(ecs_access_term_access(b[bi])))) {
                return true;
            }
        }
    }
    return false;
}

static bool ecs_system_conflict(const ecs_system_t *a, const ecs_system_t *b) {
    if (a->main_thread_only || b->main_thread_only) return true;
    if (a->qid == UINT16_MAX || b->qid == UINT16_MAX) return false;
    const ecs_query_cache_t *a_cache =
        sicore_vec_get(&query_index.queries, a->qid, ecs_query_cache_t);
    const ecs_query_cache_t *b_cache =
        sicore_vec_get(&query_index.queries, b->qid, ecs_query_cache_t);
    const ecs_query_t *aq = a_cache->query;
    const ecs_query_t *bq = b_cache->query;
    if (ecs_query_terms_conflict(ecs_query_resources(aq), aq->resource_count,
                                 ecs_query_resources(bq), bq->resource_count)) {
        return true;
    }
    return ecs_query_terms_conflict(ecs_query_fields(aq), aq->field_count,
                                    ecs_query_fields(bq), bq->field_count) &&
           ecs_query_tables_overlap(a_cache, b_cache);
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
