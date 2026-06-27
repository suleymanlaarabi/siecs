#ifndef ECS_ARENA_H
#define ECS_ARENA_H

#include "../compiler.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint32_t capacity;
    uint32_t cursor;
    uint8_t *buf;
} ecs_arena_t;

void ecs_arena_init(ecs_arena_t *allocator);
void ecs_arena_fini(ecs_arena_t *allocator);

static inline void *ecs_arena_alloc(ecs_arena_t *allocator, uint32_t size) {
    if (ECS_LIKELY(allocator->cursor + size <= allocator->capacity)) {
        allocator->cursor += size;
        return allocator->buf + (allocator->cursor - size);
    }
    allocator->buf = (uint8_t *)realloc(allocator->buf, allocator->capacity + size);
    allocator->capacity = allocator->capacity + size;
    allocator->cursor += size;
    return allocator->buf + (allocator->cursor - size);
}

static inline void ecs_arena_reset(ecs_arena_t *allocator) { allocator->cursor = 0; }

#endif
