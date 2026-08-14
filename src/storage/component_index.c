#include "component_index.h"
#include "sicore.h"
#include "siecs.h"
#include "sireflect.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

sicore_vec_t ecs_component_default_relation_index;
ecs_component_index_t component_index;

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

void ecs_component_index_register(
    ecs_component_t id,
    const char *name,
    uint64_t size,
    ecs_type_ops_t ops,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    ecs_component_inheritance_t inheritance,
    uint32_t relation_flags,
    sireflect_handle_t type,
    const sireflect_struct_desc_t *reflection_desc
) {
    sicore_vec_ensure(
        &component_index.components,
        (uint32_t)id + 1,
        sizeof(ecs_component_record_t)
    );

    ecs_component_record_t *existing =
        sicore_vec_get_mut(&component_index.components, id, ecs_component_record_t);
    if (existing->tables.data) {
        return;
    }

    ecs_component_info_t *info = malloc(sizeof *info);
    if (!info) {
        abort();
    }
    sireflect_struct_desc_t *reflection = ecs_component_reflection_desc_copy(reflection_desc);
    *info = (ecs_component_info_t){
        .name = name ? strdup(name) : NULL,
        .size = size,
        .type = type,
        .reflection = reflection,
        .inheritance = inheritance,
    };
    if (name && !info->name) {
        abort();
    }

    ecs_component_record_t record = {
        .info = info,
        .required = NULL,
        .required_count = 0,
        .ops = ops,
        .on_set = on_set,
        .on_remove = on_remove,
        .on_add = on_add,
        .relation_flags = relation_flags,
        .tables = { 0 },
    };
    sicore_vec_init(&record.tables, sizeof(uint16_t));

    *existing = record;
}

void ecs_component_index_init() {
    sicore_vec_init_w_size(&component_index.components, sizeof(ecs_component_record_t), 256);
}

void ecs_component_index_fini() {
    ecs_component_record_t *records = component_index.components.data;

    for (uint32_t i = 0; i < component_index.components.size; i++) {
        if (records[i].info) {
            free((char *)records[i].info->name);

            if (records[i].info->reflection) {
                free((char *)records[i].info->reflection->name);
                free((char *)records[i].info->reflection->fields);
                free((void *)records[i].info->reflection);
            }

            free(records[i].info);
        }
        free(records[i].required);
        sicore_vec_fini(&records[i].tables);
    }
    if (ecs_component_default_relation_index.data) {
        ecs_component_required_relation_t **defaults = ecs_component_default_relation_index.data;
        for (uint32_t i = 0; i < ecs_component_default_relation_index.size; i++) {
            free(defaults[i]);
        }
        sicore_vec_fini(&ecs_component_default_relation_index);
    }
    ecs_component_default_relation_index = (sicore_vec_t){ 0 };
    sicore_vec_fini(&component_index.components);
}

ecs_component_record_t *ecs_component_index_get(ecs_component_t cid) {
    return sicore_vec_get_mut(&component_index.components, cid, ecs_component_record_t);
}
