#include "component_index.h"
#include "ecs/datastructure/vec.h"
#include "ecs/world.h"
#include <stdlib.h>

ecs_component_t ecs_component_index_create(
    ecs_component_index_t *index,
    const char *name,
    uint64_t size,
    bool is_bitset
) {
    ecs_component_record_t record = {
        .name = name,
        .required = NULL,
        .required_count = 0,
        .size = is_bitset ? UINT32_MAX : size,
    };

    ecs_vec_push(&index->components, &record, sizeof(ecs_component_record_t));
    return index->components.size - 1;
}

void ecs_component_index_init(ecs_component_index_t *index) {
    ecs_vec_init(&index->components, sizeof(ecs_component_record_t));
}

void ecs_component_index_fini(ecs_component_index_t *index) {
    ecs_component_record_t *records = index->components.data;

    for (uint32_t i = 0; i < index->components.size; i++) {
        free(records[i].required);
    }
    ecs_vec_fini(&index->components);
}
