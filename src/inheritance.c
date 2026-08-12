#include "inheritance.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "utils.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

static uint16_t ecs_inheritance_base_component_capacity(ecs_entity_t base) {
    uint32_t capacity = 0;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *table = ecs_get_table(record->table_id);
        capacity += table->type.component_count;
        base = table->type.base;
    }
    ecs_assert(capacity <= UINT16_MAX, "too many inherited components: %u\n", capacity);
    return (uint16_t)capacity;
}

static bool ecs_inheritance_type_has(
    const ecs_type_t *type,
    ecs_component_t component
) {
    uint16_t first = 0;
    uint16_t last = type->component_count;
    while (first < last) {
        uint16_t middle = (uint16_t)(first + (last - first) / 2);
        ecs_component_t current = type->ids[middle];
        if (current == component) {
            return true;
        }
        if (current < component) {
            first = (uint16_t)(middle + 1);
        } else {
            last = middle;
        }
    }
    return false;
}

static bool ecs_inheritance_component_is_owned(ecs_component_t component) {
    if (component == ecs_id(Abstract)) {
        return false;
    }

    const ecs_component_record_t *record = ecs_component_index_get(component);
    if (record->relation_flags != 0) {
        return false;
    }
    return record->info->inheritance == EcsInheritOwned;
}

static int ecs_inheritance_component_compare(const void *left, const void *right) {
    ecs_component_t a = *(const ecs_component_t *)left;
    ecs_component_t b = *(const ecs_component_t *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

void ecs_inheritance_plan_build(
    const ecs_type_t *child_type,
    ecs_entity_t base,
    ecs_inheritance_plan_t *plan
) {
    plan->ids = NULL;
    plan->count = 0;

    uint16_t capacity = ecs_inheritance_base_component_capacity(base);
    if (capacity == 0) {
        return;
    }

    ecs_component_t *ids = malloc(sizeof(ecs_component_t) * capacity);
    ecs_assert_not_null(ids);

    uint16_t count = 0;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *table = ecs_get_table(record->table_id);
        for (uint16_t i = 0; i < table->type.component_count; i++) {
            ecs_component_t component = table->type.ids[i];
            if (ecs_inheritance_component_is_owned(component) &&
                !ecs_inheritance_type_has(child_type, component)) {
                ids[count++] = component;
            }
        }
        base = table->type.base;
    }

    if (count == 0) {
        free(ids);
        return;
    }

    qsort(ids, count, sizeof(ecs_component_t), ecs_inheritance_component_compare);
    uint16_t unique = 1;
    for (uint16_t i = 1; i < count; i++) {
        if (ids[i] != ids[unique - 1]) {
            ids[unique++] = ids[i];
        }
    }

    plan->ids = ids;
    plan->count = unique;
}

void ecs_inheritance_plan_fini(ecs_inheritance_plan_t *plan) {
    free(plan->ids);
    plan->ids = NULL;
    plan->count = 0;
}

void ecs_inheritance_plan_copy(
    const ecs_inheritance_plan_t *plan,
    ecs_entity_t base,
    ecs_table_t *child_table,
    uint32_t child_row
) {
    for (uint16_t i = 0; i < plan->count; i++) {
        ecs_component_t component = plan->ids[i];
        const ecs_component_record_t *record = ecs_component_index_get(component);
        const void *source = ecs_try_get_cid(base, component);
        void *destination = ecs_table_get_component(child_table, component, child_row);
        if (record->ops.copy) {
            record->ops.copy(destination, source, 1);
        } else if (record->info->size) {
            memcpy(destination, source, record->info->size);
        }
    }
}
