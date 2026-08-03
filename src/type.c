#include "type.h"
#include <stdlib.h>
#include <string.h>

static size_t ecs_type_relations_offset(uint16_t count) {
    size_t end = (size_t)count * sizeof(uint16_t);
    return (end + _Alignof(ecs_relation_t) - 1) & ~(size_t)(_Alignof(ecs_relation_t) - 1);
}

static ecs_type_t ecs_type_alloc(const ecs_type_t *type, int components, int relations) {
    uint16_t component_count = (uint16_t)(type->component_count + components);
    uint16_t relation_count = (uint16_t)(type->relation_count + relations);
    size_t bytes = ecs_type_relations_offset(component_count) +
                   (size_t)relation_count * sizeof(ecs_relation_t);
    return (ecs_type_t){
        .ids = bytes ? malloc(bytes) : NULL,
        .component_count = component_count,
        .relation_count = relation_count,
        .base = type->base,
    };
}

static void ecs_type_copy_ids(
    ecs_type_t *dst,
    const ecs_type_t *src,
    uint16_t at,
    int delta,
    uint16_t id
) {
    if (at) {
        memcpy(dst->ids, src->ids, (size_t)at * sizeof(uint16_t));
    }
    if (delta > 0) {
        dst->ids[at] = id;
    }
    uint16_t from = (uint16_t)(at + (delta < 0));
    uint16_t to = (uint16_t)(at + (delta > 0));
    if (from < src->component_count) {
        memcpy(
            dst->ids + to,
            src->ids + from,
            (size_t)(src->component_count - from) * sizeof(uint16_t)
        );
    }
}

static void ecs_type_copy_relations(
    ecs_type_t *dst,
    const ecs_type_t *src,
    uint16_t at,
    int delta,
    uint16_t relation,
    ecs_entity_t target
) {
    const ecs_relation_t *old = ecs_type_relations(src);
    ecs_relation_t *keys = ecs_type_relations(dst);
    if (at) {
        memcpy(keys, old, (size_t)at * sizeof(*keys));
    }
    if (delta > 0) {
        keys[at] = (ecs_relation_t){ .relation_id = relation };
        ecs_relation_key_set_target(&keys[at], target);
    }
    uint16_t from = (uint16_t)(at + (delta < 0));
    uint16_t to = (uint16_t)(at + (delta > 0));
    if (from < src->relation_count) {
        memcpy(
            keys + to,
            old + from,
            (size_t)(src->relation_count - from) * sizeof(*keys)
        );
    }
}

static void ecs_type_copy(
    ecs_type_t *dst,
    const ecs_type_t *src,
    uint16_t component_at,
    int component_delta,
    uint16_t relation_at,
    int relation_delta,
    uint16_t component,
    uint16_t relation,
    ecs_entity_t target
) {
    ecs_type_copy_ids(dst, src, component_at, component_delta, component);
    ecs_type_copy_relations(dst, src, relation_at, relation_delta, relation, target);
}

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id) {
    uint16_t at = 0;
    while (at < type->component_count && type->ids[at] < id) {
        at++;
    }
    ecs_type_t out = ecs_type_alloc(type, 1, 0);
    ecs_type_copy(&out, type, at, 1, 0, 0, id, 0, 0);
    return out;
}

ecs_type_t ecs_type_with_remove_at(const ecs_type_t *type, uint16_t index) {
    ecs_type_t out = ecs_type_alloc(type, -1, 0);
    ecs_type_copy(&out, type, index, -1, 0, 0, 0, 0, 0);
    return out;
}

ecs_type_t ecs_type_with_ids(const ecs_type_t *type, const uint16_t *ids, uint16_t count) {
    ecs_type_t out = ecs_type_alloc(type, count - type->component_count, 0);
    if (count) {
        memcpy(out.ids, ids, (size_t)count * sizeof(uint16_t));
    }
    ecs_type_copy_relations(&out, type, 0, 0, 0, 0);
    return out;
}

ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base) {
    ecs_type_t out = ecs_type_alloc(type, 0, 0);
    out.base = base;
    ecs_type_copy(&out, type, 0, 0, 0, 0, 0, 0, 0);
    return out;
}

ecs_type_t ecs_type_with_relation(
    const ecs_type_t *type,
    uint16_t relation,
    ecs_entity_t target
) {
    uint16_t at = ecs_type_relation_index(type, relation);
    int delta = 0;
    if (at == UINT16_MAX) {
        const ecs_relation_t *keys = ecs_type_relations(type);
        at = 0;
        while (at < type->relation_count && keys[at].relation_id < relation) {
            at++;
        }
        delta = 1;
    }
    ecs_type_t out = ecs_type_alloc(type, 0, delta);
    ecs_type_copy(&out, type, 0, 0, at, delta, 0, relation, target);
    if (!delta) {
        ecs_relation_key_set_target(&ecs_type_relations(&out)[at], target);
    }
    return out;
}

ecs_type_t ecs_type_without_relation(const ecs_type_t *type, uint16_t relation) {
    uint16_t at = ecs_type_relation_index(type, relation);
    if (at == UINT16_MAX) {
        return ecs_type_with_base(type, type->base);
    }
    ecs_type_t out = ecs_type_alloc(type, 0, -1);
    ecs_type_copy(&out, type, 0, 0, at, -1, 0, 0, 0);
    return out;
}

ecs_type_t ecs_type_with_component_relation(
    const ecs_type_t *type,
    uint16_t component,
    uint16_t relation,
    ecs_entity_t target
) {
    uint16_t component_at = 0;
    while (component_at < type->component_count && type->ids[component_at] < component) {
        component_at++;
    }
    const ecs_relation_t *keys = ecs_type_relations(type);
    uint16_t relation_at = 0;
    while (relation_at < type->relation_count && keys[relation_at].relation_id < relation) {
        relation_at++;
    }
    ecs_type_t out = ecs_type_alloc(type, 1, 1);
    ecs_type_copy(&out, type, component_at, 1, relation_at, 1, component, relation, target);
    return out;
}

ecs_type_t ecs_type_without_component_relation(
    const ecs_type_t *type,
    uint16_t component_at,
    uint16_t relation
) {
    ecs_type_t out = ecs_type_alloc(type, -1, -1);
    ecs_type_copy(
        &out, type, component_at, -1, ecs_type_relation_index(type, relation), -1, 0, 0, 0
    );
    return out;
}

void ecs_type_fini(ecs_type_t *type) {
    free(type->ids);
    type->ids = NULL;
}

uint64_t ecs_type_bloom(const ecs_type_t *type) {
    uint64_t bloom = 0;
    for (uint16_t i = 0; i < type->component_count; i++) {
        bloom |= UINT64_C(1) << (type->ids[i] % 64);
    }
    return bloom;
}
