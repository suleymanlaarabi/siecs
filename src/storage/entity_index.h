#ifndef SIECS_STORAGE_ENTITY_INDEX_H
#define SIECS_STORAGE_ENTITY_INDEX_H
#include "helper.h"
#include "sicore_vec.h"
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
    sicore_vec_t entities;    // ecs_entity_record_t
    uint32_t first_available; // UINT32_MAX when no dead entity can be reused
} ecs_entity_index_t;

#define ecs_entity_index_get_record(entity_id)                                                     \
    sicore_vec_get_mut(&ecs_world.entity_index.entities, entity_id, ecs_entity_record_t)

bool ecs_entity_index_is_alive(ecs_entity_t entity);

void ecs_entity_index_init();
void ecs_entity_index_fini();

#endif
