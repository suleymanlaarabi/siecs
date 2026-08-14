#include "resource_index.h"
#include "../utils.h"
#include "../world_internal.h"
#include "siecs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

ecs_resource_index_t resource_index;

static uint64_t ecs_resource_storage_size(const ecs_resource_record_t *record) {
    return record->size ? record->size : 1;
}

static bool ecs_resource_index_registered(const ecs_resource_index_t *index, ecs_resource_t id) {
    return index->records[id].name != NULL;
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

    for (uint64_t i = index->capacity; i < capacity; i++) {
        records[i] = (ecs_resource_record_t){ 0 };
    }

    index->records = records;
    index->capacity = capacity;
}

void ecs_resource_index_init() {
    ecs_resource_index_t *index = &resource_index;
    index->records = NULL;
    sicore_vec_init(&index->registration_order, sizeof(ecs_resource_t));
    index->capacity = 0;
    index->count = 1;
}

void ecs_resource_index_fini() {
    ecs_resource_index_t *index = &resource_index;
    const ecs_resource_t *ids = sicore_vec_data(&index->registration_order, ecs_resource_t);
    for (uint32_t i = index->registration_order.size; i > 0; i--) {
        if (ids[i - 1] < index->capacity &&
            index->records[ids[i - 1]].data != NULL) {
            ecs_resource_index_remove(ids[i - 1]);
        }
    }

    sicore_vec_fini(&index->registration_order);
    free(index->records);
    *index = (ecs_resource_index_t){ 0 };
}

ecs_resource_t ecs_resource_index_register(ecs_resource_t id, const ecs_resource_desc_t *desc) {
    ecs_resource_index_t *index = &resource_index;
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);
    ecs_assert_id_valid(id);

    ecs_resource_index_ensure(index, id);
    if (ecs_resource_index_registered(index, id)) {
        return id;
    }
    index->records[id] = (ecs_resource_record_t){
        .name = desc->name,
        .size = desc->size,
        .data = NULL,
        .ops = desc->ops,
        .on_set = desc->on_set,
        .on_remove = desc->on_remove,
    };
    if (id >= index->count) {
        index->count = (uint64_t)id + 1;
    }
    sicore_vec_push(&index->registration_order, &id, sizeof(id));
    return id;
}

ecs_resource_t ecs_resource_index_find(const char *name) {
    const ecs_resource_index_t *index = &resource_index;
    ecs_assert_not_null(name);
    for (uint32_t id = 1; id < index->count; id++) {
        if (ecs_resource_index_registered(index, id) &&
            strcmp(index->records[id].name, name) == 0) {
            return id;
        }
    }
    return 0;
}

bool ecs_resource_index_is_registered(ecs_resource_t id) {
    const ecs_resource_index_t *index = &resource_index;
    return id != 0 && id < index->count && id < index->capacity &&
           ecs_resource_index_registered(index, id);
}

void ecs_resource_index_set(ecs_resource_t id, const void *data) {
    ecs_resource_index_t *index = &resource_index;
    ecs_resource_index_assert_registered(index, id);

    ecs_resource_record_t *record = &index->records[id];
    bool was_present = record->data != NULL;

    if (!was_present) {
        record->data = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(record->data);
    }

    if (record->on_set) {
        record->on_set(data);
    }

    if (record->size) {
        if (was_present) {
            if (record->ops.copy) {
                record->ops.copy(record->data, data, 1);
            } else {
                memcpy(record->data, data, record->size);
            }
        } else {
            if (record->ops.copy_ctor) {
                record->ops.copy_ctor(record->data, data, 1);
            } else {
                memcpy(record->data, data, record->size);
            }
        }
    }
}

void ecs_resource_index_move(ecs_resource_t id, void *data) {
    ecs_resource_index_t *index = &resource_index;
    ecs_resource_index_assert_registered(index, id);

    ecs_resource_record_t *record = &index->records[id];
    bool was_present = record->data != NULL;

    if (!was_present) {
        record->data = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(record->data);
    }

    if (record->on_set) {
        record->on_set(data);
    }

    if (record->size) {
        if (was_present) {
            if (record->ops.move) {
                record->ops.move(record->data, data, 1);
            } else if (record->ops.copy) {
                record->ops.copy(record->data, data, 1);
                if (record->ops.dtor) {
                    record->ops.dtor(data, 1);
                }
            } else {
                memcpy(record->data, data, record->size);
            }
        } else {
            if (record->ops.move_ctor) {
                record->ops.move_ctor(record->data, data, 1);
            } else if (record->ops.copy_ctor) {
                record->ops.copy_ctor(record->data, data, 1);
                if (record->ops.dtor) {
                    record->ops.dtor(data, 1);
                }
            } else {
                memcpy(record->data, data, record->size);
            }
        }
    }
}

void *ecs_resource_index_get(ecs_resource_t id) {
    ecs_resource_index_t *index = &resource_index;
    ecs_resource_index_assert_registered(index, id);
    return index->records[id].data;
}

bool ecs_resource_index_has(ecs_resource_t id) {
    const ecs_resource_index_t *index = &resource_index;
    ecs_resource_index_assert_registered(index, id);
    return index->records[id].data != NULL;
}

void ecs_resource_index_remove(ecs_resource_t id) {
    ecs_resource_index_t *index = &resource_index;
    ecs_resource_index_assert_registered(index, id);
    ecs_resource_record_t *record = &index->records[id];

    if (record->data == NULL) {
        return;
    }

    void *value = record->data;
    record->data = NULL;
    if (record->on_remove) {
        record->on_remove(value);
    }
    if (record->ops.dtor) {
        record->ops.dtor(value, 1);
    }

    free(value);
}
