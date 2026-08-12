#ifndef SIECS_STORAGE_VALUE_OPS_H
#define SIECS_STORAGE_VALUE_OPS_H

#include "siecs.h"
#include <stdint.h>
#include <string.h>

static inline void ecs_value_ctor(
    uint64_t size,
    const ecs_type_ops_t *ops,
    void *dst,
    uint32_t count
) {
    if (size == 0 || count == 0) {
        return;
    }
    if (ops->ctor != NULL) {
        ops->ctor(dst, count);
        return;
    }
    memset(dst, 0, (size_t)size * count);
}

static inline void ecs_value_dtor(
    uint64_t size,
    const ecs_type_ops_t *ops,
    void *ptr,
    uint32_t count
) {
    if (size == 0 || count == 0 || ops->dtor == NULL) {
        return;
    }
    ops->dtor(ptr, count);
}

static inline void ecs_value_copy_ctor(
    uint64_t size,
    const ecs_type_ops_t *ops,
    void *dst,
    const void *src,
    uint32_t count
) {
    if (size == 0 || count == 0) {
        return;
    }
    if (ops->copy_ctor != NULL) {
        ops->copy_ctor(dst, src, count);
        return;
    }
    memcpy(dst, src, (size_t)size * count);
}

static inline void ecs_value_copy(
    uint64_t size,
    const ecs_type_ops_t *ops,
    void *dst,
    const void *src,
    uint32_t count
) {
    if (size == 0 || count == 0) {
        return;
    }
    if (ops->copy != NULL) {
        ops->copy(dst, src, count);
        return;
    }
    memcpy(dst, src, (size_t)size * count);
}

static inline void ecs_value_move_ctor(
    uint64_t size,
    const ecs_type_ops_t *ops,
    void *dst,
    void *src,
    uint32_t count
) {
    if (size == 0 || count == 0) {
        return;
    }
    if (ops->move_ctor != NULL) {
        ops->move_ctor(dst, src, count);
        return;
    }
    if (ops->copy_ctor != NULL) {
        ops->copy_ctor(dst, src, count);
        ecs_value_dtor(size, ops, src, count);
        return;
    }
    memcpy(dst, src, (size_t)size * count);
}

static inline void ecs_value_move(
    uint64_t size,
    const ecs_type_ops_t *ops,
    void *dst,
    void *src,
    uint32_t count
) {
    if (size == 0 || count == 0) {
        return;
    }
    if (ops->move != NULL) {
        ops->move(dst, src, count);
        return;
    }
    if (ops->copy != NULL) {
        ops->copy(dst, src, count);
        ecs_value_dtor(size, ops, src, count);
        return;
    }
    memcpy(dst, src, (size_t)size * count);
}

#endif
