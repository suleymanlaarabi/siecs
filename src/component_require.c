#include "component_require.h"
#include "world_internal.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void ecs_add_plan_init(ecs_add_plan_t *plan) {
    plan->type = (ecs_type_t){ 0 };
    plan->added = plan->inline_added;
    plan->added_count = 0;
    plan->added_capacity = 32;
}

void ecs_add_plan_fini(ecs_add_plan_t *plan) {
    if (plan->added != plan->inline_added) {
        free(plan->added);
    }
}

static inline bool ecs_add_plan_has(const ecs_add_plan_t *plan, ecs_component_t id) {
    for (uint16_t i = 0; i < plan->added_count; i++) {
        if (plan->added[i] == id) {
            return true;
        }
    }
    return false;
}

static inline void ecs_add_plan_push(ecs_add_plan_t *plan, ecs_component_t id) {
    if (plan->added_count == plan->added_capacity) {
        const uint16_t old_capacity = plan->added_capacity;
        plan->added_capacity *= 2;
        if (plan->added == plan->inline_added) {
            plan->added = malloc(sizeof(ecs_component_t) * plan->added_capacity);
            memcpy(plan->added, plan->inline_added, sizeof(ecs_component_t) * old_capacity);
        } else {
            plan->added = realloc(plan->added, sizeof(ecs_component_t) * plan->added_capacity);
        }
    }
    plan->added[plan->added_count++] = id;
}

static void ecs_add_plan_collect_requirements(
    ecs_world_t *world,
    ecs_table_t *from_table,
    ecs_add_plan_t *plan,
    const ecs_component_record_t *crec
) {
    for (uint32_t i = 0; i < crec->required_count; i++) {
        ecs_component_t required = crec->required[i];
        if (ecs_table_has_owned(from_table, required) || ecs_add_plan_has(plan, required)) {
            continue;
        }

        const ecs_component_record_t *required_rec =
            ecs_component_index_get(&world->component_index, required);
        if (required_rec->required_count) {
            ecs_add_plan_collect_requirements(world, from_table, plan, required_rec);
        }
        ecs_add_plan_push(plan, required);
    }
}

static inline void ecs_sort_component_ids(ecs_component_t *ids, uint16_t count) {
    for (uint16_t i = 1; i < count; i++) {
        ecs_component_t id = ids[i];
        uint16_t j = i;
        while (j > 0 && ids[j - 1] > id) {
            ids[j] = ids[j - 1];
            j--;
        }
        ids[j] = id;
    }
}

void ecs_add_plan_build_type(
    ecs_world_t *world,
    ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec,
    ecs_add_plan_t *plan
) {
    ecs_add_plan_init(plan);
    ecs_add_plan_collect_requirements(world, from_table, plan, crec);
    ecs_add_plan_push(plan, cid);

    ecs_component_t inline_sorted[32];
    ecs_component_t *sorted = plan->added_count <= 32
                                  ? inline_sorted
                                  : malloc(sizeof(ecs_component_t) * plan->added_count);
    memcpy(sorted, plan->added, sizeof(ecs_component_t) * plan->added_count);
    ecs_sort_component_ids(sorted, plan->added_count);

    ecs_type_t type = {
        .ids = malloc(sizeof(ecs_component_t) * (from_table->type.count + plan->added_count)),
        .count = from_table->type.count + plan->added_count,
        .base = from_table->type.base,
    };

    uint16_t from_i = 0;
    uint16_t add_i = 0;
    uint16_t out_i = 0;
    while (from_i < from_table->type.count && add_i < plan->added_count) {
        ecs_component_t from_id = from_table->type.ids[from_i];
        ecs_component_t add_id = sorted[add_i];
        if (from_id < add_id) {
            type.ids[out_i++] = from_id;
            from_i++;
        } else {
            type.ids[out_i++] = add_id;
            add_i++;
        }
    }
    while (from_i < from_table->type.count) {
        type.ids[out_i++] = from_table->type.ids[from_i++];
    }
    while (add_i < plan->added_count) {
        type.ids[out_i++] = sorted[add_i++];
    }

    if (sorted != inline_sorted) {
        free(sorted);
    }

    plan->type = type;
}

void ecs_add_plan_build_added_only(
    ecs_world_t *world,
    ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec,
    ecs_add_plan_t *plan
) {
    ecs_add_plan_init(plan);
    ecs_add_plan_collect_requirements(world, from_table, plan, crec);
    ecs_add_plan_push(plan, cid);
}

#ifndef NDEBUG
bool ecs_component_requires(
    const ecs_world_t *world,
    ecs_component_t component,
    ecs_component_t require
) {
    const ecs_component_record_t *record =
        ecs_component_index_get(&world->component_index, component);

    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t current = record->required[i];
        if (current == require || ecs_component_requires(world, current, require)) {
            return true;
        }
    }

    return false;
}
#endif
