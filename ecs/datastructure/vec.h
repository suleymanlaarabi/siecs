#pragma once
#include <stdint.h>
#include "../compiler.h"

typedef struct {
    void *data;
    uint32_t size;
    uint32_t capacity;
} ecs_vec_t;

void ecs_vec_init(ecs_vec_t *vec, const uint32_t element_size);
void ecs_vec_fini(ecs_vec_t *vec);
void ecs_vec_grow(ecs_vec_t *vec, const uint32_t element_size);
void ecs_vec_resize_max(ecs_vec_t *vec, uint32_t new_capacity, const uint32_t element_size);

// Ensure vec has at least `count` elements. New slots are zero-initialized.
void ecs_vec_ensure(ecs_vec_t *vec, uint32_t count, const uint32_t element_size);

// Copy element into the vec (memcpy). The pointer is not retained.
// Safe to call repeatedly — any grow only invalidates the internal buffer, not
// the source pointer.
void ecs_vec_push(ecs_vec_t *vec, const void *element, const uint32_t element_size);

// Reserve one slot and return a pointer to it (uninitialized).
// WARNING: the returned pointer is invalidated by any subsequent push or grow
// on the same vec. Finish all writes through this pointer before pushing again.
static inline void *ecs_vec_push_empty(ecs_vec_t *vec, const uint32_t element_size) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, element_size);
    }
    void *ptr = (uint8_t *)vec->data + (vec->size * element_size);
    vec->size++;
    return ptr;
}

// Specialized push for 2-byte types
static inline void ecs_vec_push_u16(ecs_vec_t *vec, uint16_t value) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, sizeof(uint16_t));
    }
    ((uint16_t *)vec->data)[vec->size++] = value;
}

// Specialized push for 4-byte types
static inline void ecs_vec_push_u32(ecs_vec_t *vec, uint32_t value) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, sizeof(uint32_t));
    }
    ((uint32_t *)vec->data)[vec->size++] = value;
}

// Direct pointer access for fast iteration
#define ecs_vec_get(vec, index, type) (&((const type *)(vec)->data)[index])
#define ecs_vec_get_mut(vec, index, type) (&((type *)(vec)->data)[index])
#define ecs_vec_get_last(vec, type) (&((type *)(vec)->data)[(vec)->size - 1])
#define ecs_vec_remove_last(vec) ((vec)->size--)
#define ecs_vec_clear(vec) ((vec)->size = 0)
#define ecs_vec_data(vec, type) ((type *)(vec)->data)

// Direct indexed access for fast writes. Does not check bounds or grow the vec.
#define ecs_vec_set(vec, type, index, value) (((type *)vec->data)[index] = value)
