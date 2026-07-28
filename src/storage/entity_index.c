#include "entity_index.h"
#include "../world_internal.h"

bool ecs_entity_index_is_alive(ecs_entity_t entity) {
    return ecs_entity_index_get_record(ecs_first(entity))->generation == ecs_second(entity);
}

void ecs_entity_index_init() {
    ecs_entity_index_t *index = &ecs_world.entity_index;
    ecs_vec_init_w_size(&index->entities, sizeof(ecs_entity_record_t), 256);
    index->first_available = UINT32_MAX;
}

void ecs_entity_index_fini() { ecs_vec_fini(&ecs_world.entity_index.entities); }
