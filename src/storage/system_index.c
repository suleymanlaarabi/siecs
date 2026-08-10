#include "system_index.h"
#include "../utils.h"
#include "../world_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool ecs_system_id_valid(const ecs_system_index_t *index, ecs_system_id_t system) {
    return system != 0 && system < index->systems.size;
}

static bool ecs_system_resource_conflict(const ecs_system_t *a, const ecs_system_t *b) {
    for (uint16_t i = 0; i < ECS_SYSTEM_RESOURCE_CAPACITY && a->read_resources[i]; i++) {
        for (uint16_t j = 0; j < ECS_SYSTEM_RESOURCE_CAPACITY && b->write_resources[j]; j++) {
            if (a->read_resources[i] == b->write_resources[j]) {
                return true;
            }
        }
    }
    for (uint16_t i = 0; i < ECS_SYSTEM_RESOURCE_CAPACITY && a->write_resources[i]; i++) {
        for (uint16_t j = 0; j < ECS_SYSTEM_RESOURCE_CAPACITY && b->read_resources[j]; j++) {
            if (a->write_resources[i] == b->read_resources[j]) {
                return true;
            }
        }
        for (uint16_t j = 0; j < ECS_SYSTEM_RESOURCE_CAPACITY && b->write_resources[j]; j++) {
            if (a->write_resources[i] == b->write_resources[j]) {
                return true;
            }
        }
    }
    return false;
}

ecs_phase_info_t *ecs_system_index_get_phase(ecs_phase_t phase) {
    ecs_system_index_t *index = &ecs_world.system_index;
    if (phase >= index->phases.size) {
        return NULL;
    }
    return sicore_vec_get_mut(&index->phases, phase, ecs_phase_info_t);
}

ecs_phase_t ecs_phase_register(const ecs_phase_desc_t *desc) {
    ecs_system_index_t *index = &ecs_world.system_index;
    ecs_phase_t id = (ecs_phase_t)index->phases.size;

    ecs_phase_t after = desc ? desc->after : ECS_PHASE_NONE;
    ecs_phase_t before = desc ? desc->before : ECS_PHASE_NONE;

    bool is_start = false;
    if (after != ECS_PHASE_NONE) {
        ecs_phase_info_t *a_info = ecs_system_index_get_phase(after);
        if (a_info && a_info->is_start_phase) {
            is_start = true;
        }
    }
    if (!is_start && before != ECS_PHASE_NONE) {
        ecs_phase_info_t *b_info = ecs_system_index_get_phase(before);
        if (b_info && b_info->is_start_phase) {
            is_start = true;
        }
    }

    if (id >= 11) {
        if (after == ECS_PHASE_NONE && before == ECS_PHASE_NONE) {
            after = EcsOnUpdate;
            before = EcsPostUpdate;
        } else if (after != ECS_PHASE_NONE && before == ECS_PHASE_NONE) {
            for (uint32_t i = 0; i < id; i++) {
                ecs_phase_info_t *other = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
                if (other->is_start_phase == is_start && other->after == after) {
                    before = other->id;
                    break;
                }
            }
        } else if (before != ECS_PHASE_NONE && after == ECS_PHASE_NONE) {
            ecs_phase_info_t *b_info = ecs_system_index_get_phase(before);
            if (b_info && b_info->after != ECS_PHASE_NONE) {
                after = b_info->after;
            }
        }
    }

    ecs_phase_info_t info = {
        .id = id,
        .name = (desc && desc->name) ? desc->name : "unnamed",
        .after = after,
        .before = before,
        .is_start_phase = is_start,
    };
    sicore_vec_init(&info.systems_order, sizeof(ecs_system_id_t));
    sicore_vec_init(&info.batches, sizeof(ecs_system_batch_t));

    sicore_vec_push(&index->phases, &info, sizeof(ecs_phase_info_t));
    index->plan_dirty = true;
    return id;
}

static void ecs_system_index_plan_one(
    ecs_system_index_t *index,
    ecs_system_id_t system,
    uint8_t *state,
    sicore_vec_t *order
) {
    if (!ecs_system_id_valid(index, system)) {
        ecs_assert(false, "invalid system dependency: %u\n", system);
        return;
    }

    if (state[system] == 2) {
        return;
    }

    if (state[system] == 1) {
        ecs_assert(false, "system dependency cycle detected at system %u\n", system);
        return;
    }

    state[system] = 1;

    ecs_system_t *sys = ecs_system_index_get(system);
    for (uint32_t i = 0; i < ECS_SYSTEM_AFTER_CAPACITY; i++) {
        ecs_system_id_t after = sys->after[i];
        if (after == 0) {
            continue;
        }

        if (!ecs_system_id_valid(index, after)) {
            ecs_assert(false, "invalid system dependency: %u\n", after);
            continue;
        }

        ecs_system_t *dep = ecs_system_index_get(after);
        if (dep->phase != sys->phase) {
            ecs_assert(false, "system dependency must be in the same phase\n");
            continue;
        }

        ecs_system_index_plan_one(index, after, state, order);
    }

    state[system] = 2;

    if (sys->enabled) {
        sicore_vec_push_u16(order, system);
    }
}

void ecs_system_index_init(void) {
    ecs_system_index_t *index = &ecs_world.system_index;
    sicore_vec_init(&index->systems, sizeof(ecs_system_t));
    sicore_vec_ensure(&index->systems, 1, sizeof(ecs_system_t));

    sicore_vec_init(&index->phases, sizeof(ecs_phase_info_t));
    sicore_vec_init(&index->start_execution_order, sizeof(ecs_phase_t));
    sicore_vec_init(&index->main_execution_order, sizeof(ecs_phase_t));

    ecs_phase_register(&(ecs_phase_desc_t){ .name = "EcsPreStart",
                                            .after = ECS_PHASE_NONE,
                                            .before = ECS_PHASE_NONE });
    ecs_phase_register(
        &(ecs_phase_desc_t){ .name = "EcsStart", .after = EcsPreStart, .before = ECS_PHASE_NONE }
    );
    ecs_phase_register(
        &(ecs_phase_desc_t){ .name = "EcsPostStart", .after = EcsStart, .before = ECS_PHASE_NONE }
    );

    for (uint32_t i = 0; i <= 2; i++) {
        ecs_phase_info_t *p = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
        p->is_start_phase = true;
    }

    ecs_phase_register(&(ecs_phase_desc_t){ .name = "EcsOnLoad",
                                            .after = ECS_PHASE_NONE,
                                            .before = ECS_PHASE_NONE });
    ecs_phase_register(
        &(ecs_phase_desc_t){ .name = "EcsPostLoad", .after = EcsOnLoad, .before = ECS_PHASE_NONE }
    );
    ecs_phase_register(&(ecs_phase_desc_t){ .name = "EcsPreUpdate",
                                            .after = EcsPostLoad,
                                            .before = ECS_PHASE_NONE });
    ecs_phase_register(&(ecs_phase_desc_t){ .name = "EcsOnUpdate",
                                            .after = EcsPreUpdate,
                                            .before = ECS_PHASE_NONE });
    ecs_phase_register(&(ecs_phase_desc_t){ .name = "EcsPostUpdate",
                                            .after = EcsOnUpdate,
                                            .before = ECS_PHASE_NONE });
    ecs_phase_register(&(ecs_phase_desc_t){ .name = "EcsPreRender",
                                            .after = EcsPostUpdate,
                                            .before = ECS_PHASE_NONE });
    ecs_phase_register(&(ecs_phase_desc_t){ .name = "EcsOnRender",
                                            .after = EcsPreRender,
                                            .before = ECS_PHASE_NONE });
    ecs_phase_register(&(ecs_phase_desc_t){ .name = "EcsPostRender",
                                            .after = EcsOnRender,
                                            .before = ECS_PHASE_NONE });

    index->plan_dirty = true;
}

ecs_system_id_t ecs_system_index_create(const ecs_system_t *system) {
    ecs_system_index_t *index = &ecs_world.system_index;
    sicore_vec_push(&index->systems, system, sizeof(ecs_system_t));
    index->plan_dirty = true;
    return index->systems.size - 1;
}

ecs_system_t *ecs_system_index_get(ecs_system_id_t system) {
    ecs_system_index_t *index = &ecs_world.system_index;
    ecs_assert(ecs_system_id_valid(index, system), "invalid system id: %u\n", system);
    return sicore_vec_get_mut(&index->systems, system, ecs_system_t);
}

static void sort_phase_group(ecs_system_index_t *index, bool is_start, sicore_vec_t *out_order) {
    sicore_vec_clear(out_order);

    uint32_t total_phases = index->phases.size;
    uint32_t group_count = 0;
    for (uint32_t i = 0; i < total_phases; i++) {
        ecs_phase_info_t *p = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
        if (p->is_start_phase == is_start) {
            group_count++;
        }
    }

    if (group_count == 0)
        return;

    uint32_t in_degree_stack[32];
    bool processed_stack[32];
    uint32_t *in_degree =
        (total_phases <= 32) ? in_degree_stack : calloc(total_phases, sizeof(uint32_t));
    bool *processed = (total_phases <= 32) ? processed_stack : calloc(total_phases, sizeof(bool));
    ecs_assert_not_null(in_degree);
    ecs_assert_not_null(processed);

    if (total_phases <= 32) {
        memset(in_degree_stack, 0, total_phases * sizeof(uint32_t));
        memset(processed_stack, 0, total_phases * sizeof(bool));
    }

    for (uint32_t i = 0; i < total_phases; i++) {
        ecs_phase_info_t *p = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
        if (p->is_start_phase != is_start)
            continue;

        if (p->after != ECS_PHASE_NONE && p->after < total_phases && p->after != p->id) {
            ecs_phase_info_t *a = sicore_vec_get_mut(&index->phases, p->after, ecs_phase_info_t);
            if (a->is_start_phase == is_start) {
                in_degree[i]++;
            }
        }
        if (p->before != ECS_PHASE_NONE && p->before < total_phases && p->before != p->id) {
            ecs_phase_info_t *b = sicore_vec_get_mut(&index->phases, p->before, ecs_phase_info_t);
            if (b->is_start_phase == is_start) {
                in_degree[p->before]++;
            }
        }
    }

    for (uint32_t step = 0; step < group_count; step++) {
        int candidate = -1;
        for (uint32_t i = 0; i < total_phases; i++) {
            ecs_phase_info_t *p = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
            if (p->is_start_phase == is_start && !processed[i] && in_degree[i] == 0) {
                candidate = (int)i;
                break;
            }
        }

        if (candidate == -1) {
            ecs_assert(false, "phase dependency cycle detected\n");
            for (uint32_t i = 0; i < total_phases; i++) {
                ecs_phase_info_t *p = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
                if (p->is_start_phase == is_start && !processed[i]) {
                    candidate = (int)i;
                    break;
                }
            }
        }

        uint32_t chosen = (uint32_t)candidate;
        processed[chosen] = true;
        ecs_phase_t phase_id = (ecs_phase_t)chosen;
        sicore_vec_push(out_order, &phase_id, sizeof(ecs_phase_t));

        ecs_phase_info_t *p = sicore_vec_get_mut(&index->phases, chosen, ecs_phase_info_t);

        for (uint32_t i = 0; i < total_phases; i++) {
            ecs_phase_info_t *other = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
            if (other->is_start_phase == is_start && !processed[i]) {
                if (other->after == phase_id) {
                    if (in_degree[i] > 0)
                        in_degree[i]--;
                }
            }
        }
        if (p->before != ECS_PHASE_NONE && p->before < total_phases && p->before != chosen) {
            ecs_phase_info_t *b = sicore_vec_get_mut(&index->phases, p->before, ecs_phase_info_t);
            if (b->is_start_phase == is_start && !processed[p->before]) {
                if (in_degree[p->before] > 0)
                    in_degree[p->before]--;
            }
        }
    }

    if (total_phases > 32) {
        free(in_degree);
        free(processed);
    }
}

void ecs_system_index_build_plan(void) {
    ecs_system_index_t *index = &ecs_world.system_index;

    sort_phase_group(index, true, &index->start_execution_order);
    sort_phase_group(index, false, &index->main_execution_order);

    for (uint32_t i = 0; i < index->phases.size; i++) {
        ecs_phase_info_t *p = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
        sicore_vec_clear(&p->systems_order);
        sicore_vec_clear(&p->batches);
    }

    uint8_t *state = calloc(index->systems.size, sizeof(uint8_t));
    ecs_assert_not_null(state);

    for (uint32_t system = 1; system < index->systems.size; system++) {
        ecs_system_t *sys = ecs_system_index_get(system);
        ecs_phase_info_t *pinfo = ecs_system_index_get_phase(sys->phase);
        ecs_assert(pinfo != NULL, "invalid system phase: %u\n", sys->phase);

        if (!pinfo) {
            continue;
        }

        ecs_system_index_plan_one(index, system, state, &pinfo->systems_order);
    }

    for (uint32_t phase_id = 0; phase_id < index->phases.size; phase_id++) {
        ecs_phase_info_t *phase =
            sicore_vec_get_mut(&index->phases, phase_id, ecs_phase_info_t);
        const ecs_system_id_t *order = sicore_vec_data(&phase->systems_order, ecs_system_id_t);
        for (uint32_t i = 0; i < phase->systems_order.size; i++) {
            ecs_system_id_t system = order[i];
            ecs_system_t *current = ecs_system_index_get(system);
            bool placed = false;
            if (phase->batches.size != 0) {
                ecs_system_batch_t *batch = sicore_vec_get_mut(
                    &phase->batches,
                    phase->batches.size - 1,
                    ecs_system_batch_t
                );
                bool blocked = false;
                for (uint32_t j = 0; j < batch->count; j++) {
                    ecs_system_id_t other = order[batch->first + j];
                    ecs_system_t *previous = ecs_system_index_get(other);
                    if (ecs_system_resource_conflict(current, previous)) {
                        blocked = true;
                        break;
                    }
                    for (uint32_t a = 0; a < ECS_SYSTEM_AFTER_CAPACITY; a++) {
                        if (current->after[a] == other || previous->after[a] == system) {
                            blocked = true;
                            break;
                        }
                    }
                    if (blocked) {
                        break;
                    }

                    const ecs_query_cache_t *current_cache = NULL;
                    const ecs_query_cache_t *previous_cache = NULL;
                    if (current->qid != UINT16_MAX) {
                        current_cache = sicore_vec_get(
                            &ecs_world.query_index.queries,
                            current->qid,
                            ecs_query_cache_t
                        );
                    }
                    if (previous->qid != UINT16_MAX) {
                        previous_cache = sicore_vec_get(
                            &ecs_world.query_index.queries,
                            previous->qid,
                            ecs_query_cache_t
                        );
                    }
                    if (!current_cache || !previous_cache) {
                        continue;
                    }

                    bool tables_overlap = false;
                    const uint16_t *current_tables = current_cache->table_ids.data;
                    const uint16_t *previous_tables = previous_cache->table_ids.data;
                    for (uint32_t c = 0; c < current_cache->table_ids.size && !tables_overlap; c++) {
                        for (uint32_t p = 0; p < previous_cache->table_ids.size; p++) {
                            if (current_tables[c] == previous_tables[p]) {
                                tables_overlap = true;
                                break;
                            }
                        }
                    }
                    if (!tables_overlap) {
                        continue;
                    }

                    bool current_writes = false;
                    bool previous_writes = false;
                    for (uint16_t t = 0; t < current_cache->query.term_count; t++) {
                        ecs_term_access_t access = ecs_query_term_access(current_cache->query.terms[t]);
                        current_writes |= access == EcsOut || access == EcsInOut ||
                                          access == EcsInOutOptional;
                    }
                    for (uint16_t t = 0; t < previous_cache->query.term_count; t++) {
                        ecs_term_access_t access = ecs_query_term_access(previous_cache->query.terms[t]);
                        previous_writes |= access == EcsOut || access == EcsInOut ||
                                           access == EcsInOutOptional;
                    }
                    if (current_writes && previous_writes) {
                        blocked = true;
                        break;
                    }
                    for (uint16_t c = 0; c < current_cache->query.term_count && !blocked; c++) {
                        ecs_query_term_t current_term = current_cache->query.terms[c];
                        ecs_term_access_t current_access = ecs_query_term_access(current_term);
                        bool current_write = current_access == EcsOut || current_access == EcsInOut ||
                                              current_access == EcsInOutOptional;
                        for (uint16_t p = 0; p < previous_cache->query.term_count; p++) {
                            ecs_query_term_t previous_term = previous_cache->query.terms[p];
                            if (current_term.id != previous_term.id) {
                                continue;
                            }
                            ecs_term_access_t previous_access = ecs_query_term_access(previous_term);
                            bool previous_write = previous_access == EcsOut ||
                                                   previous_access == EcsInOut ||
                                                   previous_access == EcsInOutOptional;
                            if (current_write || previous_write) {
                                blocked = true;
                                break;
                            }
                        }
                    }
                }
                if (!blocked) {
                    batch->count++;
                    placed = true;
                }
            }
            if (!placed) {
                ecs_system_batch_t batch = {
                    .first = i,
                    .count = 1,
                };
                sicore_vec_push(&phase->batches, &batch, sizeof(batch));
            }
        }
    }

    free(state);
    index->plan_dirty = false;
}

void ecs_system_index_fini(void) {
    ecs_system_index_t *index = &ecs_world.system_index;
    ecs_system_t *systems = sicore_vec_data(&index->systems, ecs_system_t);
    for (uint32_t i = 1; i < index->systems.size; i++) {
        if (systems[i].user_data_dtor) {
            systems[i].user_data_dtor(systems[i].user_data);
        }
    }

    for (uint32_t i = 0; i < index->phases.size; i++) {
        ecs_phase_info_t *p = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
        sicore_vec_fini(&p->systems_order);
        sicore_vec_fini(&p->batches);
    }

    sicore_vec_fini(&index->phases);
    sicore_vec_fini(&index->start_execution_order);
    sicore_vec_fini(&index->main_execution_order);
    sicore_vec_fini(&index->systems);
}
