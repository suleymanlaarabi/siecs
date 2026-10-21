#include "entity_index.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

static ecs_entity_record_t *ecs_entity_index_ensure(uint32_t id) {
    _Atomic(ecs_entity_record_t *) *slot = &entity_index.pages[id >> ECS_ENTITY_PAGE_BITS];
    ecs_entity_record_t *page = atomic_load_explicit(slot, memory_order_acquire);
    if (!page) {
        ecs_entity_record_t *allocated = malloc(ECS_ENTITY_PAGE_SIZE * sizeof *allocated);
        ecs_fatal_if(!allocated, "entity page allocation failed\n");
        for (uint32_t i = 0; i < ECS_ENTITY_PAGE_SIZE; i++) {
            allocated[i].generation = 0;
            atomic_init(&allocated[i].table_id, UINT16_MAX);
            allocated[i].table_row = UINT32_MAX;
        }
        if (atomic_compare_exchange_strong_explicit(
                slot,
                &page,
                allocated,
                memory_order_acq_rel,
                memory_order_acquire
            ))
            page = allocated;
        else
            free(allocated);
    }
    return page + (id & (ECS_ENTITY_PAGE_SIZE - 1));
}

void ecs_entity_index_init(void) {
    entity_index.pages = malloc(ECS_ENTITY_PAGE_COUNT * sizeof *entity_index.pages);
    ecs_fatal_if(!entity_index.pages, "entity page directory allocation failed\n");
    for (uint32_t i = 0; i < ECS_ENTITY_PAGE_COUNT; i++)
        atomic_init(&entity_index.pages[i], NULL);
    sicore_vec_init(&entity_index.available, sizeof(uint32_t));
    atomic_init(&entity_index.count, 1);
    atomic_init(&entity_index.available_cursor, 0);
    ecs_entity_index_ensure(0);
}

void ecs_entity_index_fini(void) {
    for (uint32_t i = 0; i < ECS_ENTITY_PAGE_COUNT; i++)
        free(atomic_load_explicit(&entity_index.pages[i], memory_order_relaxed));
    free(entity_index.pages);
    sicore_vec_fini(&entity_index.available);
}

void ecs_entity_index_begin_parallel(void) {
    entity_index.available_snapshot = entity_index.available.size;
    atomic_store_explicit(
        &entity_index.available_cursor,
        entity_index.available.size,
        memory_order_relaxed
    );
}

void ecs_entity_index_end_parallel(void) {
    int64_t remaining = atomic_load_explicit(&entity_index.available_cursor, memory_order_relaxed);
    uint32_t prefix = remaining > 0 ? (uint32_t)remaining : 0;
    uint32_t returned = entity_index.available.size - entity_index.available_snapshot;
    if (returned) {
        uint32_t *ids = entity_index.available.data;
        memmove(ids + prefix, ids + entity_index.available_snapshot, returned * sizeof *ids);
    }
    entity_index.available.size = prefix + returned;
}

void ecs_entity_index_return(ecs_entity_reservation_t *reservation) {
    // Return unissued IDs only while every worker is quiescent. Returned IDs
    // never became alive and retain their generation.
    while (reservation->recycled_begin < reservation->recycled_end) {
        uint32_t id = ((uint32_t *)entity_index.available.data)[reservation->recycled_begin++];
        sicore_vec_push(&entity_index.available, &id, sizeof id);
    }
    while (reservation->next < reservation->end) {
        uint32_t id = reservation->next++;
        sicore_vec_push(&entity_index.available, &id, sizeof id);
    }
    *reservation = (ecs_entity_reservation_t){ 0 };
}

ecs_entity_t ecs_entity_index_reserve(bool reuse, ecs_entity_reservation_t *reservation) {
    uint32_t id;
    if (reservation) {
        if (reuse && !reservation->recycled_exhausted &&
            reservation->recycled_begin == reservation->recycled_end) {
            int64_t end = atomic_fetch_sub_explicit(
                &entity_index.available_cursor,
                ECS_ENTITY_RESERVATION_SIZE,
                memory_order_relaxed
            );
            reservation->recycled_exhausted = end <= 0;
            if (end > 0) {
                reservation->recycled_end = (uint32_t)end;
                reservation->recycled_begin = end > ECS_ENTITY_RESERVATION_SIZE
                                                  ? (uint32_t)end - ECS_ENTITY_RESERVATION_SIZE
                                                  : 0;
            }
        }
        if (reuse && reservation->recycled_begin < reservation->recycled_end) {
            id = ((const uint32_t *)entity_index.available.data)[--reservation->recycled_end];
        } else {
            if (reservation->next == reservation->end) {
                uint32_t next = atomic_fetch_add_explicit(
                    &entity_index.count,
                    ECS_ENTITY_RESERVATION_SIZE,
                    memory_order_relaxed
                );
                ecs_fatal_if(
                    next > UINT32_MAX - ECS_ENTITY_RESERVATION_SIZE,
                    "entity index exhausted\n"
                );
                reservation->next = next;
                reservation->end = next + ECS_ENTITY_RESERVATION_SIZE;
            }
            id = reservation->next++;
        }
    } else if (reuse && entity_index.available.size) {
        id = ((const uint32_t *)entity_index.available.data)[--entity_index.available.size];
    } else {
        id = ecs_entity_index_count();
        ecs_fatal_if(id == UINT32_MAX, "entity index exhausted\n");
        atomic_store_explicit(&entity_index.count, id + 1, memory_order_relaxed);
    }
    ecs_entity_record_t *record = ecs_entity_index_ensure(id);
    // Generation and row are immutable during parallel reservations. Publishing
    // only table_id makes liveness lookup independent of the allocation cursor.
    atomic_store_explicit(&record->table_id, 0, memory_order_release);
    return ecs_entity(id, record->generation);
}
