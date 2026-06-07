#ifndef SIECS_TYPE_H
#define SIECS_TYPE_H
#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t *ids;
    uint16_t count;
} ecs_type_t;

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id);
ecs_type_t ecs_type_with_remove(const ecs_type_t *type, uint16_t id);

uint64_t ecs_type_bloom(const ecs_type_t *type);

// returns the index of the id in the type, or -1 if not found
int ecs_type_find(const ecs_type_t *type, uint16_t id);

void ecs_type_fini(ecs_type_t *type);

static inline int
ecs_type_equals(const uint16_t *a_ids, uint16_t a_count, const uint16_t *b_ids, uint16_t b_count) {
    if (a_count != b_count)
        return 0;
    if (a_count == 0)
        return 1;
    return memcmp(a_ids, b_ids, (size_t)a_count * sizeof(uint16_t)) == 0;
}

#endif
