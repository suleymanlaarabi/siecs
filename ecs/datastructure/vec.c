#include "vec.h"
#include <stdlib.h>

void ecs_vec_init(ecs_vec_t *vec, uint32_t element_size) {
    vec->data = malloc(element_size * 8); // Start with 8 elements
    vec->size = 0;
    vec->capacity = 8;
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
