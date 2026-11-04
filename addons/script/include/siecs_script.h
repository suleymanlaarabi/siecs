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
typedef struct ecs_script_document_t ecs_script_document_t;

typedef struct ecs_script_eval_desc_t {
    ecs_script_runtime_t *runtime;
    bool skip_unknown;
    /* Precompiled statement dispatch; expressions use the native value evaluator. */
    bool ir;
} ecs_script_eval_desc_t;

typedef struct ecs_script_eval_result_t {
    char *error; /* Owned by caller; release with free(). */
    int32_t line;
    int32_t column;
} ecs_script_eval_result_t;

/* Parsing does not mutate or require a world. Evaluation uses the active
 * native SiECS world, initialized by ecs_init(). */
SIECS_SCRIPT_API ecs_script_t *ecs_script_parse(
    const char *name,
    const char *code,
    const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result
);
SIECS_SCRIPT_API void ecs_script_free(ecs_script_t *script);
/* A runtime reuses local and value scratch storage across evaluations.
 * Use a separate runtime for each concurrent execution thread. */
SIECS_SCRIPT_API ecs_script_runtime_t *ecs_script_runtime_new(void);
SIECS_SCRIPT_API void ecs_script_runtime_free(ecs_script_runtime_t *runtime);
SIECS_SCRIPT_API int ecs_script_eval(
    const ecs_script_t *script,
    const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result
);
SIECS_SCRIPT_API int
ecs_script_run(const char *name, const char *code, ecs_script_eval_result_t *result);
SIECS_SCRIPT_API int ecs_script_run_w_desc(
    const char *name,
    const char *code,
    const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result
);
SIECS_SCRIPT_API int ecs_script_run_file(const char *filename, ecs_script_eval_result_t *result);
SIECS_SCRIPT_API int ecs_script_run_file_w_desc(
    const char *filename,
    const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result
);

/* Source documents own parsed revisions. Edits preserve untouched bytes and
 * commit only after successful parsing; they never evaluate or mutate a world.
 * Offsets and lengths count bytes. Undo/redo also operate on source only. */
SIECS_SCRIPT_API ecs_script_document_t *ecs_script_document_new(
    const char *name, const char *code, ecs_script_eval_result_t *result);
SIECS_SCRIPT_API void ecs_script_document_free(ecs_script_document_t *document);
/* Borrowed until the next edit, undo/redo or destruction. */
SIECS_SCRIPT_API const ecs_script_t *ecs_script_document_script(
    const ecs_script_document_t *document);
SIECS_SCRIPT_API int ecs_script_document_edit(ecs_script_document_t *document,
    size_t offset, size_t length, const char *replacement,
    ecs_script_eval_result_t *result);
SIECS_SCRIPT_API bool ecs_script_document_undo(ecs_script_document_t *document);
SIECS_SCRIPT_API bool ecs_script_document_redo(ecs_script_document_t *document);
/* Save through an exclusive sibling temporary file, then replace the target.
 * On error the existing target is preserved. This does not clear history. */
SIECS_SCRIPT_API int ecs_script_document_save(const ecs_script_document_t *document,
    const char *filename, ecs_script_eval_result_t *result);

#ifdef __cplusplus
}
#endif
#endif
