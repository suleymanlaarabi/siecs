#ifndef SIECS_STORAGE_ENTITY_INDEX_H
#define SIECS_STORAGE_ENTITY_INDEX_H
#include "../datastructure/vec.h"
#include "helper.h"
#include "siecs.h"
#include <stdint.h>

typedef struct {
    uint16_t generation;
    uint16_t table_id;
    // Alive records store the row in their table. Dead records reuse this field
    // as the next entity id in the free list headed by first_available.
    uint32_t table_row;
} ecs_entity_record_t;

typedef struct {
    ecs_vec_t entities;       // ecs_entity_record_t
    uint32_t first_available; // UINT32_MAX when no dead entity can be reused
} ecs_entity_index_t;

#define ecs_entity_index_get_record(index, entity_id)                                              \
    ecs_vec_get_mut((&(index)->entities), entity_id, ecs_entity_record_t)

static inline ecs_entity_t ecs_entity_index_create(ecs_entity_index_t *index, uint32_t row) {
    uint32_t entity_id;
    uint32_t generation;
    if (index->first_available != UINT32_MAX) {
        entity_id = index->first_available;
        ecs_entity_record_t *record = ecs_entity_index_get_record(index, entity_id);
        index->first_available = record->table_row;
        generation = record->generation;
        record->table_id = 0;
        record->table_row = row;
    } else {
        entity_id = index->entities.size;
        generation = 0;
        ecs_entity_record_t *record = (ecs_entity_record_t *)
            ecs_vec_push_empty(&index->entities, sizeof(ecs_entity_record_t));
        *record = (ecs_entity_record_t){ .generation = 0, .table_row = row, .table_id = 0 };
    }
    return ecs_entity(entity_id, generation);
}

static inline bool ecs_entity_index_is_alive(const ecs_entity_index_t *index, ecs_entity_t entity) {
    return ecs_entity_index_get_record(index, ecs_first(entity))->generation == ecs_second(entity);
}

static inline void ecs_entity_index_kill(ecs_entity_index_t *index, uint32_t entity_id) {
    ecs_entity_record_t *record = ecs_entity_index_get_record(index, entity_id);
    record->generation += 1;
    record->table_row = index->first_available;
    record->table_id = UINT16_MAX;
    index->first_available = entity_id;
}

void ecs_entity_index_init(ecs_entity_index_t *index);
void ecs_entity_index_fini(ecs_entity_index_t *index);

#endif
