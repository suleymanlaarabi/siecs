#include "component_index.h"
#include "siecs.h"
#if SIECS_HAS_META && !defined(SIREFLECT_H)
#include "sireflect.h"
#endif
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

#if SIECS_HAS_META
static sireflect_struct_desc_t *
ecs_component_reflection_desc_copy(const sireflect_struct_desc_t *desc) {
    if (!desc) {
        return NULL;
    }

    sireflect_struct_desc_t *copy = malloc(sizeof *copy);
    if (!copy) {
        abort();
    }
    *copy = (sireflect_struct_desc_t){
        .name = strdup(desc->name),
        .fields = strdup(desc->fields),
        .size = desc->size,
        .align = desc->align,
    };
    if (!copy->name || !copy->fields) {
        abort();
    }
    return copy;
}
#endif

void ecs_component_index_register(
    ecs_component_t id,
#if SIECS_HAS_NAMES
    const char *name,
#endif
    uint64_t size,
    ecs_type_ops_t ops,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    uint32_t relation_flags
#if SIECS_HAS_META
    ,
    sireflect_handle_t type,
    const sireflect_struct_desc_t *reflection_desc
#endif
) {
    sicore_vec_ensure(
        &ecs_world.component_index.components,
        (uint32_t)id + 1,
        sizeof(ecs_component_record_t)
    );

    ecs_component_record_t *existing =
        sicore_vec_get_mut(&ecs_world.component_index.components, id, ecs_component_record_t);
    if (existing->tables.data) {
        return;
    }

    ecs_component_info_t *info = malloc(sizeof *info);
    if (!info) {
        abort();
    }
    *info = (ecs_component_info_t){
#if SIECS_HAS_NAMES
        .name = name ? strdup(name) : NULL,
#endif
        .size = size,
#if SIECS_HAS_META
        .type = type,
#endif
    };
#if SIECS_HAS_NAMES
    if (name && !info->name) {
        abort();
    }
#endif

    ecs_component_record_t record = {
        .info = info,
        .required = NULL,
        .required_count = 0,
        .size = size,
        .ops = ops,
        .on_set = on_set,
        .on_remove = on_remove,
        .on_add = on_add,
        .relation_flags = relation_flags,
        .tables = { 0 },
#if SIECS_HAS_META
        .reflection_desc = ecs_component_reflection_desc_copy(reflection_desc),
#endif
    };
    sicore_vec_init(&record.tables, sizeof(uint16_t));

    *existing = record;
}

void ecs_component_index_init() {
    sicore_vec_init_w_size(
        &ecs_world.component_index.components,
        sizeof(ecs_component_record_t),
        256
    );
}

void ecs_component_index_fini() {
    ecs_component_record_t *records = ecs_world.component_index.components.data;

    for (uint32_t i = 0; i < ecs_world.component_index.components.size; i++) {
        if (records[i].info) {
#if SIECS_HAS_NAMES
            free((char *)records[i].info->name);
#endif
            free(records[i].info);
        }
#if SIECS_HAS_META
        if (records[i].reflection_desc) {
            free((char *)records[i].reflection_desc->name);
            free((char *)records[i].reflection_desc->fields);
            free((void *)records[i].reflection_desc);
        }
#endif
        free(records[i].required);
        sicore_vec_fini(&records[i].tables);
    }
    sicore_vec_fini(&ecs_world.component_index.components);
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
