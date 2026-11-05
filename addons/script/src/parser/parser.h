#ifndef SIECS_SCRIPT_PARSER_H
#define SIECS_SCRIPT_PARSER_H
#include "../platform.h"

typedef struct ecs_script_impl_t ecs_script_impl_t;
typedef struct ecs_script_scope_t ecs_script_scope_t;
typedef enum ecs_token_kind_t {
    EcsTokEnd = 0,
    EcsTokUnknown = 1,
    EcsTokAssign = '=',
    EcsTokAdd = '+',
    EcsTokSub = '-',
    EcsTokMul = '*',
    EcsTokDiv = '/',
    EcsTokMod = '%',
    EcsTokBitwiseOr = '|',
    EcsTokBitwiseAnd = '&',
    EcsTokNot = '!',
    EcsTokGt = '>',
    EcsTokLt = '<',
    EcsTokEq = 256,
    EcsTokNeq,
    EcsTokGtEq,
    EcsTokLtEq,
    EcsTokAnd,
    EcsTokOr,
    EcsTokRange,
    EcsTokShiftLeft,
    EcsTokShiftRight,
    EcsTokArrow,
    EcsTokMatch,
    EcsTokIdentifier,
    EcsTokString,
    EcsTokNumber,
    EcsTokChar,
    EcsTokHasBracketOpen
} ecs_token_kind_t;
typedef struct ecs_token_t {
    ecs_token_kind_t kind;
    const char *value;
    const char *begin;
    const char *end;
} ecs_token_t;
typedef struct ecs_parser_t {
    ecs_script_impl_t *script;
    ecs_script_scope_t *scope;
    const char *name;
    const char *code;
    const char *pos;
    const char *stmt_pos;
    ecs_token_t token;
    uint32_t depth;
    bool failed;
    bool async_scope;
} ecs_parser_t;
void script_lex(ecs_parser_t *parser);
void script_parse_error(ecs_parser_t *parser, const char *message);
bool script_take(ecs_parser_t *parser, int token);
bool script_expect(ecs_parser_t *parser, int token);
const char *script_parse_name(ecs_parser_t *parser);
void script_skip_lines(ecs_parser_t *parser);
void *script_node(ecs_parser_t *parser, size_t size, int kind, bool statement);
struct ecs_script_entity_t *script_parse_new_entity(ecs_parser_t *parser);
struct ecs_expr_node_t *script_parse_expression(ecs_parser_t *parser, int minimum);
struct ecs_expr_initializer_t *script_parse_arguments(ecs_parser_t *parser, int closing);
bool script_parse_scope(ecs_parser_t *parser, ecs_script_scope_t *scope, bool braces);
#endif
