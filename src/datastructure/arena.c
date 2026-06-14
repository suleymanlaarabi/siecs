#include "arena.h"
#include <stdlib.h>


void ecs_arena_init(ecs_arena_t *allocator) {
    allocator->buf = malloc(8);
    allocator->capacity = 8;
    allocator->cursor = 0;
}
void ecs_arena_fini(ecs_arena_t *allocator) {
    free(allocator->buf);
}
