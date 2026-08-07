#include "table_migration.h"
#include "component_require.h"
#include "world_internal.h"
#include <stdbool.h>
#include <stdlib.h>

static inline bool ecs_required_has(
    const ecs_component_t *ids,
    uint16_t count,
    ecs_component_t id
) {
    for (uint16_t i = 0; i < count; i++) {
        if (ids[i] == id) {
            return true;
        }
    }
    return false;
}

static inline void ecs_required_push(
    ecs_component_t *ids,
    uint16_t *count,
    uint16_t capacity,
    ecs_component_t id
) {
#ifndef NDEBUG
    if (*count == capacity) {
        abort();
    }
#endif
    ids[(*count)++] = id;
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
            ecs_required_has(ids, *count, required)) {
            continue;
        }

        const ecs_component_record_t *required_rec = ecs_component_index_get(required);
        ecs_collect_required_components_inner(table, required_rec, ids, count, capacity);
        if (!ecs_required_has(ids, *count, required)) {
            ecs_required_push(ids, count, capacity, required);
        }
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
    ecs_required_push(ids, &count, ECS_ADD_PLAN_MAX_COMPONENTS, cid);
    ecs_sort_component_ids(ids, count);

    uint16_t total_count = from_table->type.component_count + count;
    ecs_component_t *merged = malloc(sizeof(ecs_component_t) * total_count);

    uint16_t from_i = 0;
    uint16_t add_i = 0;
    uint16_t out_i = 0;
    while (from_i < from_table->type.component_count && add_i < count) {
        ecs_component_t from_id = from_table->type.ids[from_i];
        ecs_component_t add_id = ids[add_i];
        if (from_id < add_id) {
            merged[out_i++] = from_id;
            from_i++;
        } else {
            merged[out_i++] = add_id;
            add_i++;
        }
    }
    while (from_i < from_table->type.component_count) {
        merged[out_i++] = from_table->type.ids[from_i++];
    }
    while (add_i < count) {
        merged[out_i++] = ids[add_i++];
    }

    ecs_type_t type = ecs_type_with_ids(&from_table->type, merged, total_count);
    free(merged);
    return type;
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
