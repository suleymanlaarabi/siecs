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

static void ecs_array_edit(
    void *dst,
    const void *src,
    size_t element_size,
    uint16_t count,
    uint16_t at,
    int delta,
    const void *value
) {
    if (!value && !delta) {
        if (count) memcpy(dst, src, (size_t)count * element_size);
        return;
    }
    uint8_t *out = dst;
    const uint8_t *in = src;
    if (at) memcpy(out, in, (size_t)at * element_size);
    if (delta >= 0) memcpy(out + (size_t)at * element_size, value, element_size);
    uint16_t from = (uint16_t)(at + (delta <= 0));
    uint16_t to = (uint16_t)(at + (delta >= 0));
    if (from < count)
        memcpy(out + (size_t)to * element_size, in + (size_t)from * element_size,
               (size_t)(count - from) * element_size);
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
    ecs_array_edit(out.ids, type->ids, sizeof *type->ids, type->component_count,
                   component_at, component != 0, component ? &component : NULL);
    ecs_array_edit(ecs_type_pairs(&out), ecs_type_pairs(type), sizeof pair, type->pair_count,
                   pair_at, pair_delta, pair.key ? &pair : NULL);
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
    ecs_array_edit(out.ids, type->ids, sizeof *type->ids, type->component_count,
                   component_at, component_delta, NULL);
    ecs_array_edit(ecs_type_pairs(&out), ecs_type_pairs(type), sizeof(ecs_type_pair_t),
                   type->pair_count, pair_at, pair_delta, NULL);
    return out;
}

ecs_type_t ecs_type_with_ids(const ecs_type_t *type, const uint16_t *ids, uint16_t count) {
    ecs_type_t out = ecs_type_alloc(type, count - type->component_count, 0);
    if (count) {
        memcpy(out.ids, ids, (size_t)count * sizeof(uint16_t));
    }
    ecs_array_edit(ecs_type_pairs(&out), ecs_type_pairs(type), sizeof(ecs_type_pair_t),
                   type->pair_count, 0, 0, NULL);
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

    ecs_array_edit(ecs_type_pairs(&out), ecs_type_pairs(type), sizeof(ecs_type_pair_t),
                   type->pair_count, 0, 0, NULL);
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
