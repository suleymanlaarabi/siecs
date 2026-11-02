/* Derived from Flecs. Copyright (c) 2025 Sander Mertens.
 * Portions Copyright (c) Meta Platforms, Inc. and affiliates.
 * Distributed under the MIT license in addons/script/LICENSE.flecs. */
/**
 * @file addons/parser/parser.h
 * @brief Parser addon.
 */

#ifndef SIECS_PARSER_H
#define SIECS_PARSER_H

#include "../platform.h"

#ifdef SIECS_PARSER

typedef struct ecs_script_impl_t ecs_script_impl_t;
typedef struct ecs_script_scope_t ecs_script_scope_t;

#ifndef ECS_PARSER_MAX_RECURSION_DEPTH
#define ECS_PARSER_MAX_RECURSION_DEPTH (64)
#endif

typedef struct ecs_parser_t {
    const char *name;
    const char *code;

    const char *pos;
    const char *fixed_pos;
    const char *stmt_pos;
    const char *token_ends[8];
    int32_t token_ends_i;
    const char *expr_pos;
    const char *expr_end;
    char *token_cur;
    char *token_end;
    char *token_keep;
    bool significant_newline;
    bool merge_variable_members;
    bool async_scope;
    int16_t scope_depth;
    int16_t expr_depth;
    int16_t elem_depth;


    /* For script parser */
    ecs_script_impl_t *script;
    ecs_script_scope_t *scope;

} ecs_parser_t;

#include "tokenizer.h"

#endif

#endif
