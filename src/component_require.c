#include "component_require.h"
#include "storage/component_index.h"
#include "world_internal.h"
#include <stdbool.h>
#include <stdlib.h>

static inline bool ecs_required_add_sorted(
    ecs_component_t *ids,
    uint16_t *count,
    uint16_t capacity,
    ecs_component_t id
) {
    uint16_t at = 0;
    while (at < *count && ids[at] < id) {
        at++;
    }
    if (at < *count && ids[at] == id) {
        return false;
    }

#ifndef NDEBUG
    if (*count == capacity) {
        abort();
    }
#endif
    for (uint16_t i = *count; i > at; i--) {
        ids[i] = ids[i - 1];
    }
    ids[at] = id;
    (*count)++;
    return true;
}

static void ecs_collect_required_components_inner(
    const ecs_table_t *table,
    const ecs_component_record_t *record,
    ecs_component_t *ids,
    uint16_t *count,
    uint16_t capacity
) {
    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t required = record->required[i];
        if (ecs_table_has_owned(table, required) ||
            !ecs_required_add_sorted(ids, count, capacity, required)) {
            continue;
        }

        const ecs_component_record_t *required_rec = ecs_component_index_get(required);
        ecs_collect_required_components_inner(table, required_rec, ids, count, capacity);
    }
}

void ecs_collect_required_components(
    const ecs_table_t *table,
    const ecs_component_t component,
    ecs_component_t *ids,
    uint16_t *count,
    const uint16_t capacity
) {
    const ecs_component_record_t *record = ecs_component_index_get(component);
    ecs_collect_required_components_inner(table, record, ids, count, capacity);
}

ecs_type_t ecs_type_with_requirements(
    ecs_table_t *from_table,
    ecs_component_t cid
) {
    ecs_component_t ids[ECS_ADD_PLAN_MAX_COMPONENTS];
    uint16_t count = 0;
    ecs_collect_required_components(
        from_table,
        cid,
        ids,
        &count,
        ECS_ADD_PLAN_MAX_COMPONENTS
    );
    ecs_required_add_sorted(ids, &count, ECS_ADD_PLAN_MAX_COMPONENTS, cid);
    return ecs_type_with_added_ids(&from_table->type, ids, count);
}

#ifndef NDEBUG
bool ecs_component_requires(const ecs_component_t component, ecs_component_t require) {
    const ecs_component_record_t *record = ecs_component_index_get(component);

    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t current = record->required[i];
        if (current == require || ecs_component_requires(current, require)) {
            return true;
        }
    }

    return false;
}
#endif
