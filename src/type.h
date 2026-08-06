#ifndef SIECS_TYPE_H
#define SIECS_TYPE_H
#include "siecs.h"
#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t key;
    uint64_t value;
} ecs_type_pair_t;

typedef struct {
    uint16_t *ids;
    uint16_t component_count;
    uint16_t pair_count;
    uint32_t hash;
    ecs_entity_t base;
} ecs_type_t;

ecs_type_t ecs_type_with(
    const ecs_type_t *type,
    ecs_component_t component,
    ecs_type_pair_t pair
);
ecs_type_t ecs_type_without(
    const ecs_type_t *type,
    uint16_t component_index,
    uint16_t pair_key
);
ecs_type_t ecs_type_with_ids(const ecs_type_t *type, const uint16_t *ids, uint16_t count);
ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base);

static inline ecs_type_pair_t *ecs_type_pairs(const ecs_type_t *type) {
    uintptr_t end = (uintptr_t)type->ids +
                    (uintptr_t)type->component_count * sizeof(uint16_t);
    return (ecs_type_pair_t *)((end + _Alignof(ecs_type_pair_t) - 1) &
                               ~(uintptr_t)(_Alignof(ecs_type_pair_t) - 1));
}

static inline uint16_t ecs_type_pair_index(const ecs_type_t *type, uint16_t key) {
    const ecs_type_pair_t *pairs = ecs_type_pairs(type);
    for (uint16_t i = 0; i < type->pair_count; i++) {
        if (pairs[i].key >= key) {
            return pairs[i].key == key ? i : UINT16_MAX;
        }
    }
    return UINT16_MAX;
}

static inline uint64_t ecs_type_pair_get(const ecs_type_t *type, uint16_t key) {
    uint16_t index = ecs_type_pair_index(type, key);
    return index == UINT16_MAX ? 0 : ecs_type_pairs(type)[index].value;
}

uint64_t ecs_type_bloom(const ecs_type_t *type);
void ecs_type_fini(ecs_type_t *type);

static inline int ecs_type_equals(const ecs_type_t *a, const ecs_type_t *b) {
    if (a->base != b->base || a->component_count != b->component_count ||
        a->pair_count != b->pair_count) {
        return 0;
    }
    if (a->component_count &&
        memcmp(a->ids, b->ids, (size_t)a->component_count * sizeof(uint16_t)) != 0) {
        return 0;
    }
    const ecs_type_pair_t *ap = ecs_type_pairs(a);
    const ecs_type_pair_t *bp = ecs_type_pairs(b);
    for (uint16_t i = 0; i < a->pair_count; i++) {
        if (ap[i].key != bp[i].key || ap[i].value != bp[i].value) {
            return 0;
        }
    }
    return 1;
}

#endif
