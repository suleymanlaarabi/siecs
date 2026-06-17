#include "resource_index.h"
#include "../utils.h"
#include "component_index.h"
#include "siecs.h"
#include <stdlib.h>
#include <string.h>

static uint64_t ecs_resource_storage_size(const ecs_component_record_t *record) {
    return record->size ? record->size : 1;
}

static void ecs_resource_index_ensure(ecs_resource_index_t *index, ecs_component_t id) {
    if (id < index->capacity) {
        return;
    }

    uint64_t capacity = index->capacity ? index->capacity : 16;
    while (id >= capacity) {
        capacity *= 2;
    }

    void **data = realloc(index->data, sizeof(void *) * capacity);
    ecs_assert_not_null(data);
    bool *present = realloc(index->present, sizeof(bool) * capacity);
    ecs_assert_not_null(present);

    for (uint64_t i = index->capacity; i < capacity; i++) {
        data[i] = NULL;
        present[i] = false;
    }

    index->data = data;
    index->present = present;
    index->capacity = capacity;
}

void ecs_resource_index_init(ecs_resource_index_t *index) {
    index->data = NULL;
    index->present = NULL;
    index->capacity = 0;
}

void ecs_resource_index_fini(
    ecs_resource_index_t *index,
    ecs_world_t *world,
    const ecs_component_index_t *component_index
) {
    for (uint64_t id = 1; id < index->capacity; id++) {
        if (!index->present[id]) {
            continue;
        }

        const ecs_component_record_t *record =
            ecs_component_index_get(component_index, (ecs_component_t)id);
        if (record->on_remove) {
            record->on_remove(world, 0, (ecs_component_t)id, index->data[id]);
        }
        free(index->data[id]);
    }

    free(index->data);
    free(index->present);
}

void ecs_resource_index_set(
    ecs_resource_index_t *index,
    ecs_world_t *world,
    const ecs_component_index_t *component_index,
    ecs_component_t id,
    const void *data
) {
    ecs_resource_index_ensure(index, id);

    const ecs_component_record_t *record = ecs_component_index_get(component_index, id);
    if (!index->present[id]) {
        index->data[id] = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(index->data[id]);
        index->present[id] = true;
    }

    if (record->on_set) {
        record->on_set(world, 0, id, data);
    }

    if (record->size != 0) {
        memcpy(index->data[id], data, record->size);
    }
}

void *ecs_resource_index_get(ecs_resource_index_t *index, ecs_component_t id) {
    if (!ecs_resource_index_has(index, id)) {
        return NULL;
    }

    return index->data[id];
}

const void *ecs_resource_index_get_const(const ecs_resource_index_t *index, ecs_component_t id) {
    if (!ecs_resource_index_has(index, id)) {
        return NULL;
    }

    return index->data[id];
}

bool ecs_resource_index_has(const ecs_resource_index_t *index, ecs_component_t id) {
    return id != 0 && id < index->capacity && index->present[id];
}

void ecs_resource_index_remove(
    ecs_resource_index_t *index,
    ecs_world_t *world,
    const ecs_component_index_t *component_index,
    ecs_component_t id
) {
    if (!ecs_resource_index_has(index, id)) {
        return;
    }

    const ecs_component_record_t *record = ecs_component_index_get(component_index, id);
    if (record->on_remove) {
        record->on_remove(world, 0, id, index->data[id]);
    }

    free(index->data[id]);
    index->data[id] = NULL;
    index->present[id] = false;
}
