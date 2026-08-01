#ifndef BENCH_H
#define BENCH_H

/* This generated file contains includes for project dependencies */
#include "bench/bake_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

static inline double time_ms(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    static int initialized;
    LARGE_INTEGER counter;
    if (!initialized) {
        QueryPerformanceFrequency(&frequency);
        initialized = 1;
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

#define arg(name, value) enum { name = value }

/* Stable output format consumed by tools/bench_compare.py: [bench] name: elapsed_ms ms. */
#define BENCH(...)                                                                                 \
    do {                                                                                           \
        double __bench_start_time = time_ms();                                                     \
        do {                                                                                       \
            __VA_ARGS__                                                                            \
        } while (0);                                                                               \
        double __bench_end_time = time_ms();                                                       \
        printf("[bench] %s: %.3f ms\n", __bench_id, __bench_end_time - __bench_start_time);        \
    } while (0)

#define BENCH_SETUP(name, ...)                                                                     \
    void bench_##name(void) {                                                                      \
        const char *__bench_id = #name;                                                            \
        __VA_ARGS__                                                                                \
    }

#define run_bench(id)                                                                              \
    {                                                                                              \
        ecs_init();                                                                                \
        bench_##id();                                                                              \
        ecs_fini();                                                                                \
    }

#ifdef __cplusplus
}
#endif

#endif
