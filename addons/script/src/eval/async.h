#ifndef SIECS_SCRIPT_ASYNC_H
#define SIECS_SCRIPT_ASYNC_H
#include "eval.h"
struct script_transaction_t;
extern SCRIPT_THREAD_LOCAL ecs_script_instance_t *script_current_instance;
void script_instance_runtime_init(void);
void script_instance_async_activity(ecs_script_instance_t *instance, bool suspended);
bool script_instance_async_begin(ecs_script_instance_t *instance);
void script_instance_async_end(ecs_script_instance_t *instance);
int script_instance_async_prepare(
    ecs_script_instance_t *instance,
    struct script_transaction_t *transaction
);
void script_instance_async_published(struct script_transaction_t *transaction, void *context);
void script_instance_async_state(
    ecs_script_instance_t *instance,
    int before,
    int after,
    const char *message,
    int32_t line,
    int32_t column
);
int script_async_spawn(script_eval_t *ctx, const ecs_script_async_t *node);
int script_async_host_call(
    script_eval_t *ctx,
    const ecs_expr_function_t *call,
    script_value_t *out
);
int script_async_flush(bool resume_yields);
void script_async_names_changed(void);
void script_async_instance_free(ecs_script_instance_t *instance);
void script_async_fini(void);
void script_async_transaction_commit(struct script_transaction_t *transaction);
void script_async_transaction_fini(struct script_transaction_t *transaction);
script_eval_t *script_context_clone(const script_eval_t *source);
void script_context_copy(script_eval_t *destination, const script_eval_t *source);
bool script_context_same_captures(const script_eval_t *left, const script_eval_t *right);
void script_context_remap(struct script_transaction_t *transaction, script_eval_t *ctx);
void script_context_restore(script_eval_t *ctx, script_eval_t *snapshot, uint32_t owned_count);
void script_context_fini(script_eval_t *ctx);
void script_context_release_values(script_eval_t *ctx, uint32_t keep);
ecs_script_scope_t *script_matching_catch(const ecs_script_try_t *attempt, const char *message);
ecs_script_scope_t *
script_pair_expressions(script_eval_t *ctx, const ecs_script_pair_scope_t *pair);
void script_with_scope_push(
    script_eval_t *ctx,
    ecs_script_scope_t *expressions,
    const ecs_script_pair_scope_t *pair
);
int script_entity_scope_begin(script_eval_t *ctx, const ecs_script_entity_t *entity);
#endif
