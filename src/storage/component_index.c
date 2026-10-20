#include "component_index.h"
#include "sicore.h"
#include "siecs.h"
#include "sireflect.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

ecs_component_index_t component_index;

void ecs_component_index_register(
    ecs_component_t id,
    const ecs_component_desc_t *desc,
    uint32_t relation_flags,
    sireflect_handle_t type
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
    sireflect_struct_desc_t *reflection = ecs_reflection_desc_copy(desc->struct_desc);
    *info = (ecs_component_info_t){
        .name = desc->name ? strdup(desc->name) : NULL,
        .size = desc->size,
        .type = type,
        .reflection = reflection,
        .inheritance = desc->inheritance,
        .mutation = desc->mutation,
    };
    if (desc->name && !info->name) {
        abort();
    }

    ecs_component_record_t record = {
        .info = info,
        .required = NULL,
        .required_count = 0,
        .default_relations = NULL,
        .default_relation_count = 0,
        .ops = desc->ops,
        .on_set = desc->on_set,
        .on_remove = desc->on_remove,
        .on_add = desc->on_add,
        .relation_flags = relation_flags,
        .tables = { 0 },
    };
    sicore_vec_init(&record.tables, sizeof(uint16_t));

    *existing = record;
    /* Relation implementation components belong to the relation namespace. */
    if (!relation_flags && info->name && info->name[0] &&
        sicore_map_get(&component_index.names, info->name) == UINT32_MAX) {
        sicore_map_set(&component_index.names, info->name, id);
    }
}

void ecs_component_index_init() {
    sicore_map_init(&component_index.names);
    sicore_vec_init_w_size(&component_index.components, sizeof(ecs_component_record_t), 256);
}

void ecs_component_index_fini() {
    sicore_map_fini(&component_index.names);
    ecs_component_record_t *records = component_index.components.data;

    for (uint32_t i = 0; i < component_index.components.size; i++) {
        if (records[i].info) {
            free((char *)records[i].info->name);

            ecs_reflection_desc_free(records[i].info->reflection);
            free(records[i].info);
        }
        free(records[i].required);
        free(records[i].default_relations);
        sicore_vec_fini(&records[i].tables);
    }
    sicore_vec_fini(&component_index.components);
    component_index = (ecs_component_index_t){ 0 };
}

ecs_component_record_t *ecs_component_index_get(ecs_component_t cid) {
    return sicore_vec_get_mut(&component_index.components, cid, ecs_component_record_t);
}
