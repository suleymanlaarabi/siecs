#ifndef SIECS_SCRIPT_INTERNAL_H
#define SIECS_SCRIPT_INTERNAL_H

#include "platform.h"
#include "parser/parser.h"

typedef struct ecs_expr_node_t ecs_expr_node_t;
typedef struct ecs_script_entity_t ecs_script_entity_t;
typedef union ecs_expr_small_value_t {
    bool bool_;
    char char_;
    int64_t i64;
    uint64_t u64;
    double f64;
    char *string;
    ecs_entity_t entity;
    char small_data[24];
} ecs_expr_small_value_t;

typedef struct ecs_function_calldata_t {
    ecs_entity_t function;
    void *ctx;
} ecs_function_calldata_t;

/* Lexical kinds are recorded without consulting a live registry. Type
 * resolution binds these to sireflect handles before evaluation. */
typedef enum script_literal_kind_t {
    ScriptLiteralBool, ScriptLiteralChar, ScriptLiteralI64,
    ScriptLiteralU64, ScriptLiteralF64, ScriptLiteralString
} script_literal_kind_t;

#include "expr/ast.h"
#include "ast.h"

typedef int (*siecs_script_scope_action_t)(ecs_script_scope_t *scope, void *ctx);

struct ecs_script_impl_t {
    ecs_script_t pub;
    ecs_allocator_t allocator;
    ecs_script_scope_t *root;
    char *token_buffer;
    char *token_remaining;
    int32_t token_buffer_size;
    bool skip_unknown;
    bool ir_enabled;
    sicore_vec_t nested_scripts;
};
#define siecs_script_impl(script) ((ecs_script_impl_t*)(script))

ecs_script_t *siecs_script_new(void);
int script_ir_compile(ecs_script_t *script);
void script_adopt_nested(ecs_script_impl_t *parent, ecs_script_t *child);
ecs_script_scope_t *siecs_script_scope_new(ecs_parser_t *parser);
int siecs_script_visit_scopes(ecs_script_node_t *node,
    siecs_script_scope_action_t action, void *ctx);
const char *siecs_script_stmt(ecs_parser_t *parser, const char *pos);
const char *siecs_script_stmt_w_separator(ecs_parser_t *parser, const char *pos);
ecs_script_t *siecs_script_parse_init(const char *name,
    const char *code, ecs_parser_t *parser);
ecs_script_t *siecs_script_parse_nested(const ecs_parser_t *outer,
    const char *code, const char **next);
const char *siecs_script_parse_expr(ecs_parser_t *parser, const char *pos,
    ecs_token_kind_t left_oper, ecs_expr_node_t **out);
const char *siecs_script_parse_initializer(ecs_parser_t *parser, const char *pos,
    char until, ecs_expr_node_t **out);
void siecs_expr_visit_free(ecs_script_t *script, ecs_expr_node_t *node);
bool siecs_string_is_interpolated(const char *value);
char *siecs_string_escape(char *str);
int script_prepare_interpolation(ecs_script_t *script, ecs_expr_interpolated_string_t *node);

#endif
