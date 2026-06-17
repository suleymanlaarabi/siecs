#include "resource_index.h"
#include "../utils.h"
#include "siecs.h"
#include <stdlib.h>
#include <string.h>

static uint64_t ecs_resource_storage_size(const ecs_resource_desc_t *record) {
    return record->size ? record->size : 1;
}

static void ecs_resource_index_assert_registered(
    const ecs_resource_index_t *index,
    ecs_resource_t id
) {
    ecs_assert(
        id != 0 && id < index->count && id < index->capacity && index->records[id].name != NULL,
        "invalid resource id: %d\n",
        id
    );
}

static void ecs_resource_index_ensure(ecs_resource_index_t *index, ecs_resource_t id) {
    if (id < index->capacity) {
        return;
    }

    uint64_t capacity = index->capacity ? index->capacity : 16;
    while (id >= capacity) {
        capacity *= 2;
    }

    ecs_resource_desc_t *records = realloc(index->records, sizeof(ecs_resource_desc_t) * capacity);
    ecs_assert_not_null(records);
    void **data = realloc(index->data, sizeof(void *) * capacity);
    ecs_assert_not_null(data);
    bool *present = realloc(index->present, sizeof(bool) * capacity);
    ecs_assert_not_null(present);

    for (uint64_t i = index->capacity; i < capacity; i++) {
        records[i] = (ecs_resource_desc_t){ 0 };
        data[i] = NULL;
        present[i] = false;
    }

    index->records = records;
    index->data = data;
    index->present = present;
    index->capacity = capacity;
}

void ecs_resource_index_init(ecs_resource_index_t *index) {
    index->records = NULL;
    index->data = NULL;
    index->present = NULL;
    index->capacity = 0;
    index->count = 1;
}

void ecs_resource_index_fini(ecs_resource_index_t *index, ecs_world_t *world) {
    for (uint64_t id = 1; id < index->capacity; id++) {
        if (!index->present[id]) {
            continue;
        }

        const ecs_resource_desc_t *record = &index->records[id];
        if (record->on_remove) {
            record->on_remove(world, index->data[id]);
        }
        free(index->data[id]);
    }

    free(index->records);
    free(index->data);
    free(index->present);
}

ecs_resource_t ecs_resource_index_register(
    ecs_resource_index_t *index,
    const ecs_resource_desc_t *desc
) {
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);

    ecs_resource_t id = (ecs_resource_t)index->count++;
    ecs_resource_index_ensure(index, id);
    index->records[id] = *desc;
    return id;
}

void ecs_resource_index_set(
    ecs_resource_index_t *index,
    ecs_world_t *world,
    ecs_resource_t id,
    const void *data
) {
    ecs_resource_index_assert_registered(index, id);
    ecs_resource_index_ensure(index, id);

    const ecs_resource_desc_t *record = &index->records[id];
    if (!index->present[id]) {
        index->data[id] = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(index->data[id]);
        index->present[id] = true;
    }

    if (record->on_set) {
        record->on_set(world, data);
    }

    if (record->size != 0) {
        memcpy(index->data[id], data, record->size);
    }
}

void *ecs_resource_index_get(ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    if (!ecs_resource_index_has(index, id)) {
        return NULL;
    }

    return index->data[id];
}

const void *ecs_resource_index_get_const(const ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    if (!ecs_resource_index_has(index, id)) {
        return NULL;
    }

    return index->data[id];
}

bool ecs_resource_index_has(const ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    return id != 0 && id < index->capacity && index->present[id];
}

void ecs_resource_index_remove(ecs_resource_index_t *index, ecs_world_t *world, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    if (!ecs_resource_index_has(index, id)) {
        return;
    }

    const ecs_resource_desc_t *record = &index->records[id];
    if (record->on_remove) {
        record->on_remove(world, index->data[id]);
    }

    free(index->data[id]);
    index->data[id] = NULL;
    index->present[id] = false;
}
