#ifndef SIECS_STORAGE_ENTITY_INDEX_H
#define SIECS_STORAGE_ENTITY_INDEX_H
#include "helper.h"
#include "siecs.h"
#include <stdatomic.h>
#include <stdint.h>

#define ECS_ENTITY_PAGE_BITS 16
#define ECS_ENTITY_PAGE_SIZE (UINT32_C(1) << ECS_ENTITY_PAGE_BITS)
#define ECS_ENTITY_PAGE_COUNT (UINT32_C(1) << (32 - ECS_ENTITY_PAGE_BITS))
#define ECS_ENTITY_RESERVATION_SIZE 256

typedef struct {
    uint16_t generation;
    _Atomic uint16_t table_id;
    // Reserved entities have table 0 and row UINT32_MAX until publication.
    uint32_t table_row;
} ecs_entity_record_t;

_Static_assert(sizeof(ecs_entity_record_t) == 8, "entity records must remain compact");

typedef struct {
    uint32_t next, end;
    uint32_t recycled_begin, recycled_end;
    bool recycled_exhausted;
} ecs_entity_reservation_t;

typedef struct {
    _Atomic(ecs_entity_record_t *) *pages;
    sicore_vec_t available;
    uint32_t available_snapshot;
    _Alignas(64) atomic_uint count;
    _Alignas(64) atomic_int_fast64_t available_cursor;
} ecs_entity_index_t;

extern ecs_entity_index_t entity_index;

void ecs_entity_index_init(void);
void ecs_entity_index_fini(void);
ecs_entity_t ecs_entity_index_reserve(bool reuse, ecs_entity_reservation_t *reservation);
void ecs_entity_index_begin_parallel(void);
void ecs_entity_index_end_parallel(void);
void ecs_entity_index_return(ecs_entity_reservation_t *reservation);
void ecs_entity_materialize(ecs_entity_t entity);

static inline uint32_t ecs_entity_index_count(void) {
    return atomic_load_explicit(&entity_index.count, memory_order_relaxed);
}

static inline ecs_entity_record_t *ecs_entity_index_get_record(uint32_t id) {
    ecs_entity_record_t *page =
        atomic_load_explicit(&entity_index.pages[id >> ECS_ENTITY_PAGE_BITS], memory_order_acquire);
    return page ? page + (id & (ECS_ENTITY_PAGE_SIZE - 1)) : NULL;
}

static inline uint16_t ecs_entity_record_table(const ecs_entity_record_t *record) {
    return atomic_load_explicit(&record->table_id, memory_order_relaxed);
}

static inline bool ecs_entity_index_is_alive(ecs_entity_t entity) {
    uint32_t id = ecs_first(entity);
    if (!id || id >= ecs_entity_index_count())
        return false;
    const ecs_entity_record_t *record = ecs_entity_index_get_record(id);
    return record && atomic_load_explicit(&record->table_id, memory_order_acquire) != UINT16_MAX &&
           record->generation == ecs_second(entity);
}

#endif
