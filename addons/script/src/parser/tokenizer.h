/* Derived from Flecs. Copyright (c) 2025 Sander Mertens.
 * Portions Copyright (c) Meta Platforms, Inc. and affiliates.
 * Distributed under the MIT license in addons/script/LICENSE.flecs. */
/**
 * @file addons/parser/tokenizer.h
 * @brief Parser tokenizer.
 */

#ifndef SIECS_PARSER_TOKENIZER_H
#define SIECS_PARSER_TOKENIZER_H

#ifndef ECS_PARSER_MAX_TOKENS
#define ECS_PARSER_MAX_TOKENS (32)
#endif

/* Tokenizer */
typedef enum ecs_token_kind_t {
    EcsTokEnd = '\0',
    EcsTokUnknown,
    EcsTokScopeOpen = '{',
    EcsTokScopeClose = '}',
    EcsTokParenOpen = '(',
    EcsTokParenClose = ')',
    EcsTokBracketOpen = '[',
    EcsTokBracketClose = ']',
    EcsTokMember = '.',
    EcsTokComma = ',',
    EcsTokSemiColon = ';',
    EcsTokColon = ':',
    EcsTokAssign = '=',
    EcsTokAdd = '+',
    EcsTokSub = '-',
    EcsTokMul = '*',
    EcsTokDiv = '/',
    EcsTokMod = '%',
    EcsTokBitwiseOr = '|',
    EcsTokBitwiseAnd = '&',
    EcsTokNot = '!',
    EcsTokOptional = '?',
    EcsTokAnnotation = '@',
    EcsTokNewline = '\n',
    EcsTokChar = '\'',
    EcsTokGt = '>',
    EcsTokLt = '<',
    EcsTokEq = 128,
    EcsTokNeq,
    EcsTokGtEq,
    EcsTokLtEq,
    EcsTokAnd,
    EcsTokOr,
    EcsTokMatch,
    EcsTokRange,
    EcsTokShiftLeft,
    EcsTokShiftRight,
    EcsTokHasBracketOpen,
    EcsTokArrow,
    EcsTokIdentifier,
    EcsTokString,
    EcsTokNumber,
    EcsTokKeywordWith,
    EcsTokKeywordUsing,
    EcsTokKeywordProp,
    EcsTokKeywordMut,
    EcsTokKeywordConst,
    EcsTokKeywordMatch,
    EcsTokKeywordNew,
    EcsTokKeywordExport,
    EcsTokKeywordIf,
    EcsTokKeywordElse,
    EcsTokKeywordFor,
    EcsTokKeywordIn,
    EcsTokKeywordTemplate,
    EcsTokKeywordModule,
    EcsTokKeywordInclude,
    EcsTokKeywordFn,
    EcsTokKeywordAwait,
    EcsTokKeywordTry,
    EcsTokKeywordCatch,
    EcsTokKeywordContinue,
    EcsTokKeywordAsync,
    EcsTokKeywordWhile,
    EcsTokKeywordScript,
    EcsTokLast
} ecs_token_kind_t;

typedef struct ecs_token_t {
    const char *value;
    ecs_token_kind_t kind;
} ecs_token_t;

typedef struct ecs_tokenizer_t {
    int32_t count;
    ecs_token_t stack[ECS_PARSER_MAX_TOKENS];
    ecs_token_t *tokens;
} ecs_tokenizer_t;

const char* siecs_tokenizer_line(
    ecs_parser_t *parser,
    const char *ptr,
    ecs_token_t *out,
    bool stmt_end);

const char* siecs_token_kind_str(
    ecs_token_kind_t kind);

const char* siecs_token_str(
    ecs_token_kind_t kind);

const char* siecs_token(
    ecs_parser_t *parser,
    const char *ptr,
    ecs_token_t *out,
    bool is_lookahead);

void siecs_token_restore(
    ecs_parser_t *parser,
    const char *token_cur);

const char* siecs_scan_whitespace(
    ecs_parser_t *parser,
    const char *pos);

const char* siecs_scan_whitespace_and_comment(
    ecs_parser_t *parser,
    const char *pos,
    bool *newline);

const char* siecs_parser_skip_newlines(
    ecs_parser_t *parser,
    const char *pos);

const char* siecs_tokenizer_identifier(
    ecs_parser_t *parser,
    const char *pos,
    ecs_token_t *out);

int64_t siecs_parser_errpos(
    const ecs_parser_t *parser,
    const char *pos);

const char* siecs_parser_stmt_end(
    const ecs_parser_t *parser,
    const char *pos);

#endif
