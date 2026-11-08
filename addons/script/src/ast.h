#ifndef SIECS_SCRIPT_STATEMENTS_H
#define SIECS_SCRIPT_STATEMENTS_H

typedef enum ecs_script_node_kind_t {
    EcsAstScope,
    EcsAstTag,
    EcsAstComponent,
    EcsAstWith,
    EcsAstUsing,
    EcsAstModule,
    EcsAstAnnotation,
    EcsAstTemplate,
    EcsAstProp,
    EcsAstMut,
    EcsAstConst,
    EcsAstExportConst,
    EcsAstExportMut,
    EcsAstEntity,
    EcsAstPairScope,
    EcsAstIf,
    EcsAstFor,
    EcsAstInclude,
    EcsAstFunction,
    EcsAstAwait,
    EcsAstTry,
    EcsAstContinue,
    EcsAstAsync,
    EcsAstWhile,
    EcsAstAssign,
    EcsAstExpression
} ecs_script_node_kind_t;
typedef struct ecs_script_node_t {
    ecs_script_node_kind_t kind;
    const char *pos;
    const char *end;
} ecs_script_node_t;
struct ecs_script_scope_t {
    ecs_script_node_t node;
    sicore_vec_t stmts;
    struct script_instruction_t *instructions;
    ecs_script_scope_t *parent;
    ecs_script_scope_t *next;
};
typedef struct ecs_script_id_t {
    const char *first;
    const char *second;
    bool auto_override;
    ecs_expr_node_t *first_expr;
    ecs_expr_node_t *second_expr;
} ecs_script_id_t;
typedef struct ecs_script_tag_t {
    ecs_script_node_t node;
    ecs_script_id_t id;
} ecs_script_tag_t;
typedef struct ecs_script_component_t {
    ecs_script_node_t node;
    ecs_script_id_t id;
    ecs_expr_node_t *expr;
    bool is_partial;
} ecs_script_component_t;
struct ecs_script_entity_t {
    ecs_script_node_t node;
    const char *kind;
    const char *name;
    bool kind_w_expr;
    ecs_script_scope_t *scope;
    ecs_expr_node_t *name_expr;
    uint32_t anonymous_id;
    uint64_t identity_hash;
    ecs_script_entity_t *next_anonymous;
    sicore_vec_t capture_names;
    bool cacheable;
};
typedef struct ecs_script_template_node_t {
    ecs_script_node_t node;
    const char *name;
    const char *base;
    const char *parent;
    ecs_script_scope_t *scope;
} ecs_script_template_node_t;
typedef struct ecs_script_var_node_t {
    ecs_script_node_t node;
    const char *name;
    const char *type;
    ecs_expr_node_t *expr;
    bool is_await;
    bool type_is_template;
    bool type_is_vector;
} ecs_script_var_node_t;
typedef struct ecs_script_if_t {
    ecs_script_node_t node;
    ecs_expr_node_t *expr;
    ecs_script_scope_t *if_true;
    ecs_script_scope_t *if_false;
} ecs_script_if_t;
typedef struct ecs_script_for_t {
    ecs_script_node_t node;
    const char *loop_vars[3];
    int32_t loop_var_count;
    ecs_expr_node_t *from;
    ecs_expr_node_t *to;
    ecs_expr_node_t *expr;
    ecs_script_scope_t *scope;
} ecs_script_for_t;
typedef struct ecs_script_using_t {
    ecs_script_node_t node;
    const char *name;
} ecs_script_using_t;
typedef ecs_script_using_t ecs_script_module_t;
typedef struct ecs_script_include_t {
    ecs_script_node_t node;
    const char *filename;
    ecs_script_t *program;
} ecs_script_include_t;
typedef struct ecs_script_annot_t {
    ecs_script_node_t node;
    const char *name;
    const char *expr;
} ecs_script_annot_t;
typedef struct ecs_script_async_t {
    ecs_script_node_t node;
    ecs_script_scope_t *scope;
} ecs_script_async_t;
typedef struct ecs_script_while_t {
    ecs_script_node_t node;
    ecs_expr_node_t *expr;
    ecs_script_scope_t *scope;
} ecs_script_while_t;
typedef struct ecs_script_await_t {
    ecs_script_node_t node;
    ecs_expr_node_t *expr;
} ecs_script_await_t;
typedef struct ecs_script_assign_t {
    ecs_script_node_t node;
    const char *name;
    ecs_expr_node_t *expr;
} ecs_script_assign_t;
typedef struct ecs_script_pair_scope_t {
    ecs_script_node_t node;
    ecs_script_id_t id;
    ecs_script_scope_t *scope;
} ecs_script_pair_scope_t;
typedef struct ecs_script_with_t {
    ecs_script_node_t node;
    ecs_script_scope_t *expressions;
    ecs_script_scope_t *scope;
} ecs_script_with_t;
typedef struct ecs_script_catch_t {
    const char *error;
    ecs_script_scope_t *scope;
} ecs_script_catch_t;
typedef struct ecs_script_try_t {
    ecs_script_node_t node;
    ecs_script_scope_t *try_scope;
    sicore_vec_t catches;
} ecs_script_try_t;
typedef struct ecs_script_fn_param_t {
    const char *name;
    const char *type;
} ecs_script_fn_param_t;
typedef struct ecs_script_function_node_t {
    ecs_script_node_t node;
    const char *name;
    const char *return_type;
    sicore_vec_t params;
    ecs_script_scope_t *body;
    ecs_expr_node_t *return_expr;
} ecs_script_function_node_t;
ecs_script_scope_t *siecs_script_scope_new(ecs_parser_t *parser);
#endif
