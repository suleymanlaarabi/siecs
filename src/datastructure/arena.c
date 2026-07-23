#include "arena.h"
#include <stdlib.h>

#define ECS_ARENA_INITIAL_CAPACITY 4096u

static ecs_arena_block_t *ecs_arena_block_new(uint32_t capacity) {
    ecs_arena_block_t *block = malloc(sizeof(ecs_arena_block_t) + capacity);
    *block = (ecs_arena_block_t){ .capacity = capacity };
    return block;
}

void ecs_arena_init(ecs_arena_t *allocator) {
    ecs_arena_block_t *block = ecs_arena_block_new(ECS_ARENA_INITIAL_CAPACITY);
    *allocator = (ecs_arena_t){
        .first = block,
        .current = block,
        .last = block,
    };
}

void *ecs_arena_alloc_slow(ecs_arena_t *allocator, uint32_t size) {
    for (ecs_arena_block_t *block = allocator->current->next; block; block = block->next) {
        if (size <= block->capacity) {
            allocator->current = block;
            block->cursor = size;
            return block->data;
        }
    }

    uint32_t capacity = allocator->last->capacity;
    if (capacity <= UINT32_MAX / 2u) {
        capacity *= 2u;
    }
    while (capacity < size) {
        if (capacity > UINT32_MAX / 2u) {
            capacity = size;
            break;
        }
        capacity *= 2u;
    }

    ecs_arena_block_t *block = ecs_arena_block_new(capacity);
    allocator->last->next = block;
    allocator->last = block;
    allocator->current = block;
    block->cursor = size;
    return block->data;
}

void ecs_arena_fini(ecs_arena_t *allocator) {
    ecs_arena_block_t *block = allocator->first;
    while (block) {
        ecs_arena_block_t *next = block->next;
        free(block);
        block = next;
    }
}
