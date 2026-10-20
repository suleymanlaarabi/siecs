#ifndef SIECS_VALUE_H
#define SIECS_VALUE_H

#include "siecs.h"
#include <stdlib.h>
#include <string.h>

/* Copying an inherited owned tag still invokes its explicit copy hook. Normal
 * storage transfers skip zero-sized values before entering this primitive. */
static inline bool ecs_value_copy(
    const ecs_type_ops_t *ops,
    size_t size,
    void *dst,
    const void *src,
    uint32_t count,
    bool construct
) {
    ecs_type_copy_t copy = construct ? ops->copy_ctor : ops->copy;
    if (copy) {
        copy(dst, src, count);
        return true;
    }
    if (size)
        memcpy(dst, src, size * count);
    return false;
}

/* Construction and assignment have different hooks. A move consumes its source
 * only when a move hook or a copy hook plus destructor performs that work. */
static inline void ecs_value_transfer(
    const ecs_type_ops_t *ops,
    size_t size,
    void *dst,
    const void *src,
    uint32_t count,
    bool move,
    bool construct
) {
    if (!size)
        return;
    ecs_type_move_t move_op = construct ? ops->move_ctor : ops->move;
    if (move && move_op) {
        move_op(dst, (void *)src, count);
        return;
    }
    if (ecs_value_copy(ops, size, dst, src, count, construct) && move && ops->dtor)
        ops->dtor((void *)src, count);
}

sireflect_struct_desc_t *ecs_reflection_desc_copy(const sireflect_struct_desc_t *desc);
void ecs_reflection_desc_free(const sireflect_struct_desc_t *desc);

#endif
