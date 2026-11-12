#ifndef SIECS_SCRIPT_EXPRESSION_H
#define SIECS_SCRIPT_EXPRESSION_H

typedef enum ecs_expr_node_kind_t {
    EcsExprValue,
    EcsExprInterpolatedString,
    EcsExprInitializer,
    EcsExprUnary,
    EcsExprBinary,
    EcsExprIdentifier,
    EcsExprVariable,
    EcsExprFunction,
    EcsExprMember,
    EcsExprElement,
    EcsExprHas,
    EcsExprMatch,
    EcsExprRange,
    EcsExprNew,
    EcsExprScript
} ecs_expr_node_kind_t;
struct ecs_expr_node_t {
    ecs_expr_node_kind_t kind;
    const char *pos;
    const char *end;
};
typedef struct ecs_expr_value_node_t {
    ecs_expr_node_t node;
    ecs_expr_small_value_t storage;
    script_literal_kind_t literal_kind;
} ecs_expr_value_node_t;
typedef struct ecs_expr_identifier_t {
    ecs_expr_node_t node;
    const char *value;
} ecs_expr_identifier_t;
typedef struct ecs_expr_variable_t {
    ecs_expr_node_t node;
    const char *name;
} ecs_expr_variable_t;
typedef struct ecs_expr_unary_t {
    ecs_expr_node_t node;
    ecs_expr_node_t *expr;
    ecs_token_kind_t operator;
} ecs_expr_unary_t;
typedef struct ecs_expr_binary_t {
    ecs_expr_node_t node;
    ecs_expr_node_t *left;
    ecs_expr_node_t *right;
    ecs_token_kind_t operator;
} ecs_expr_binary_t;
typedef struct ecs_expr_initializer_element_t {
    const char *member;
    ecs_expr_node_t *value;
} ecs_expr_initializer_element_t;
typedef struct ecs_expr_initializer_t {
    ecs_expr_node_t node;
    sicore_vec_t elements;
} ecs_expr_initializer_t;
typedef struct ecs_expr_member_t {
    ecs_expr_node_t node;
    ecs_expr_node_t *left;
    const char *member_name;
} ecs_expr_member_t;
typedef struct ecs_expr_element_t {
    ecs_expr_node_t node;
    ecs_expr_node_t *left;
    ecs_expr_node_t *index;
    ecs_expr_node_t *second;
} ecs_expr_element_t;
typedef struct ecs_expr_function_t {
    ecs_expr_node_t node;
    ecs_expr_node_t *left;
    ecs_expr_initializer_t *args;
    const char *function_name;
} ecs_expr_function_t;
typedef struct ecs_expr_range_t {
    ecs_expr_node_t node;
    ecs_expr_node_t *from;
    ecs_expr_node_t *to;
} ecs_expr_range_t;
typedef struct ecs_expr_has_t {
    ecs_expr_node_t node;
    ecs_expr_node_t *left;
    ecs_expr_node_t *first;
    ecs_expr_node_t *second;
} ecs_expr_has_t;
typedef struct ecs_expr_match_element_t {
    ecs_expr_node_t *compare;
    ecs_expr_node_t *expr;
} ecs_expr_match_element_t;
typedef struct ecs_expr_match_t {
    ecs_expr_node_t node;
    ecs_expr_node_t *expr;
    sicore_vec_t elements;
    ecs_expr_match_element_t any;
} ecs_expr_match_t;
typedef struct ecs_expr_script_t {
    ecs_expr_node_t node;
    ecs_script_t *script;
} ecs_expr_script_t;
typedef struct ecs_expr_new_t {
    ecs_expr_node_t node;
    ecs_script_entity_t *entity;
} ecs_expr_new_t;
typedef struct ecs_expr_fragment_t {
    const char *text;
    ecs_expr_node_t *expr;
    const char *format;
} ecs_expr_fragment_t;
typedef struct ecs_expr_interpolated_string_t {
    ecs_expr_node_t node;
    const char *value;
    sicore_vec_t fragments;
    bool prepared;
} ecs_expr_interpolated_string_t;
#endif
