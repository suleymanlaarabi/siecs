#define _POSIX_C_SOURCE 199309L
#include "bench.h"
#include <stdio.h>
#include <float.h>

bench_result_t bench_run(const bench_desc_t *desc) {
    bench_result_t res = {
        .min_ns    = DBL_MAX,
        .max_ns    = 0.0,
        .iterations = desc->iterations,
    };

    double total = 0.0;
    for (uint64_t i = 0; i < desc->iterations; i++) {
        if (desc->setup)
            desc->setup(desc->ctx);

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        desc->fn(desc->ctx);
        clock_gettime(CLOCK_MONOTONIC, &t1);

        double ns = bench_ns(t0, t1);
        if (ns < res.min_ns) res.min_ns = ns;
        if (ns > res.max_ns) res.max_ns = ns;
        total += ns;

        if (desc->teardown)
            desc->teardown(desc->ctx);
    }

    res.avg_ns   = total / (double)desc->iterations;
    res.total_ms = total / 1e6;
    return res;
}

void bench_print(const char *name, bench_result_t r) {
    printf("%-40s  avg: %8.2f ns  min: %8.2f ns  max: %8.2f ns  total: %8.2f ms  iters: %lu\n",
           name, r.avg_ns, r.min_ns, r.max_ns, r.total_ms, (unsigned long)r.iterations);
}
