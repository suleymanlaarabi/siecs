#include "sicore.h"
#include "siecs.h"
#include "utils.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    ecs_resource_info_t *info;
    void *data;
    ecs_type_ops_t ops;
    ecs_resource_hook_t on_set;
    ecs_resource_hook_t on_remove;
    ecs_resource_t previous;
} ecs_resource_record_t;

static sicore_vec_t ecs_resources;
static ecs_resource_t ecs_last_resource;

void ecs_resource_storage_init(void) {
    sicore_vec_init_w_size(&ecs_resources, sizeof(ecs_resource_record_t), 1);
    sicore_vec_ensure(&ecs_resources, 1, sizeof(ecs_resource_record_t));
    ecs_last_resource = 0;
}

static inline ecs_resource_record_t *ecs_resource_record(ecs_resource_t id) {
    return sicore_vec_get_mut(&ecs_resources, id, ecs_resource_record_t);
}

static inline bool ecs_resource_registered(ecs_resource_t id) {
    return id != 0 && id < ecs_resources.size && ecs_resource_record(id)->info != NULL;
}

static inline void ecs_resource_assert_registered(ecs_resource_t id) {
    ecs_assert(ecs_resource_registered(id), "invalid resource id: %u\n", id);
}

static ecs_resource_t ecs_resource_alloc_id(void) {
    ecs_assert(ecs_resources.size < UINT16_MAX, "resource id overflow\n");
    return (ecs_resource_t)ecs_resources.size;
}

ecs_resource_t ecs_resource_init(const ecs_resource_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("resource registration");
    ecs_resource_t id = 0;
    return ecs_resource_register(&id, desc);
}

ecs_resource_t ecs_resource_register(ecs_resource_t *id, const ecs_resource_desc_t *desc) {
    ecs_assert_not_null(id);
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);

    if (*id && ecs_resource_registered(*id)) {
        return *id;
    }
    ecs_assert_not_scheduler_parallel("resource registration");
    sireflect_handle_t type = SIREFLECT_INVALID_HANDLE;
    if (desc->struct_desc) {
        type = sireflect_try_register_struct(desc->struct_desc);
        if (type == SIREFLECT_INVALID_HANDLE)
            return 0;
    }

    ecs_resource_info_t *info = malloc(sizeof *info);
    if (!info)
        abort();
    sireflect_struct_desc_t *reflection = NULL;
    if (desc->struct_desc) {
        reflection = malloc(sizeof *reflection);
        if (!reflection)
            abort();
        *reflection = (sireflect_struct_desc_t){
            .name = strdup(desc->struct_desc->name),
            .fields = strdup(desc->struct_desc->fields),
            .size = desc->struct_desc->size,
            .align = desc->struct_desc->align,
        };
        if (!reflection->name || !reflection->fields)
            abort();
    }
    *info = (ecs_resource_info_t){
        .name = strdup(desc->name),
        .size = desc->size,
        .type = type,
        .reflection = reflection,
    };
    if (!info->name)
        abort();

    ecs_resource_t assigned = *id ? *id : ecs_resource_alloc_id();
    sicore_vec_ensure(&ecs_resources, (uint32_t)assigned + 1, sizeof(ecs_resource_record_t));
    ecs_resource_record_t *record = ecs_resource_record(assigned);
    *record = (ecs_resource_record_t){
        .info = info,
        .data = NULL,
        .ops = desc->ops,
        .on_set = desc->on_set,
        .on_remove = desc->on_remove,
        .previous = ecs_last_resource,
    };
    ecs_last_resource = assigned;
    *id = assigned;
    return assigned;
}

ecs_resource_t ecs_resource_find(const char *name) {
    ecs_assert_not_null(name);
    ecs_resource_record_t *records = ecs_resources.data;
    for (uint32_t i = 1; i < ecs_resources.size; i++) {
        if (records[i].info && strcmp(records[i].info->name, name) == 0) {
            return (ecs_resource_t)i;
        }
    }
    return 0;
}

const char *ecs_resource_name(ecs_resource_t resource) {
    ecs_resource_assert_registered(resource);
    return ecs_resource_record(resource)->info->name;
}

const ecs_resource_info_t *ecs_resource_info(ecs_resource_t resource) {
    return ecs_resource_registered(resource) ? ecs_resource_record(resource)->info : NULL;
}

uint32_t ecs_resource_count(void) { return ecs_resources.size; }

bool ecs_resource_is_registered_rid(ecs_resource_t id) { return ecs_resource_registered(id); }

static inline void ecs_resource_store(ecs_resource_t id, void *data, bool move) {
    ecs_assert_not_null(data);
    ecs_resource_assert_registered(id);
    ecs_resource_record_t *record = ecs_resource_record(id);

    if (record->on_set) {
        record->on_set(data);
    }
    bool construct = !record->data;
    if (construct) {
        record->data = calloc(1, record->info->size ? record->info->size : 1);
        ecs_assert_not_null(record->data);
    }
    if (!record->info->size)
        return;
    ecs_type_move_t move_op = construct ? record->ops.move_ctor : record->ops.move;
    if (move && move_op) {
        move_op(record->data, data, 1);
    } else {
        ecs_type_copy_t copy_op = construct ? record->ops.copy_ctor : record->ops.copy;
        if (copy_op) {
            copy_op(record->data, data, 1);
            if (move && record->ops.dtor)
                record->ops.dtor(data, 1);
        } else
            memcpy(record->data, data, record->info->size);
    }
}

void ecs_set_resource_rid(ecs_resource_t id, const void *data) {
    ecs_resource_store(id, (void *)data, false);
}

void ecs_move_resource_rid(ecs_resource_t id, void *data) { ecs_resource_store(id, data, true); }

void *ecs_resource_rid(ecs_resource_t id) {
    ecs_resource_assert_registered(id);
    void *data = ecs_resource_record(id)->data;
    ecs_assert(data != NULL, "resource does not exist: %u\n", id);
    return data;
}

void *ecs_try_resource_rid(ecs_resource_t id) {
    ecs_resource_assert_registered(id);
    return ecs_resource_record(id)->data;
}

bool ecs_has_resource_rid(const ecs_resource_t id) {
    ecs_resource_assert_registered(id);
    return ecs_resource_record(id)->data != NULL;
}

void ecs_remove_resource_rid(ecs_resource_t id) {
    ecs_resource_assert_registered(id);
    ecs_resource_record_t *record = ecs_resource_record(id);
    void *data = record->data;
    if (!data) {
        return;
    }
    record->data = NULL;
    if (record->on_remove) {
        record->on_remove(data);
    }
    if (record->ops.dtor) {
        record->ops.dtor(data, 1);
    }
    free(data);
}

void ecs_resource_storage_fini(void) {
    for (ecs_resource_t id = ecs_last_resource; id; id = ecs_resource_record(id)->previous) {
        if (ecs_resource_record(id)->data) {
            ecs_remove_resource_rid(id);
        }
    }
    ecs_resource_record_t *records = ecs_resources.data;
    for (uint32_t i = 1; i < ecs_resources.size; i++) {
        ecs_resource_info_t *info = records[i].info;
        if (!info)
            continue;
        free((char *)info->name);
        if (info->reflection) {
            free((char *)info->reflection->name);
            free((char *)info->reflection->fields);
            free((void *)info->reflection);
        }
        free(info);
    }
    sicore_vec_fini(&ecs_resources);
}
