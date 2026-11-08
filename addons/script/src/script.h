#ifndef SIECS_SCRIPT_INTERNAL_H
#define SIECS_SCRIPT_INTERNAL_H

#include "parser/parser.h"
#include "platform.h"
#include <stdatomic.h>

/* Component IDs and native/staged pair identities share Script's value pipeline. */
typedef uint64_t script_key_t;
#define SCRIPT_PAIR_KEY (UINT64_C(1) << 32)
#define SCRIPT_PAIR_PENDING (UINT64_C(1) << 33)

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

/* Lexical kinds are recorded without consulting a live registry. Type
 * resolution binds these to sireflect handles before evaluation. */
typedef enum script_literal_kind_t {
    ScriptLiteralBool,
    ScriptLiteralChar,
    ScriptLiteralI64,
    ScriptLiteralU64,
    ScriptLiteralF64,
    ScriptLiteralString
} script_literal_kind_t;

#include "ast.h"
#include "expr/ast.h"

typedef int (*siecs_script_scope_action_t)(ecs_script_scope_t *scope, void *ctx);

struct ecs_script_impl_t {
    ecs_script_t pub;
    atomic_uint references;
    ecs_allocator_t allocator;
    ecs_script_scope_t *root;
    ecs_script_scope_t *scopes;
    bool skip_unknown;
    bool ir_enabled;
    bool from_file;
    bool has_async;
    uint32_t anonymous_count;
    ecs_script_entity_t *anonymous;
    sicore_vec_t nested_scripts;
};
#define siecs_script_impl(script) ((ecs_script_impl_t *)(script))

ecs_script_t *siecs_script_new(void);
int script_ir_compile(ecs_script_t *script);
uint64_t script_source_hash(const char *begin, const char *end);
void script_adopt_nested(ecs_script_impl_t *parent, ecs_script_t *child);
ecs_script_scope_t *siecs_script_scope_new(ecs_parser_t *parser);
bool siecs_string_is_interpolated(const char *value);
char *siecs_string_escape(char *str);
int script_prepare_interpolation(ecs_script_t *script, ecs_expr_interpolated_string_t *node);
#endif
