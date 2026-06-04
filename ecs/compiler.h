#pragma once
#if defined(__GNUC__) || defined(__clang__)
#define ECS_LIKELY(x) __builtin_expect(!!(x), 1)
#define ECS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define ECS_LIKELY(x) (x)
#define ECS_UNLIKELY(x) (x)
#endif
