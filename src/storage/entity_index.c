#include "entity_index.h"

void ecs_entity_index_init(ecs_entity_index_t *index) {
    ecs_vec_init_w_size(&index->entities, sizeof(ecs_entity_record_t), 256);
    index->first_available = UINT32_MAX;
}

void ecs_entity_index_fini(ecs_entity_index_t *index) {
    ecs_vec_fini(&index->entities);
}
