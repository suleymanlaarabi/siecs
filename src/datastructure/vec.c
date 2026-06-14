#include "vec.h"
#include <stdlib.h>
#include <string.h>

void ecs_vec_init(ecs_vec_t *vec, uint32_t element_size) {
    vec->data = malloc(element_size); // Start with 1 elements
    vec->size = 0;
    vec->capacity = 1;
}

void ecs_vec_init_w_size(ecs_vec_t *vec, uint32_t element_size, uint32_t size) {
    vec->data = malloc(element_size * size);
    vec->size = 0;
    vec->capacity = size;
}

void ecs_vec_fini(ecs_vec_t *vec) { free(vec->data); }

void ecs_vec_grow(ecs_vec_t *vec, uint32_t element_size) {
    vec->capacity *= 2;
    vec->data = realloc(vec->data, element_size * vec->capacity);
}

void ecs_vec_resize_max(ecs_vec_t *vec, uint32_t new_capacity, uint32_t element_size) {
    if (new_capacity > vec->capacity) {
        vec->data = realloc(vec->data, element_size * new_capacity);
        memset(
            (uint8_t *)vec->data + (element_size * vec->capacity),
            0xFF,
            element_size * (new_capacity - vec->capacity)
        );
        vec->capacity = new_capacity;
    }
    if (new_capacity < vec->size) {
        vec->size = new_capacity;
    }
}

void ecs_vec_push(ecs_vec_t *vec, const void *element, const uint32_t element_size) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, element_size);
    }
    memcpy((uint8_t *)vec->data + (vec->size * element_size), element, element_size);
    vec->size++;
}

void ecs_vec_ensure(ecs_vec_t *vec, uint32_t count, const uint32_t element_size) {
    if (count <= vec->size)
        return;
    while (vec->capacity < count)
        ecs_vec_grow(vec, element_size);
    memset((uint8_t *)vec->data + vec->size * element_size, 0, (count - vec->size) * element_size);
    vec->size = count;
}

void ecs_vec_remove_fast(ecs_vec_t *vec, uint32_t index, const uint32_t element_size) {
    if (index < vec->size - 1) {
        void *dst = (uint8_t *)vec->data + (index * element_size);
        const void *src = (uint8_t *)vec->data + ((vec->size - 1) * element_size);
        memcpy(dst, src, element_size);
    }
    vec->size--;
}

bool ecs_vec_contains_u64(const ecs_vec_t *vec, const uint64_t value) {
    ecs_vec_iter(vec, uint64_t, current, {
        if (*current == value) {
            return true;
        }
    });
    return false;
}

static inline void ecs_vec_remove_fast_u64(ecs_vec_t *vec, uint32_t index) {
    if (index < vec->size - 1) {
        uint64_t *data = vec->data;
        data[index] = data[vec->size - 1];
    }
    vec->size--;
}

void ecs_vec_remove_u64(ecs_vec_t *vec, uint64_t value) {
    ecs_vec_iter(vec, uint64_t, current, {
        if (*current == value) {
            ecs_vec_remove_fast_u64(vec, i);
            return;
        }
    });
}
