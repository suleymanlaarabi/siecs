#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define ECS_LIKELY(x) __builtin_expect(!!(x), 1)
#define ECS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define ECS_LIKELY(x) (x)
#define ECS_UNLIKELY(x) (x)
#endif

typedef struct {
    void *data;
    uint32_t size;
    uint32_t capacity;
} ecs_vec_t;

void ecs_vec_init(ecs_vec_t *vec, uint32_t element_size);
void ecs_vec_fini(ecs_vec_t *vec);
void ecs_vec_grow(ecs_vec_t *vec, uint32_t element_size);
void ecs_vec_resize_max(ecs_vec_t *vec, uint32_t new_capacity, uint32_t element_size);

// Ensure vec has at least `count` elements. New slots are zero-initialized.
static inline void ecs_vec_ensure(ecs_vec_t *vec, uint32_t count, uint32_t element_size) {
    if (count <= vec->size)
        return;
    while (vec->capacity < count)
        ecs_vec_grow(vec, element_size);
    memset((uint8_t *)vec->data + vec->size * element_size, 0, (count - vec->size) * element_size);
    vec->size = count;
}

// Overwrite element at index (index must be < size).
static inline void
ecs_vec_set(ecs_vec_t *vec, uint32_t element_size, uint32_t index, const void *element) {
    memcpy((uint8_t *)vec->data + (index * element_size), element, element_size);
}

// Copy element into the vec (memcpy). The pointer is not retained.
// Safe to call repeatedly — any grow only invalidates the internal buffer, not
// the source pointer.
static inline void ecs_vec_push(ecs_vec_t *vec, const void *element, uint32_t element_size) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, element_size);
    }
    memcpy((uint8_t *)vec->data + (vec->size * element_size), element, element_size);
    vec->size++;
}

// Reserve one slot and return a pointer to it (uninitialized).
// WARNING: the returned pointer is invalidated by any subsequent push or grow
// on the same vec. Finish all writes through this pointer before pushing again.
static inline void *ecs_vec_push_empty(ecs_vec_t *vec, uint32_t element_size) {
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

// Specialized push for 8-byte types
static inline void ecs_vec_push_u64(ecs_vec_t *vec, uint64_t value) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, sizeof(uint64_t));
    }
    ((uint64_t *)vec->data)[vec->size++] = value;
}

// Direct pointer access for fast iteration
#define ecs_vec_get(vec, index, type) (&((const type *)(vec)->data)[index])
#define ecs_vec_get_mut(vec, index, type) (&((type *)(vec)->data)[index])
#define ecs_vec_get_last(vec, type) (&((type *)(vec)->data)[(vec)->size - 1])
#define ecs_vec_remove_last(vec) ((vec)->size--)
#define ecs_vec_clear(vec) ((vec)->size = 0)
#define ecs_vec_data(vec, type) ((type *)(vec)->data)
