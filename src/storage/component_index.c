#include "component_index.h"
#include "../datastructure/vec.h"
#include "siecs.h"
#include "sireflect.h"
#include <stdlib.h>

void ecs_component_index_register(
    ecs_component_index_t *index,
    ecs_component_t id,
    uint64_t size,
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
    if (existing->registered) {
        return;
    }

    ecs_component_record_t record = {
        .registered = true,
        .required = NULL,
        .required_count = 0,
        .size = size,
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
        if (!records[i].registered) {
            continue;
        }
        free(records[i].required);
        ecs_vec_fini(&records[i].tables);
    }
    ecs_vec_fini(&index->components);
}
