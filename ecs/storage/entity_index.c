#include "entity_index.h"

void ecs_entity_index_init(ecs_entity_index_t *index) {
    ecs_vec_init(&index->entities, sizeof(ecs_entity_record_t));
    ecs_vec_init(&index->available, sizeof(uint32_t));
}

void ecs_entity_index_fini(ecs_entity_index_t *index) {
    ecs_vec_fini(&index->entities);
    ecs_vec_fini(&index->available);
}
