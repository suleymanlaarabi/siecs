#ifndef SIECS_TYPE_H
#define SIECS_TYPE_H
#include "siecs.h"
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t target_index;
    uint16_t target_generation;
    uint16_t relation_id;
} ecs_relation_t;

typedef struct {
    uint16_t *ids;
    uint16_t component_count;
    uint16_t relation_count;
    uint32_t hash;
    ecs_entity_t base;
} ecs_type_t;

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id);
ecs_type_t ecs_type_with_remove_at(const ecs_type_t *type, uint16_t index);
ecs_type_t ecs_type_with_ids(const ecs_type_t *type, const uint16_t *ids, uint16_t count);
ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base);
ecs_type_t ecs_type_with_relation(
    const ecs_type_t *type,
    uint16_t relation_id,
    ecs_entity_t target
);
ecs_type_t ecs_type_without_relation(const ecs_type_t *type, uint16_t relation_id);
ecs_type_t ecs_type_with_component_relation(
    const ecs_type_t *type,
    uint16_t component,
    uint16_t relation_id,
    ecs_entity_t target
);
ecs_type_t ecs_type_without_component_relation(
    const ecs_type_t *type,
    uint16_t component_index,
    uint16_t relation_id
);

static inline ecs_entity_t ecs_relation_key_target(const ecs_relation_t *key) {
    return ((ecs_entity_t)key->target_index << 32) | key->target_generation;
}

static inline void ecs_relation_key_set_target(ecs_relation_t *key, ecs_entity_t target) {
    key->target_index = (uint32_t)(target >> 32);
    key->target_generation = (uint16_t)target;
}

static inline ecs_relation_t *ecs_type_relations(const ecs_type_t *type) {
    uintptr_t end = (uintptr_t)type->ids +
                    (uintptr_t)type->component_count * sizeof(uint16_t);
    return (ecs_relation_t *)((end + _Alignof(ecs_relation_t) - 1) &
                              ~(uintptr_t)(_Alignof(ecs_relation_t) - 1));
}

static inline uint16_t ecs_type_relation_index(const ecs_type_t *type, uint16_t relation_id) {
    const ecs_relation_t *relations = ecs_type_relations(type);
    for (uint16_t i = 0; i < type->relation_count; i++) {
        if (relations[i].relation_id >= relation_id) {
            return relations[i].relation_id == relation_id ? i : UINT16_MAX;
        }
    }
    return UINT16_MAX;
}

uint64_t ecs_type_bloom(const ecs_type_t *type);


void ecs_type_fini(ecs_type_t *type);

static inline int ecs_type_equals(const ecs_type_t *a, const ecs_type_t *b) {
    if (a->base != b->base)
        return 0;
    if (a->component_count != b->component_count || a->relation_count != b->relation_count)
        return 0;
    if (a->component_count &&
        memcmp(a->ids, b->ids, (size_t)a->component_count * sizeof(uint16_t)) != 0)
        return 0;
    if (a->relation_count &&
        memcmp(
            ecs_type_relations(a),
            ecs_type_relations(b),
            (size_t)a->relation_count * sizeof(ecs_relation_t)
        ) != 0)
        return 0;
    return 1;
}

#endif
