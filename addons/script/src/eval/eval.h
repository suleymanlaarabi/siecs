#ifndef SIECS_SCRIPT_EVAL_H
#define SIECS_SCRIPT_EVAL_H
#include "../backend/siecs_backend.h"

/* Values refer to native reflection handles; storage is inline for scalars. */
typedef struct script_value_t {
    sireflect_handle_t type;
    const void *ptr;
    ecs_expr_small_value_t storage;
} script_value_t;

static inline const void *script_value_data(const script_value_t *value) {
    return value->ptr ? value->ptr : &value->storage;
}

typedef struct script_owned_value_t {
    ecs_component_t component;
    void *ptr;
} script_owned_value_t;

typedef struct script_variable_t {
    const char *name;
    script_value_t value;
} script_variable_t;

typedef struct script_template_t {
    struct script_template_t *next;
    const ecs_script_template_node_t *node;
    ecs_component_t component;
    script_variable_t *captures;
    uint32_t capture_count;
} script_template_t;

struct ecs_script_runtime_t {
    sicore_vec_t variables;
    sicore_vec_t owned_values;
    ecs_allocator_t values;
    bool busy;
};

typedef struct script_eval_t {
    const ecs_script_t *script;
    const char *position;
    sicore_vec_t variables;
    sicore_vec_t owned_values;
    ecs_allocator_t values;
    ecs_entity_t parent;
    ecs_entity_t entity;
    bool skip_unknown;
    bool ir;
    int32_t loop_depth;
    uint32_t variable_base;
    const script_variable_t *captures;
    uint32_t capture_count;
    script_template_t *templates;
    uint32_t template_depth;
} script_eval_t;

int script_eval_error(script_eval_t *ctx, const char *pos, const char *format, ...);
int script_eval_expr(script_eval_t *ctx, const ecs_expr_node_t *expr, script_value_t *out);
int script_value_write(script_eval_t *ctx, const ecs_expr_node_t *expr,
    sireflect_handle_t type, void *destination);
int script_value_convert(script_eval_t *ctx, const char *position,
    const script_value_t *value, sireflect_handle_t type, void *destination);
int script_operator_int64_t(const void *left, const void *right, void *out, ecs_token_kind_t operator);
int script_operator_uint64_t(const void *left, const void *right, void *out, ecs_token_kind_t operator);
int script_operator_double(const void *left, const void *right, void *out, ecs_token_kind_t operator);
int script_operator_bool(const void *left, const void *right, void *out, ecs_token_kind_t operator);
int script_eval_scope(script_eval_t *ctx, ecs_script_scope_t *scope);
const script_variable_t *script_find_variable(script_eval_t *ctx, const char *name);
script_template_t *script_template_lookup(script_eval_t *ctx, const char *name);
int script_template_declare(script_eval_t *ctx, const ecs_script_template_node_t *node);
int script_template_eval(script_eval_t *ctx, script_template_t *template,
    const ecs_expr_node_t *arguments);
#endif
