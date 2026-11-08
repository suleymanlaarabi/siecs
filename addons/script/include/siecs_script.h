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
    ecs_entity_t parent;
    uint32_t instruction_budget;
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
SIECS_SCRIPT_API ecs_script_t *ecs_script_parse_file(
    const char *filename,
    const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result
);
SIECS_SCRIPT_API void ecs_script_free(ecs_script_t *script);
SIECS_SCRIPT_API const ecs_script_t *ecs_script_retain(const ecs_script_t *script);

/* Borrowed text annotation attached to an entity, or NULL when absent.
 * Annotations are owning native CString components under
 * siecs::script::annotation::<name>, and participate in instance ownership. */
SIECS_SCRIPT_API const char *ecs_script_annotation(ecs_entity_t entity, const char *name);

typedef struct ecs_script_instance_t ecs_script_instance_t;
typedef enum ecs_script_instance_state_t {
    EcsScriptActive,
    EcsScriptPending,
    EcsScriptFailed
} ecs_script_instance_state_t;
typedef struct ecs_script_instance_desc_t {
    ecs_entity_t parent;
    uint32_t instruction_budget;
    bool watch;
    double watch_interval;
} ecs_script_instance_desc_t;
typedef struct ecs_script_stats_t {
    uint64_t evaluations;
    uint64_t commits;
    uint64_t invalidations;
    uint64_t suspensions;
    uint64_t resumptions;
} ecs_script_stats_t;

/* An instance retains its program. Mutations and explicit flushes use the
 * current world's main thread. Instances must be freed before ecs_fini(). */
SIECS_SCRIPT_API ecs_script_instance_t *ecs_script_instance_new(
    const ecs_script_t *program,
    const ecs_script_instance_desc_t *desc,
    ecs_script_eval_result_t *result
);
SIECS_SCRIPT_API ecs_script_instance_t *ecs_script_instance_run(
    const char *name,
    const char *code,
    const ecs_script_instance_desc_t *desc,
    ecs_script_eval_result_t *result
);
SIECS_SCRIPT_API ecs_script_instance_t *ecs_script_instance_load(
    const char *filename,
    const ecs_script_instance_desc_t *desc,
    ecs_script_eval_result_t *result
);
SIECS_SCRIPT_API int ecs_script_instance_reload(
    ecs_script_instance_t *instance,
    const ecs_script_t *program,
    ecs_script_eval_result_t *result
);
SIECS_SCRIPT_API void ecs_script_instance_free(ecs_script_instance_t *instance);
SIECS_SCRIPT_API ecs_script_instance_state_t
ecs_script_instance_state(const ecs_script_instance_t *instance);
SIECS_SCRIPT_API ecs_script_stats_t
ecs_script_instance_stats(const ecs_script_instance_t *instance);
/* Borrowed until the next evaluation, reload or destruction. */
SIECS_SCRIPT_API const ecs_script_eval_result_t *
ecs_script_instance_diagnostic(const ecs_script_instance_t *instance);
SIECS_SCRIPT_API int ecs_script_flush(ecs_script_eval_result_t *result);

typedef double (*ecs_script_clock_t)(void *context);
/* Select the seconds clock for async sleep on the world thread. NULL restores
 * the monotonic system clock. Switching preserves each timer's remaining delay.
 * The callback must be non-reentrant and its context must outlive its use.
 * Invalid initial samples reject the change; later nonfinite/backward samples
 * leave time unchanged. File watching continues to use real monotonic time.
 * Returns zero on success, or -1 for an invalid sample or an active async flush. */
SIECS_SCRIPT_API int ecs_script_clock_set(ecs_script_clock_t clock, void *context);

/* A runtime reuses local and value scratch storage across evaluations.
 * Use a separate runtime for each concurrent execution thread. */
SIECS_SCRIPT_API ecs_script_runtime_t *ecs_script_runtime_new(void);
SIECS_SCRIPT_API void ecs_script_runtime_free(ecs_script_runtime_t *runtime);
/* Evaluation publishes Script writes only after success. An evaluation error
 * discards staged entities, declarations, pairs and values. Native callbacks
 * remain responsible for their own effects. */
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

typedef uint64_t ecs_script_task_t;
typedef struct ecs_script_value_t {
    sireflect_handle_t type;
    const void *data;
} ecs_script_value_t;
typedef int (*ecs_script_function_callback_t)(
    const ecs_script_value_t *arguments,
    uint32_t count,
    void *result,
    void *context,
    const char **error
);
typedef void (*ecs_script_task_start_t)(
    ecs_script_task_t task,
    const ecs_script_value_t *arguments,
    uint32_t count,
    void *context
);
typedef void (*ecs_script_task_cancel_t)(ecs_script_task_t task, void *context);
typedef struct ecs_script_function_desc_t {
    const char *name;
    const sireflect_handle_t *parameters;
    uint32_t parameter_count;
    sireflect_handle_t result_type;
    ecs_type_ops_t result_ops;
    ecs_script_function_callback_t callback;
    ecs_script_task_start_t start;
    ecs_script_task_cancel_t cancel;
    void *context;
    void (*context_free)(void *context);
} ecs_script_function_desc_t;
/* Register on the main thread. A function supplies either callback (pure,
 * evaluated during preparation) or start (launched only after commit).
 * Async argument views remain valid until settlement or cancel returns.
 * Result lifecycle functions must be thread-safe when completion uses a worker. */
SIECS_SCRIPT_API int ecs_script_function_register(const ecs_script_function_desc_t *desc);
/* Complete once from any thread. Values are copied through result_ops. A stale,
 * already completed or cancelled handle returns false without copying data.
 * Failure to copy default reflected storage returns false and leaves the live
 * task unsettled, so the caller can retry. */
SIECS_SCRIPT_API bool ecs_script_task_complete(ecs_script_task_t task, const void *value);
SIECS_SCRIPT_API bool
ecs_script_task_fail(ecs_script_task_t task, const char *error_type, const char *message);

/* Source documents own parsed revisions. Edits preserve untouched bytes and
 * commit only after successful parsing; they never evaluate or mutate a world.
 * Offsets and lengths count bytes. Undo/redo also operate on source only. */
SIECS_SCRIPT_API ecs_script_document_t *
ecs_script_document_new(const char *name, const char *code, ecs_script_eval_result_t *result);
SIECS_SCRIPT_API void ecs_script_document_free(ecs_script_document_t *document);
/* Borrowed until the next edit, undo/redo or destruction. */
SIECS_SCRIPT_API const ecs_script_t *
ecs_script_document_script(const ecs_script_document_t *document);
SIECS_SCRIPT_API int ecs_script_document_edit(
    ecs_script_document_t *document,
    size_t offset,
    size_t length,
    const char *replacement,
    ecs_script_eval_result_t *result
);
SIECS_SCRIPT_API bool ecs_script_document_undo(ecs_script_document_t *document);
SIECS_SCRIPT_API bool ecs_script_document_redo(ecs_script_document_t *document);
/* Save through an exclusive sibling temporary file, then replace the target.
 * On error the existing target is preserved. This does not clear history. */
SIECS_SCRIPT_API int ecs_script_document_save(
    const ecs_script_document_t *document,
    const char *filename,
    ecs_script_eval_result_t *result
);

#ifdef __cplusplus
}
#endif
#endif
