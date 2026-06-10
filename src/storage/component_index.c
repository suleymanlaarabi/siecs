#include "component_index.h"
#include "../datastructure/map.h"
#include "../datastructure/vec.h"
#include "siecs.h"
#include "sireflect.h"
#include <stdlib.h>

ecs_component_t ecs_component_index_create(
    ecs_component_index_t *index,
    const char *name,
    uint64_t size,
    ecs_component_hook_t on_set,
    ecs_component_hook_t on_remove,
    sireflect_handle_t reflection
) {
    ecs_component_record_t record = {
        .name = name,
        .required = NULL,
        .required_count = 0,
        .size = size,
        .on_set = on_set,
        .on_remove = on_remove,
        .tables = { 0 },
        .reflection = reflection,
    };
    ecs_vec_init(&record.tables, sizeof(uint16_t));

    ecs_vec_push(&index->components, &record, sizeof(ecs_component_record_t));
#ifndef NDEBUG
    if (name) {
        ecs_map_set(&index->component_name_map, name, index->components.size - 1);
    }
#endif
    return index->components.size - 1;
}

void ecs_component_index_init(ecs_component_index_t *index) {
    ecs_vec_init(&index->components, sizeof(ecs_component_record_t));
#ifndef NDEBUG
    ecs_map_init(&index->component_name_map, 16);
#endif
}

void ecs_component_index_fini(ecs_component_index_t *index) {
    ecs_component_record_t *records = index->components.data;

    for (uint32_t i = 0; i < index->components.size; i++) {
        free(records[i].required);
        ecs_vec_fini(&records[i].tables);
    }
    ecs_vec_fini(&index->components);

#ifndef NDEBUG
    ecs_map_fini(&index->component_name_map);
#endif
}
