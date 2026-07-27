#ifndef SIECS_TYPE_H
#define SIECS_TYPE_H
#include "siecs.h"
#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t *ids;
    uint16_t count;
    // Table metadata stored in the alignment gap before base. It is not part of
    // type identity; transient types may leave it zero until table creation.
    uint16_t data_count;
    uint32_t hash;
    ecs_entity_t base;
} ecs_type_t;

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id);
ecs_type_t ecs_type_with_remove_at(const ecs_type_t *type, uint16_t index);
ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base);

uint64_t ecs_type_bloom(const ecs_type_t *type);


void ecs_type_fini(ecs_type_t *type);

static inline int ecs_type_equals(const ecs_type_t *a, const ecs_type_t *b) {
    if (a->base != b->base)
        return 0;
    if (a->count != b->count)
        return 0;
    if (a->count == 0)
        return 1;
    return memcmp(a->ids, b->ids, (size_t)a->count * sizeof(uint16_t)) == 0;
}

#endif
