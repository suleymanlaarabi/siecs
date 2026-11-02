/* Derived API declarations from Flecs. Copyright (c) 2025 Sander Mertens.
 * Portions Copyright (c) Meta Platforms, Inc. and affiliates.
 * MIT license: addons/script/LICENSE.flecs. Native SiECS signatures. */
#ifndef SIECS_SCRIPT_H
#define SIECS_SCRIPT_H

#include <siecs_script/bake_config.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ecs_script_t {
    const char *name;
    const char *code;
} ecs_script_t;

typedef struct ecs_script_runtime_t ecs_script_runtime_t;

typedef struct ecs_script_eval_desc_t {
    ecs_script_runtime_t *runtime;
    bool skip_unknown;
    bool ir;
} ecs_script_eval_desc_t;

typedef struct ecs_script_eval_result_t {
    char *error; /* Owned by caller; release with free(). */
    int32_t line;
    int32_t column;
} ecs_script_eval_result_t;

/* Parsing does not mutate or require a world. Evaluation uses the active
 * native SiECS world, initialized by ecs_init(). */
SIECS_SCRIPT_API ecs_script_t *ecs_script_parse(const char *name,
    const char *code, const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result);
SIECS_SCRIPT_API void ecs_script_free(ecs_script_t *script);
/* A runtime reuses local and value scratch storage across evaluations.
 * Use a separate runtime for each concurrent execution thread. */
SIECS_SCRIPT_API ecs_script_runtime_t *ecs_script_runtime_new(void);
SIECS_SCRIPT_API void ecs_script_runtime_free(ecs_script_runtime_t *runtime);
SIECS_SCRIPT_API int ecs_script_eval(const ecs_script_t *script,
    const ecs_script_eval_desc_t *desc, ecs_script_eval_result_t *result);
SIECS_SCRIPT_API int ecs_script_run(const char *name, const char *code,
    ecs_script_eval_result_t *result);
SIECS_SCRIPT_API int ecs_script_run_w_desc(const char *name, const char *code,
    const ecs_script_eval_desc_t *desc, ecs_script_eval_result_t *result);

#ifdef __cplusplus
}
#endif
#endif
