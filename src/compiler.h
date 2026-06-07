#ifndef SIECS_COMPILER_H
#define SIECS_COMPILER_H
#define ECS_LIKELY(x) __builtin_expect(!!(x), 1)
#define ECS_UNLIKELY(x) __builtin_expect(!!(x), 0)

#endif
