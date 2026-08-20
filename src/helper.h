#ifndef SIECS_HELPER_H
#define SIECS_HELPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>

#define ECS_LIKELY(x) (!!(x))
#define ECS_UNLIKELY(x) (!!(x))

static inline unsigned ecs_ctz(unsigned value) {
    unsigned long index;
    _BitScanForward(&index, value);
    return (unsigned)index;
}

#define ECS_CTZ(x) ecs_ctz((unsigned)(x))
#else
#define ECS_LIKELY(x) __builtin_expect(!!(x), 1)
#define ECS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ECS_CTZ(x) __builtin_ctz((unsigned)(x))
#endif

#define ecs_entity(index, generation) (((uint64_t)(index) << 32) | (generation & 0xffffffff))

#define ecs_first(id) ((uint32_t)((id) >> 32))
#define ecs_second(id) ((uint32_t)((id) & 0xffffffff))

static inline void ecs_access_add(uint32_t *accesses, uint16_t *count,
                                  uint16_t id, bool write) {
    uint16_t at = 0;
    while (at < *count && (uint16_t)(accesses[at] & UINT16_MAX) < id) at++;
    if (at < *count && (uint16_t)(accesses[at] & UINT16_MAX) == id) {
        accesses[at] |= (uint32_t)write << 16;
        return;
    }
    memmove(accesses + at + 1, accesses + at,
            (size_t)(*count - at) * sizeof *accesses);
    accesses[at] = (uint32_t)id | ((uint32_t)write << 16);
    (*count)++;
}

static inline bool ecs_access_conflict(const uint32_t *a, uint16_t a_count,
                                       const uint32_t *b, uint16_t b_count) {
    uint16_t ai = 0, bi = 0;
    while (ai < a_count && bi < b_count) {
        const uint16_t aid = (uint16_t)(a[ai] & UINT16_MAX);
        const uint16_t bid = (uint16_t)(b[bi] & UINT16_MAX);
        if (aid < bid) ai++;
        else if (bid < aid) bi++;
        else {
            if (((a[ai] | b[bi]) >> 16) != 0) return true;
            ai++;
            bi++;
        }
    }
    return false;
}

#endif
