#include "entity_index.h"

ecs_entity_index_t entity_index;

bool ecs_entity_index_is_alive(ecs_entity_t entity) {
    return ecs_entity_index_get_record(ecs_first(entity))->generation == ecs_second(entity);
}

void ecs_entity_index_init() {
    ecs_entity_index_t *index = &entity_index;
    sicore_vec_init_w_size(&index->entities, sizeof(ecs_entity_record_t), 256);
    index->first_available = UINT32_MAX;
}

void ecs_entity_index_fini() {
    sicore_vec_fini(&entity_index.entities);
    entity_index = (ecs_entity_index_t){ 0 };
}
