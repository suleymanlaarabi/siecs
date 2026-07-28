#include "resource_index.h"
#include "../utils.h"
#include "../world_internal.h"
#include "siecs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint64_t ecs_resource_storage_size(const ecs_resource_record_t *record) {
    return record->size ? record->size : 1;
}

static void ecs_resource_value_copy_ctor(
    const ecs_resource_record_t *record,
    void *dst,
    const void *src
) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, 1);
        return;
    }
    memcpy(dst, src, record->size);
}

static void
ecs_resource_value_copy(const ecs_resource_record_t *record, void *dst, const void *src) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.copy) {
        record->ops.copy(dst, src, 1);
        return;
    }
    memcpy(dst, src, record->size);
}

static void ecs_resource_value_move_ctor(ecs_resource_record_t *record, void *dst, void *src) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.move_ctor) {
        record->ops.move_ctor(dst, src, 1);
        return;
    }
    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, 1);
        if (record->ops.dtor) {
            record->ops.dtor(src, 1);
        }
        return;
    }
    memcpy(dst, src, record->size);
}

static void ecs_resource_value_move(ecs_resource_record_t *record, void *dst, void *src) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.move) {
        record->ops.move(dst, src, 1);
        return;
    }
    if (record->ops.copy) {
        record->ops.copy(dst, src, 1);
        if (record->ops.dtor) {
            record->ops.dtor(src, 1);
        }
        return;
    }
    memcpy(dst, src, record->size);
}

static void ecs_resource_value_dtor(const ecs_resource_record_t *record, void *ptr) {
    if (record->size != 0 && record->ops.dtor) {
        record->ops.dtor(ptr, 1);
    }
}

static bool
ecs_resource_index_registered(const ecs_resource_index_t *index, ecs_resource_t id) {
#if SIECS_HAS_NAMES
    return index->records[id].name != NULL;
#else
    return index->registered[id];
#endif
}

static void
ecs_resource_index_assert_registered(const ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_assert(
        id != 0 && id < index->count && id < index->capacity &&
            ecs_resource_index_registered(index, id),
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

    ecs_resource_record_t *records =
        realloc(index->records, sizeof(ecs_resource_record_t) * capacity);
    ecs_assert_not_null(records);
    void **data = realloc(index->data, sizeof(void *) * capacity);
    ecs_assert_not_null(data);
#if !SIECS_HAS_NAMES
    bool *registered = realloc(index->registered, sizeof(bool) * capacity);
    ecs_assert_not_null(registered);
#endif
    bool *present = realloc(index->present, sizeof(bool) * capacity);
    ecs_assert_not_null(present);

    for (uint64_t i = index->capacity; i < capacity; i++) {
        records[i] = (ecs_resource_record_t){ 0 };
        data[i] = NULL;
#if !SIECS_HAS_NAMES
        registered[i] = false;
#endif
        present[i] = false;
    }

    index->records = records;
    index->data = data;
#if !SIECS_HAS_NAMES
    index->registered = registered;
#endif
    index->present = present;
    index->capacity = capacity;
}

void ecs_resource_index_init() {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    index->records = NULL;
    index->data = NULL;
#if !SIECS_HAS_NAMES
    index->registered = NULL;
#endif
    index->present = NULL;
    index->capacity = 0;
    index->count = 1;
}

void ecs_resource_index_fini() {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    for (uint64_t id = 1; id < index->capacity; id++) {
        if (!index->present[id]) {
            continue;
        }

        const ecs_resource_record_t *record = &index->records[id];
        if (record->on_remove) {
            record->on_remove(index->data[id]);
        }
        ecs_resource_value_dtor(record, index->data[id]);
        free(index->data[id]);
    }

    free(index->records);
    free(index->data);
#if !SIECS_HAS_NAMES
    free(index->registered);
#endif
    free(index->present);
}

ecs_resource_t
ecs_resource_index_register(ecs_resource_t id, const ecs_resource_desc_t *desc) {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_assert_not_null(desc);
#if SIECS_HAS_NAMES
    ecs_assert_not_null(desc->name);
#endif
    ecs_assert_id_valid(id);

    ecs_resource_index_ensure(index, id);
    if (ecs_resource_index_registered(index, id)) {
        return id;
    }
    index->records[id] = (ecs_resource_record_t){
#if SIECS_HAS_NAMES
        .name = desc->name,
#endif
        .size = desc->size,
        .ops = desc->ops,
        .on_set = desc->on_set,
        .on_remove = desc->on_remove,
    };
#if !SIECS_HAS_NAMES
    index->registered[id] = true;
#endif
    if (id >= index->count) {
        index->count = (uint64_t)id + 1;
    }
    return id;
}

#if SIECS_HAS_NAMES
ecs_resource_t ecs_resource_index_find(const char *name) {
    const ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_assert_not_null(name);
    for (uint32_t id = 1; id < index->count; id++) {
        if (ecs_resource_index_registered(index, id) &&
            strcmp(index->records[id].name, name) == 0)
        {
            return id;
        }
    }
    return 0;
}
#endif

bool ecs_resource_index_is_registered(ecs_resource_t id) {
    const ecs_resource_index_t *index = &ecs_world.resource_index;
    return id != 0 && id < index->count && id < index->capacity &&
        ecs_resource_index_registered(index, id);
}

void ecs_resource_index_set(
    ecs_resource_t id,
    const void *data
) {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_resource_index_assert_registered(index, id);

    const ecs_resource_record_t *record = &index->records[id];
    bool was_present = index->present[id];
    if (!index->present[id]) {
        index->data[id] = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(index->data[id]);
        index->present[id] = true;
    }

    if (record->on_set) {
        record->on_set(data);
    }

    if (record->size != 0) {
        if (was_present) {
            ecs_resource_value_copy(record, index->data[id], data);
        } else {
            ecs_resource_value_copy_ctor(record, index->data[id], data);
        }
    }
}

void ecs_resource_index_move(
    ecs_resource_t id,
    void *data
) {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_resource_index_assert_registered(index, id);

    ecs_resource_record_t *record = &index->records[id];
    bool was_present = index->present[id];
    if (!index->present[id]) {
        index->data[id] = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(index->data[id]);
        index->present[id] = true;
    }

    if (record->on_set) {
        record->on_set(data);
    }

    if (record->size != 0) {
        if (was_present) {
            ecs_resource_value_move(record, index->data[id], data);
        } else {
            ecs_resource_value_move_ctor(record, index->data[id], data);
        }
    }
}

void *ecs_resource_index_get(ecs_resource_t id) {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_resource_index_assert_registered(index, id);
    if (!index->present[id]) {
        return NULL;
    }

    return index->data[id];
}

bool ecs_resource_index_has(ecs_resource_t id) {
    const ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_resource_index_assert_registered(index, id);
    return index->present[id];
}

void ecs_resource_index_remove(ecs_resource_t id) {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_resource_index_assert_registered(index, id);
    if (!index->present[id]) {
        return;
    }

    const ecs_resource_record_t *record = &index->records[id];
    if (record->on_remove) {
        record->on_remove(index->data[id]);
    }
    ecs_resource_value_dtor(record, index->data[id]);

    free(index->data[id]);
    index->data[id] = NULL;
    index->present[id] = false;
}
