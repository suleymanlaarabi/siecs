#include "type.h"
#include <stdlib.h>
#include <string.h>

static size_t ecs_type_pairs_offset(uint16_t count) {
    size_t end = (size_t)count * sizeof(uint16_t);
    return (end + _Alignof(ecs_type_pair_t) - 1) &
           ~(size_t)(_Alignof(ecs_type_pair_t) - 1);
}

static ecs_type_t ecs_type_alloc(const ecs_type_t *type, int components, int pairs) {
    uint16_t component_count = (uint16_t)(type->component_count + components);
    uint16_t pair_count = (uint16_t)(type->pair_count + pairs);
    size_t bytes = ecs_type_pairs_offset(component_count) +
                   (size_t)pair_count * sizeof(ecs_type_pair_t);
    return (ecs_type_t){
        .ids = bytes ? malloc(bytes) : NULL,
        .component_count = component_count,
        .pair_count = pair_count,
        .base = type->base,
    };
}

static void ecs_type_copy_ids(
    ecs_type_t *dst,
    const ecs_type_t *src,
    uint16_t at,
    int delta,
    ecs_component_t component
) {
    if (!component && !delta) {
        if (src->component_count) {
            memcpy(dst->ids, src->ids, (size_t)src->component_count * sizeof(uint16_t));
        }
        return;
    }
    if (at) {
        memcpy(dst->ids, src->ids, (size_t)at * sizeof(uint16_t));
    }
    if (delta > 0) {
        dst->ids[at] = component;
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

static void ecs_type_copy_pairs(
    ecs_type_t *dst,
    const ecs_type_t *src,
    uint16_t at,
    int delta,
    ecs_type_pair_t pair
) {
    const ecs_type_pair_t *old = ecs_type_pairs(src);
    ecs_type_pair_t *out = ecs_type_pairs(dst);
    if (!pair.key && !delta) {
        if (src->pair_count) {
            memcpy(out, old, (size_t)src->pair_count * sizeof(ecs_type_pair_t));
        }
        return;
    }
    if (at) {
        memcpy(out, old, (size_t)at * sizeof(ecs_type_pair_t));
    }
    if (delta >= 0 && pair.key) {
        out[at] = pair;
    }
    uint16_t from = (uint16_t)(at + (delta <= 0));
    uint16_t to = (uint16_t)(at + (delta >= 0));
    if (from < src->pair_count) {
        memcpy(out + to, old + from, (size_t)(src->pair_count - from) * sizeof(ecs_type_pair_t));
    }
}

ecs_type_t ecs_type_with(
    const ecs_type_t *type,
    ecs_component_t component,
    ecs_type_pair_t pair
) {
    uint16_t component_at = 0;
    while (component_at < type->component_count && type->ids[component_at] < component) {
        component_at++;
    }

    uint16_t pair_at = 0;
    int pair_delta = 0;
    if (pair.key) {
        const ecs_type_pair_t *pairs = ecs_type_pairs(type);
        while (pair_at < type->pair_count && pairs[pair_at].key < pair.key) {
            pair_at++;
        }
        pair_delta = pair_at == type->pair_count || pairs[pair_at].key != pair.key;
    }

    ecs_type_t out = ecs_type_alloc(type, component != 0, pair_delta);
    ecs_type_copy_ids(&out, type, component_at, component != 0, component);
    ecs_type_copy_pairs(&out, type, pair_at, pair_delta, pair);
    return out;
}

ecs_type_t ecs_type_without(
    const ecs_type_t *type,
    uint16_t component_at,
    uint16_t pair_key
) {
    int component_delta = component_at != UINT16_MAX ? -1 : 0;
    int pair_delta = pair_key ? -1 : 0;
    uint16_t pair_at = pair_key ? ecs_type_pair_index(type, pair_key) : 0;
    ecs_type_t out = ecs_type_alloc(type, component_delta, pair_delta);
    ecs_type_copy_ids(&out, type, component_at, component_delta, 0);
    ecs_type_copy_pairs(
        &out,
        type,
        pair_at,
        pair_delta,
        (ecs_type_pair_t){ .key = pair_key }
    );
    return out;
}

ecs_type_t ecs_type_with_ids(const ecs_type_t *type, const uint16_t *ids, uint16_t count) {
    ecs_type_t out = ecs_type_alloc(type, count - type->component_count, 0);
    if (count) {
        memcpy(out.ids, ids, (size_t)count * sizeof(uint16_t));
    }
    ecs_type_copy_pairs(&out, type, 0, 0, (ecs_type_pair_t){ 0 });
    return out;
}

ecs_type_t ecs_type_with_added_ids(
    const ecs_type_t *type,
    const ecs_component_t *ids,
    uint16_t count
) {
    ecs_type_t out = ecs_type_alloc(type, count, 0);
    uint16_t from_i = 0;
    uint16_t added_i = 0;
    uint16_t out_i = 0;

    while (from_i < type->component_count && added_i < count) {
        if (type->ids[from_i] < ids[added_i]) {
            out.ids[out_i++] = type->ids[from_i++];
        } else {
            out.ids[out_i++] = ids[added_i++];
        }
    }
    while (from_i < type->component_count) {
        out.ids[out_i++] = type->ids[from_i++];
    }
    while (added_i < count) {
        out.ids[out_i++] = ids[added_i++];
    }

    ecs_type_copy_pairs(&out, type, 0, 0, (ecs_type_pair_t){ 0 });
    return out;
}

ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base) {
    ecs_type_t out = ecs_type_with_ids(type, type->ids, type->component_count);
    out.base = base;
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
