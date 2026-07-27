#include "entity_index.h"
#include "../world_internal.h"

ecs_entity_t ecs_entity_index_create(uint32_t row) {
    ecs_entity_index_t *index = &ecs_world.entity_index;
    uint32_t entity_id;
    uint32_t generation;
    if (index->first_available != UINT32_MAX) {
        entity_id = index->first_available;
        ecs_entity_record_t *record = ecs_entity_index_get_record(entity_id);
        index->first_available = record->table_row;
        generation = record->generation;
        record->table_id = 0;
        record->table_row = row;
    } else {
        entity_id = index->entities.size;
        generation = 0;
        ecs_entity_record_t *record =
            ecs_vec_push_empty(&index->entities, sizeof(ecs_entity_record_t));
        *record = (ecs_entity_record_t){ .generation = 0, .table_row = row, .table_id = 0 };
    }
    return ecs_entity(entity_id, generation);
}

bool ecs_entity_index_is_alive(ecs_entity_t entity) {
    return ecs_entity_index_get_record(ecs_first(entity))->generation == ecs_second(entity);
}

void ecs_entity_index_kill(uint32_t entity_id) {
    ecs_entity_index_t *index = &ecs_world.entity_index;
    ecs_entity_record_t *record = ecs_entity_index_get_record(entity_id);
    record->generation += 1;
    record->table_row = index->first_available;
    record->table_id = UINT16_MAX;
    index->first_available = entity_id;
}

void ecs_entity_index_init() {
    ecs_entity_index_t *index = &ecs_world.entity_index;
    ecs_vec_init_w_size(&index->entities, sizeof(ecs_entity_record_t), 256);
    index->first_available = UINT32_MAX;
}

void ecs_entity_index_fini() { ecs_vec_fini(&ecs_world.entity_index.entities); }
