/* Derived from Flecs. Copyright (c) 2025 Sander Mertens.
 * Portions Copyright (c) Meta Platforms, Inc. and affiliates.
 * Distributed under the MIT license in addons/script/LICENSE.flecs. */
/**
 * @file addons/script/expr/parser.c
 * @brief Script expression parser.
 */

#include "../script.h"

#ifdef SIECS_SCRIPT

#include "../script.h"
#include "../parser/grammar.h"
#include <errno.h>

/* From https://en.cppreference.com/w/c/language/operator_precedence */

static int siecs_expr_precedence[EcsTokLast] = {
    [EcsTokMember] = 1,
    [EcsTokBracketOpen] = 1,
    [EcsTokHasBracketOpen] = 1,
    [EcsTokNot] = 2,
    [EcsTokMul] = 3,
    [EcsTokDiv] = 3,
    [EcsTokMod] = 3,
    [EcsTokAdd] = 4,
    [EcsTokSub] = 4,
    [EcsTokShiftLeft] = 5,
    [EcsTokShiftRight] = 5,
    [EcsTokGt] = 6,
    [EcsTokGtEq] = 6,
    [EcsTokLt] = 6,
    [EcsTokLtEq] = 6,
    [EcsTokEq] = 7,
    [EcsTokNeq] = 7,
    [EcsTokBitwiseAnd] = 8,
    [EcsTokBitwiseOr] = 10,
    [EcsTokAnd] = 11,
    [EcsTokOr] = 12,
};

static const char* siecs_script_parse_lhs(
    ecs_parser_t *parser,
    const char *pos,
    ecs_tokenizer_t *tokenizer,
    ecs_token_kind_t left_oper,
    ecs_expr_node_t **out);

static void siecs_script_parser_expr_free(
    ecs_parser_t *parser,
    ecs_expr_node_t *node)
{
    siecs_expr_visit_free(&parser->script->pub, node);
}

static bool siecs_has_precedence(
    ecs_token_kind_t first,
    ecs_token_kind_t second)
{
    if (!siecs_expr_precedence[first]) {
        return false;
    }

    return siecs_expr_precedence[first] <= siecs_expr_precedence[second];
}

static const char* siecs_script_parse_match_elems(
    ecs_parser_t *parser,
    const char *pos,
    ecs_expr_match_t *node)
{
    ecs_allocator_t *a = &parser->script->allocator;
    bool old_significant_newline = parser->significant_newline;
    parser->significant_newline = true;

    ecs_vec_init_t(NULL, &node->elements, ecs_expr_match_element_t, 0);

    do {
        ParserBegin;

        pos = siecs_parser_skip_newlines(parser, pos);

        LookAhead(
            case '}': {
                Consume();
                parser->significant_newline = old_significant_newline;
                EndOfRule;
            }
        )

        ecs_expr_match_element_t *elem = ecs_vec_append_t(
            a, &node->elements, ecs_expr_match_element_t);
        ecs_os_zeromem(elem);

        pos = siecs_script_parse_expr(parser, pos, 0, &elem->compare);
        if (!pos) {
            goto error;
        }

        Parse_1(':', {
            pos = siecs_script_parse_expr(parser, pos, 0, &elem->expr);
            if (!pos) {
                goto error;
            }

            LookAhead_1('}', {
                Consume();
                parser->significant_newline = old_significant_newline;
                EndOfRule;
            })

            Parse(
                case ';':
                case '\n':
                    break;
            )
        })

    } while (true);

    ParserEnd;
}

const char* siecs_script_parse_initializer(
    ecs_parser_t *parser,
    const char *pos,
    char until,
    ecs_expr_node_t **node_out)
{
    ecs_expr_initializer_t *node = siecs_expr_initializer(parser);
    *node_out = (ecs_expr_node_t*)node;
    ecs_allocator_t *a = &parser->script->allocator;

    do {
        ParserBegin;

        /* End of initializer */
        LookAhead(
            case ']':
            case ')':
            case '}': {
                Consume();
                if ((char)lookahead_token.kind != until) {
                    goto unexpected_end;
                }

                if (!ecs_vec_count(&node->elements)) {
                    node->node.kind = EcsExprEmptyInitializer;
                }

                EndOfRule;
            })

        ecs_expr_initializer_element_t *elem = ecs_vec_append_t(
            a, &node->elements, ecs_expr_initializer_element_t);
        ecs_os_zeromem(elem);

        if (until != ']') {
            LookAheadAny_2(case EcsTokIdentifier: EcsTokKeyword:, case ':':,
                elem->member = Token(0);
                Consume();
            )
        }

        int16_t old_elem_depth = parser->elem_depth;
        parser->elem_depth = siecs_ito(int16_t, parser->expr_depth + 1);
        pos = siecs_script_parse_expr(parser, pos, 0, &elem->value);
        parser->elem_depth = old_elem_depth;
        if (!pos) {
            goto error;
        }

        if (until == ']') {
            bool is_key = false;

            LookAhead_1(':', {
                Consume();
                is_key = true;
            })

            if (is_key) {
                elem->key = elem->value;
                elem->value = NULL;

                parser->elem_depth = siecs_ito(int16_t, parser->expr_depth + 1);
                pos = siecs_script_parse_expr(parser, pos, 0, &elem->value);
                parser->elem_depth = old_elem_depth;
                if (!pos) {
                    goto error;
                }
            }
        }

        if (until == ']' && !elem->key && ecs_vec_count(&node->elements) == 1) {
            bool is_range = false;

            LookAhead_1(EcsTokRange, {
                Consume();
                is_range = true;
            })

            /* [expr .. expr] is a range expression */
            if (is_range) {
                ecs_expr_range_t *range = siecs_expr_range(parser);
                range->from = elem->value;
                elem->value = NULL;
                ecs_vec_remove_last(&node->elements);
                siecs_script_parser_expr_free(parser, (ecs_expr_node_t*)node);
                *node_out = (ecs_expr_node_t*)range;

                pos = siecs_script_parse_expr(parser, pos, 0, &range->to);
                if (!pos) {
                    goto error;
                }

                Parse_1(']', {
                    EndOfRule;
                })
            }
        }

        {
            bool separated = false;

            /* Parse next element or end of initializer */
            LookAhead(
                case ',': {
                    Consume();
                    separated = true;
                    break;
                }

                case ']':
                case ')':
                case '}':
                    if ((char)lookahead_token.kind == until) {
                        Consume();
                        EndOfRule;
                    }

                    if (until == '\n' && lookahead_token.kind == '}') {
                        EndOfRule;
                    }

                    Consume();
                    goto unexpected_end;

                case ';':
                case '\n':
                case '\0':
                    if (until == '\n') {
                        EndOfRule;
                    }

                    if (lookahead_token.kind == '\0') {
                        Consume();
                        goto unexpected_end;
                    }

                    break;
            )

            if (!separated) {
                bool newline = false;
                const char *next = siecs_scan_whitespace_and_comment(
                    parser, pos, &newline);
                if (!next) {
                    goto error;
                }

                if (!newline) {
                    ecs_parser_error(parser->name, parser->code,
                        siecs_parser_errpos(parser, next),
                        until == '\n'
                            ? "expected ',' or newline"
                            : "expected ',' or newline between elements");
                    goto error;
                }
            }
        }
    } while (true);

unexpected_end:
    if (until == '\n') {
        Error("expected newline");
    }

    Error("expected '%c'", until);
error:
    siecs_script_parser_expr_free(parser, *node_out);
    *node_out = NULL;
    return NULL;
}

static const char* siecs_script_parse_function(
    ecs_parser_t *parser,
    const char *pos,
    ecs_expr_node_t **out)
{
    ecs_expr_function_t *result = siecs_expr_function(parser);
    result->left = *out;

    ecs_expr_node_t *args = NULL;
    pos = siecs_script_parse_initializer(parser, pos, ')', &args);
    result->args = (ecs_expr_initializer_t*)args;
    if (!pos) {
        goto error;
    }

    *out = (ecs_expr_node_t*)result;

    return pos;
error:
    siecs_free_t(&parser->script->allocator, ecs_expr_function_t, result);
    return NULL;
}

static ecs_expr_node_t* siecs_script_parse_number(
    ecs_parser_t *parser,
    const char *expr,
    bool is_negative)
{
    int base = 10;
    if (expr[0] == '0' && (expr[1] == 'x' || expr[1] == 'X')) {
        base = 16;
    } else if (expr[0] == '0' && (expr[1] == 'b' || expr[1] == 'B')) {
        base = 2;
    }

    if (base == 10 &&
        (strchr(expr, '.') || strchr(expr, 'e') || strchr(expr, 'E')))
    {
        double v = atof(expr);
        return (ecs_expr_node_t*)siecs_expr_float(parser, is_negative ? -v : v);
    }

    errno = 0;
    uint64_t v = strtoull(base == 10 ? expr : expr + 2, NULL, base);
    if (errno == ERANGE) {
        return NULL;
    }

    if (!is_negative) {
        return (ecs_expr_node_t*)siecs_expr_uint(parser, v);
    }

    if (v > (uint64_t)INT64_MAX + 1) {
        return NULL;
    }

    return (ecs_expr_node_t*)siecs_expr_int(parser, (int64_t)(0 - v));
}

static const char* siecs_script_parse_rhs(
    ecs_parser_t *parser,
    const char *pos,
    ecs_tokenizer_t *tokenizer,
    ecs_token_kind_t left_oper,
    ecs_expr_node_t **out)
{
    const char *last_pos = pos;

    do {
        TokenFramePush();

        last_pos = pos;

        bool elem_end = false;
        if (!parser->significant_newline &&
            parser->elem_depth == parser->expr_depth)
        {
            if (!siecs_scan_whitespace_and_comment(parser, pos, &elem_end)) {
                goto error;
            }
        }

        const char *old_token_cur = parser->token_cur;
        ecs_token_t lookahead_token;
        const char *lookahead = siecs_token(
            parser, pos, &lookahead_token, true);
        siecs_token_restore(parser, old_token_cur);
        if (!lookahead) {
            goto error;
        }

        ecs_token_kind_t oper = lookahead_token.kind;
        bool is_oper = oper != EcsTokNot && siecs_expr_precedence[oper];

        if (elem_end && (oper == EcsTokBracketOpen ||
            (oper == EcsTokSub && isdigit((unsigned char)lookahead[0]))))
        {
            is_oper = false;
        }

        if (is_oper && !siecs_has_precedence(left_oper, oper)) {
            pos = lookahead;

            switch(oper) {
            case EcsTokBracketOpen: {
                ecs_expr_element_t *result = siecs_expr_element(parser);
                result->left = *out;

                *out = (ecs_expr_node_t*)result;

                pos = siecs_script_parse_expr(
                    parser, pos, 0, &result->index);
                if (!pos) {
                    goto error;
                }

                Parse_1(']', {});

                break;
            }

            case EcsTokHasBracketOpen: {
                ecs_expr_has_t *result = siecs_expr_has(parser);
                result->left = *out;

                *out = (ecs_expr_node_t*)result;

                bool is_pair = false;

                LookAhead_1('(', {
                    Consume();
                    is_pair = true;
                })

                Parse_1(EcsTokIdentifier, {
                    result->first = (ecs_expr_node_t*)
                        siecs_expr_identifier(parser, t->value);
                });

                if (is_pair) {
                    Parse_1(',', {});

                    Parse_1(EcsTokIdentifier, {
                        result->second = (ecs_expr_node_t*)
                            siecs_expr_identifier(parser, t->value);
                    });

                    Parse_1(')', {});
                }

                Parse_1(']', {});

                break;
            }

            case EcsTokMember: {
                Parse(
                case EcsTokIdentifier:
                EcsTokKeyword: {
                    ecs_expr_member_t *result = siecs_expr_member(parser);
                    result->left = *out;
                    result->member_name = Token(0);
                    *out = (ecs_expr_node_t*)result;

                    if (pos[0] == '(') {
                        pos = siecs_script_parse_function(
                            parser, pos + 1, out);
                        if (!pos) {
                            goto error;
                        }
                    }

                    break;
                });

                break;
            }

            default: {
                ecs_expr_binary_t *result = siecs_expr_binary(parser);
                result->left = *out;
                result->operator = oper;

                *out = (ecs_expr_node_t*)result;

                pos = siecs_script_parse_lhs(parser, pos, tokenizer,
                    result->operator, &result->right);
                if (!pos) {
                    goto error;
                }

                break;
            }
            };
        }

        TokenFramePop();
    } while (pos != last_pos);

    return pos;
error:
    return NULL;
}

static const char* siecs_script_parse_lhs(
    ecs_parser_t *parser,
    const char *pos,
    ecs_tokenizer_t *tokenizer,
    ecs_token_kind_t left_oper,
    ecs_expr_node_t **out)
{
    TokenFramePush();

    bool can_have_rhs = true;

    Parse(
        case EcsTokNumber: {
            *out = siecs_script_parse_number(parser, Token(0), false);
            if (!*out) {
                Error("integer literal '%s' is out of range", Token(0));
            }

            break;
        }

        case EcsTokChar: {
            *out = (ecs_expr_node_t*)siecs_expr_char(parser, Token(0));
            break;
        }

        case EcsTokString: {
            if (siecs_string_is_interpolated(Token(0))) {
                *out = (ecs_expr_node_t*)siecs_expr_interpolated_string(
                    parser, Token(0));
            } else {
                *out = (ecs_expr_node_t*)siecs_expr_string(parser, Token(0));
                if (!*out) {
                    goto error;
                }
            }

            break;
        }

        case EcsTokIdentifier: {
            const char *expr = Token(0);
            if (expr[0] == '$') {
                *out = (ecs_expr_node_t*)siecs_expr_variable(parser, &expr[1]);
            } else if (!ecs_os_strcmp(expr, "this")) {
                *out = (ecs_expr_node_t*)siecs_expr_variable(parser, expr);
            } else if (!ecs_os_strcmp(expr, "true")) {
                *out = (ecs_expr_node_t*)siecs_expr_bool(parser, true);
            } else if (!ecs_os_strcmp(expr, "false")) {
                *out = (ecs_expr_node_t*)siecs_expr_bool(parser, false);
            } else {
                char *last_elem = strrchr(expr, '.');
                if (last_elem && last_elem[1] == '$') {
                    /* Scoped global variable */
                    ecs_expr_variable_t *v = siecs_expr_variable(parser, expr);
                    ecs_os_memmove(&last_elem[1], &last_elem[2],
                        ecs_os_strlen(&last_elem[2]) + 1);
                    v->node.kind = EcsExprGlobalVariable;
                    *out = (ecs_expr_node_t*)v;
                } else {
                    /* Entity identifier */
                    *out = (ecs_expr_node_t*)siecs_expr_identifier(parser, expr);
                }
            }

            if (pos[0] == '(') {
                pos = siecs_script_parse_function(parser, pos + 1, out);
                if (!pos) {
                    goto error;
                }
            }

            break;
        }

        case EcsTokNot:
        case EcsTokSub: {
            ecs_token_kind_t oper = t->kind;
            if (oper == EcsTokSub && isdigit((unsigned char)pos[0])) {
                Parse_1(EcsTokNumber, {
                    *out = siecs_script_parse_number(parser, Token(1), true);
                    if (!*out) {
                        Error("integer literal '-%s' is out of range",
                            Token(1));
                    }
                })
                break;
            }

            ecs_expr_unary_t *node = siecs_expr_unary(parser);
            pos = siecs_script_parse_expr(parser, pos, EcsTokNot, &node->expr);
            if (!pos) {
                siecs_script_parser_expr_free(parser, (ecs_expr_node_t*)node);
                goto error;
            }

            node->operator = oper;
            *out = (ecs_expr_node_t*)node;
            break;
        }

        case EcsTokKeywordMatch: {
            ecs_expr_match_t *node = siecs_expr_match(parser);
            *out = (ecs_expr_node_t*)node;

            pos = siecs_script_parse_expr(parser, pos, 0, &node->expr);
            if (!pos) {
                goto error;
            }

            Parse_1('{', {
                pos = siecs_script_parse_match_elems(parser, pos, node);
                if (!pos) {
                    goto error;
                }
            })

            can_have_rhs = false;

            break;
        }

        case EcsTokKeywordNew: {
            ecs_expr_new_t *node = siecs_expr_new(parser);
            *out = (ecs_expr_node_t*)node;

            int32_t stmt_count = ecs_vec_count(&parser->scope->stmts);

            bool old_significant_newline = parser->significant_newline;
            const char *old_stmt_pos = parser->stmt_pos;
            parser->significant_newline = true;
            pos = siecs_script_stmt(parser, pos);
            parser->significant_newline = old_significant_newline;
            parser->stmt_pos = old_stmt_pos;
            if (!pos) {
                goto error;
            }

            if ((ecs_vec_count(&parser->scope->stmts) - stmt_count) != 1) {
                Error("expected entity statement after new");
            }

            ecs_script_node_t *script_node = ecs_vec_last_t(
                &parser->scope->stmts, ecs_script_node_t*)[0];
            if (script_node->kind != EcsAstEntity) {
                Error("expected entity statement after new");
            }

            ecs_vec_remove_last(&parser->scope->stmts);

            node->entity = (ecs_script_entity_t*)script_node;

            can_have_rhs = false;

            break;
        }

        case EcsTokKeywordScript: {
            ecs_expr_script_t *node = siecs_expr_script(parser);
            *out = (ecs_expr_node_t*)node;

            Parse_1('{', {
                node->script = siecs_script_parse_nested(parser, pos, &pos);
                if (!node->script) {
                    goto error;
                }
            })

            can_have_rhs = false;
            break;
        }

        case '(': {
            pos = siecs_script_parse_expr(parser, pos, 0, out);
            if (!pos) {
                goto error;
            }

            Parse_1(')', {})

            break;
        }

        case '{':
        case '[': {
            char until = tokenizer->tokens[0].kind == '{' ? '}' : ']';
            ecs_expr_node_t *node = NULL;
            pos = siecs_script_parse_initializer(parser, pos, until, &node);
            if (!pos) {
                goto error;
            }

            if (until == ']' && (node->kind == EcsExprInitializer ||
                node->kind == EcsExprEmptyInitializer))
            {
                ((ecs_expr_initializer_t*)node)->is_collection = true;
            }

            *out = node;
            can_have_rhs = until == ']';
            break;
        }
    )

    TokenFramePop();

    /* Return if this was end of expression, or if the parsed expression cannot
     * have a right-hand side. */
    if (!pos[0] || !can_have_rhs) {
        return pos;
    }

    /* Parse right-hand side of expression if there is one */
    return siecs_script_parse_rhs(parser, pos, tokenizer, left_oper, out);
error:
    return NULL;
}

const char* siecs_script_parse_expr(
    ecs_parser_t *parser,
    const char *pos,
    ecs_token_kind_t left_oper,
    ecs_expr_node_t **out)
{
    if (parser->expr_depth >= ECS_PARSER_MAX_RECURSION_DEPTH) {
        ecs_parser_error(parser->name, parser->code,
            siecs_parser_errpos(parser, pos),
            "maximum expression nesting depth exceeded");
        return NULL;
    }

    parser->expr_depth ++;

    ParserBegin;

    pos = siecs_script_parse_lhs(parser, pos, tokenizer, left_oper, out);
    if (!pos) {
        parser->expr_depth --;
        if (out && *out) {
            siecs_script_parser_expr_free(parser, *out);
            *out = NULL;
        }

        return NULL;
    }

    parser->expr_depth --;

    if (out && *out && !(*out)->end) {
        (*out)->end = siecs_parser_stmt_end(parser, pos);
    }

    return pos;
}


#endif
