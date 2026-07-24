#include "component_index.h"
#include "../datastructure/vec.h"
#include "siecs.h"
#ifndef SIREFLECT_H
#include "sireflect.h"
#endif
#include <stdlib.h>
#include <string.h>

void ecs_component_index_register(
    ecs_component_index_t *index,
    ecs_component_t id,
    uint64_t size,
    ecs_type_ops_t ops,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    uint32_t relation_flags,
    sireflect_handle_t reflection,
    const sireflect_struct_desc_t *reflection_desc
) {
    ecs_vec_ensure(&index->components, (uint32_t)id + 1, sizeof(ecs_component_record_t));

    ecs_component_record_t *existing =
        ecs_vec_get_mut(&index->components, id, ecs_component_record_t);
    if (existing->tables.data) {
        return;
    }

    ecs_component_record_t record = {
        .required = NULL,
        .required_count = 0,
        .size = size,
        .ops = ops,
        .on_set = on_set,
        .on_remove = on_remove,
        .on_add = on_add,
        .relation_flags = relation_flags,
        .tables = { 0 },
        .reflection = reflection,
        .reflection_desc = reflection_desc,
    };
    ecs_vec_init(&record.tables, sizeof(uint16_t));

    *existing = record;
}

void ecs_component_index_init(ecs_component_index_t *index) {
    ecs_vec_init_w_size(&index->components, sizeof(ecs_component_record_t), 256);
}

void ecs_component_index_fini(ecs_component_index_t *index) {
    ecs_component_record_t *records = index->components.data;

    for (uint32_t i = 0; i < index->components.size; i++) {
        free(records[i].required);
        ecs_vec_fini(&records[i].tables);
    }
    ecs_vec_fini(&index->components);
}

void ecs_component_value_ctor(const ecs_component_record_t *record, void *dst, uint32_t count) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.ctor) {
        record->ops.ctor(dst, count);
        return;
    }

    memset(dst, 0, (size_t)record->size * count);
}

void ecs_component_value_dtor(const ecs_component_record_t *record, void *ptr, uint32_t count) {
    if (record->size == 0 || count == 0 || !record->ops.dtor) {
        return;
    }

    record->ops.dtor(ptr, count);
}

void ecs_component_value_copy_ctor(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

void ecs_component_value_copy(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.copy) {
        record->ops.copy(dst, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

void ecs_component_value_move_ctor(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.move_ctor) {
        record->ops.move_ctor(dst, src, count);
        return;
    }
    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, count);
        ecs_component_value_dtor(record, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

void ecs_component_value_move(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.move) {
        record->ops.move(dst, src, count);
        return;
    }
    if (record->ops.copy) {
        record->ops.copy(dst, src, count);
        ecs_component_value_dtor(record, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}
