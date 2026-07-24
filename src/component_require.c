#include "table_migration.h"
#include "world_internal.h"
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    ecs_component_t ids[ECS_ADD_PLAN_MAX_COMPONENTS];
    uint16_t count;
} ecs_add_plan_t;

static inline bool ecs_add_plan_has(const ecs_add_plan_t *plan, ecs_component_t id) {
    for (uint16_t i = 0; i < plan->count; i++) {
        if (plan->ids[i] == id) {
            return true;
        }
    }
    return false;
}

static inline void ecs_add_plan_push(ecs_add_plan_t *plan, ecs_component_t id) {
#ifndef NDEBUG
    if (plan->count == ECS_ADD_PLAN_MAX_COMPONENTS) {
        abort();
    }
#endif
    plan->ids[plan->count++] = id;
}

static inline void ecs_add_plan_collect_requirements(
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
            ecs_component_index_get(&ecs_world.component_index, required);
        if (required_rec->required_count) {
            ecs_add_plan_collect_requirements(from_table, plan, required_rec);
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

ecs_type_t ecs_type_with_requirements(
        ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec
) {
    ecs_add_plan_t plan = { 0 };
    ecs_add_plan_collect_requirements(from_table, &plan, crec);
    ecs_add_plan_push(&plan, cid);
    ecs_sort_component_ids(plan.ids, plan.count);

    ecs_type_t type = {
        .ids = malloc(sizeof(ecs_component_t) * (from_table->type.count + plan.count)),
        .count = from_table->type.count + plan.count,
        .base = from_table->type.base,
    };

    uint16_t from_i = 0;
    uint16_t add_i = 0;
    uint16_t out_i = 0;
    while (from_i < from_table->type.count && add_i < plan.count) {
        ecs_component_t from_id = from_table->type.ids[from_i];
        ecs_component_t add_id = plan.ids[add_i];
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
    while (add_i < plan.count) {
        type.ids[out_i++] = plan.ids[add_i++];
    }

    return type;
}

#ifndef NDEBUG
bool ecs_component_requires(
    const     ecs_component_t component,
    ecs_component_t require
) {
    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component);

    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t current = record->required[i];
        if (current == require || ecs_component_requires(current, require)) {
            return true;
        }
    }

    return false;
}
#endif
