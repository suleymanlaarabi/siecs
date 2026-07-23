#ifndef ECS_ARENA_H
#define ECS_ARENA_H

#include "../compiler.h"
#include <stddef.h>
#include <stdint.h>

typedef struct ecs_arena_block_s {
    struct ecs_arena_block_s *next;
    uint32_t capacity;
    uint32_t cursor;
    max_align_t data[];
} ecs_arena_block_t;

typedef struct {
    ecs_arena_block_t *first;
    ecs_arena_block_t *current;
    ecs_arena_block_t *last;
} ecs_arena_t;

void ecs_arena_init(ecs_arena_t *allocator);
void ecs_arena_fini(ecs_arena_t *allocator);
void *ecs_arena_alloc_slow(ecs_arena_t *allocator, uint32_t size);

static inline void *ecs_arena_alloc(ecs_arena_t *allocator, uint32_t size) {
    ecs_arena_block_t *block = allocator->current;
    const uint32_t alignment = (uint32_t)_Alignof(max_align_t);
    const uint32_t cursor = (block->cursor + alignment - 1u) & ~(alignment - 1u);
    if (ECS_LIKELY(cursor <= block->capacity && size <= block->capacity - cursor)) {
        block->cursor = cursor + size;
        return (uint8_t *)block->data + cursor;
    }
    return ecs_arena_alloc_slow(allocator, size);
}

static inline void ecs_arena_reset(ecs_arena_t *allocator) {
    for (ecs_arena_block_t *block = allocator->first; block; block = block->next) {
        block->cursor = 0;
    }
    allocator->current = allocator->first;
}

#endif
