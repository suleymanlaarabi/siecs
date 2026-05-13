#pragma once
#include "../datastructure/vec.h"
#include "../id.h"
#include <stdint.h>

typedef struct {
    uint16_t generation;
    uint16_t table_id;
    uint32_t table_row;
} ecs_entity_record_t;

typedef struct {
    ecs_vec_t entities;  // ecs_entity_record_t
    ecs_vec_t available; // uint32_t
} ecs_entity_index_t;

#define ecs_entity_index_get_record(index, entity_id)                                              \
    ecs_vec_get_mut((&index->entities), entity_id, ecs_entity_record_t)

static inline ecs_entity_t ecs_entity_index_create(ecs_entity_index_t *index, uint32_t row) {
    uint32_t entity_id;
    uint32_t generation;
    if (index->available.size > 0) {
        entity_id = *ecs_vec_get_last(&index->available, uint32_t);
        ecs_vec_remove_last(&index->available);
        generation = ecs_entity_index_get_record(index, entity_id)->generation;
    } else {
        entity_id = index->entities.size;
        generation = 0;
        ecs_entity_record_t *record = (ecs_entity_record_t *)
            ecs_vec_push_empty(&index->entities, sizeof(ecs_entity_record_t));
        *record = (ecs_entity_record_t){ 0, .table_row = row };
    }
    return ecs_entity(entity_id, generation);
}

static inline bool ecs_entity_index_is_alive(ecs_entity_index_t *index, ecs_entity_t entity) {
    return ecs_entity_index_get_record(index, ecs_first(entity))->generation == ecs_second(entity);
}

static inline void ecs_entity_index_kill(ecs_entity_index_t *index, uint32_t entity_id) {
    ecs_entity_index_get_record(index, entity_id)->generation += 1;
    ecs_vec_push_u32(&index->available, entity_id);
}

void ecs_entity_index_init(ecs_entity_index_t *index);
void ecs_entity_index_fini(ecs_entity_index_t *index);
