#ifndef SIECS_HELPER_H
#define SIECS_HELPER_H

#include <stdbool.h>
#include <stdint.h>

#if defined(_MSC_VER)
#include <intrin.h>

#define ECS_LIKELY(x) (!!(x))
#define ECS_UNLIKELY(x) (!!(x))
#define ECS_COLD __declspec(noinline)
#define ECS_ALWAYS_INLINE __forceinline

static inline unsigned ecs_ctz(unsigned value) {
    unsigned long index;
    _BitScanForward(&index, value);
    return (unsigned)index;
}

#define ECS_CTZ(x) ecs_ctz((unsigned)(x))
#else
#define ECS_LIKELY(x) __builtin_expect(!!(x), 1)
#define ECS_UNLIKELY(x) __builtin_expect(!!(x), 0)
/* Rare paths kept out of line, so hot inline callers stay small enough to inline. */
#define ECS_COLD __attribute__((cold, noinline))
#define ECS_ALWAYS_INLINE inline __attribute__((always_inline))
#define ECS_CTZ(x) __builtin_ctz((unsigned)(x))
#endif

#define ecs_entity(index, generation) (((uint64_t)(index) << 32) | (generation & 0xffffffff))

#define ecs_first(id) ((uint32_t)((id) >> 32))
#define ecs_second(id) ((uint32_t)((id) & 0xffffffff))

#endif
