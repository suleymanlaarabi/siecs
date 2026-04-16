#pragma once
#define _POSIX_C_SOURCE 199309L
#include <stdint.h>
#include <time.h>

typedef struct {
    double min_ns;
    double max_ns;
    double avg_ns;
    double total_ms;
    uint64_t iterations;
} bench_result_t;

typedef void (*bench_setup_fn_t)(void *ctx);
typedef void (*bench_fn_t)(void *ctx);
typedef void (*bench_teardown_fn_t)(void *ctx);

typedef struct {
    const char         *name;
    bench_fn_t          fn;
    bench_setup_fn_t    setup;
    bench_teardown_fn_t teardown;
    void               *ctx;
    uint64_t            iterations;
} bench_desc_t;

bench_result_t bench_run(const bench_desc_t *desc);
void           bench_print(const char *name, bench_result_t result);

/* Run a benchmark inline — setup/teardown optional (pass NULL).
   Params use _b_ prefix to avoid collisions with bench_desc_t field names. */
#define BENCH(_bname, _biters, _bctx, _bfn, _bsetup, _bteardown) \
    bench_print(_bname, bench_run(&(bench_desc_t){          \
        .name       = (_bname),                             \
        .fn         = (bench_fn_t)(_bfn),                   \
        .setup      = (bench_setup_fn_t)(_bsetup),          \
        .teardown   = (bench_teardown_fn_t)(_bteardown),    \
        .ctx        = (_bctx),                              \
        .iterations = (_biters),                            \
    }))

static inline double bench_ns(struct timespec a, struct timespec b) {
    return (double)(b.tv_sec - a.tv_sec) * 1e9 + (double)(b.tv_nsec - a.tv_nsec);
}
