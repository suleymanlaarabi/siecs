#include "../script.h"
#include <errno.h>

#define expression(parser, T, kind) ((T *)script_node(parser, sizeof(T), kind, false))

static int precedence(int token) {
    switch (token) {
    case EcsTokRange:
        return 1;
    case EcsTokOr:
        return 2;
    case EcsTokAnd:
        return 3;
    case '|':
        return 4;
    case '&':
        return 5;
    case EcsTokEq:
    case EcsTokNeq:
        return 6;
    case '<':
    case '>':
    case EcsTokLtEq:
    case EcsTokGtEq:
        return 7;
    case EcsTokShiftLeft:
    case EcsTokShiftRight:
        return 8;
    case '+':
    case '-':
        return 9;
    case '*':
    case '/':
    case '%':
        return 10;
    default:
        return 0;
    }
}

ecs_expr_initializer_t *script_parse_arguments(ecs_parser_t *parser, int closing) {
    ecs_expr_initializer_t *args = expression(parser, ecs_expr_initializer_t, EcsExprInitializer);
    script_skip_lines(parser);
    while (!parser->failed && parser->token.kind && (int)parser->token.kind != closing) {
        ecs_expr_initializer_element_t item = { 0 };
        if (parser->token.kind == EcsTokIdentifier) {
            ecs_parser_t look = *parser;
            script_lex(&look);
            if (look.token.kind == ':') {
                item.member = parser->token.value;
                script_lex(parser);
                script_lex(parser);
            }
        }
        item.value = script_parse_expression(parser, 1);
        if (!item.value)
            break;
        *(
            ecs_expr_initializer_element_t *
        )script_vec_append(&parser->script->allocator, &args->elements, sizeof(item)) = item;
        script_skip_lines(parser);
        if (!script_take(parser, ','))
            break;
        script_skip_lines(parser);
    }
    args->node.end = parser->token.end;
    script_expect(parser, closing);
    return args;
}

static ecs_expr_node_t *primary(ecs_parser_t *parser) {
    ecs_token_t token = parser->token;
    int kind = token.kind;
    if (kind == '-' || kind == '+' || kind == '!') {
        ecs_expr_unary_t *unary = expression(parser, ecs_expr_unary_t, EcsExprUnary);
        unary->operator = token.kind;
        script_lex(parser);
        unary->expr = script_parse_expression(parser, 11);
        return (ecs_expr_node_t *)unary;
    }
    if (kind == '(') {
        script_lex(parser);
        script_skip_lines(parser);
        ecs_expr_node_t *value = script_parse_expression(parser, 1);
        script_skip_lines(parser);
        script_expect(parser, ')');
        return value;
    }
    if (kind == '{' || kind == '[') {
        script_lex(parser);
        ecs_expr_initializer_t *value = script_parse_arguments(parser, kind == '{' ? '}' : ']');
        value->node.pos = token.begin;
        value->is_collection = kind == '[';
        return (ecs_expr_node_t *)value;
    }
    if (kind == EcsTokString && siecs_string_is_interpolated(token.value)) {
        ecs_expr_interpolated_string_t *value =
            expression(parser, ecs_expr_interpolated_string_t, EcsExprInterpolatedString);
        value->value = token.value;
        script_lex(parser);
        if (script_prepare_interpolation(&parser->script->pub, value))
            parser->failed = true;
        return (ecs_expr_node_t *)value;
    }
    if (kind == EcsTokNumber || kind == EcsTokString || kind == EcsTokChar ||
        (kind == EcsTokIdentifier &&
         (!strcmp(token.value, "true") || !strcmp(token.value, "false")))) {
        ecs_expr_value_node_t *value = expression(parser, ecs_expr_value_node_t, EcsExprValue);
        if (kind == EcsTokString || kind == EcsTokChar) {
            char *text = (char *)token.value;
            char *end = siecs_string_escape(text);
            if (!end || (kind == EcsTokChar && end - text != 1)) {
                script_parse_error(parser, "invalid string or character escape");
            }
            if (kind == EcsTokString) {
                value->literal_kind = ScriptLiteralString;
                value->storage.string = text;
            } else {
                value->literal_kind = ScriptLiteralChar;
                value->storage.char_ = *text;
            }
        } else if (kind == EcsTokIdentifier) {
            value->literal_kind = ScriptLiteralBool;
            value->storage.bool_ = !strcmp(token.value, "true");
        } else {
            char *end;
            errno = 0;
            size_t length = strlen(token.value);
            bool hex = length > 2 && token.value[0] == '0' &&
                       (token.value[1] == 'x' || token.value[1] == 'X');
            bool real = !hex && (strchr(token.value, '.') || strchr(token.value, 'e') ||
                                 strchr(token.value, 'E') || token.value[length - 1] == 'f');
            if (real) {
                value->literal_kind = ScriptLiteralF64;
                value->storage.f64 = strtod(token.value, &end);
            } else if (token.value[length - 1] == 'u') {
                value->literal_kind = ScriptLiteralU64;
                value->storage.u64 = strtoull(token.value, &end, hex ? 16 : 10);
            } else {
                value->literal_kind = ScriptLiteralI64;
                value->storage.i64 = strtoll(token.value, &end, hex ? 16 : 10);
            }
            if (errno || (*end && strcmp(end, "f") && strcmp(end, "u")))
                script_parse_error(parser, "numeric literal out of range or invalid");
        }
        script_lex(parser);
        value->node.end = token.end;
        return (ecs_expr_node_t *)value;
    }
    if (kind == EcsTokIdentifier && !strcmp(token.value, "new")) {
        ecs_expr_new_t *created = expression(parser, ecs_expr_new_t, EcsExprNew);
        script_lex(parser);
        created->entity = script_parse_new_entity(parser);
        return created->entity ? (ecs_expr_node_t *)created : NULL;
    }
    if (kind == EcsTokIdentifier && !strcmp(token.value, "match")) {
        ecs_expr_match_t *match = expression(parser, ecs_expr_match_t, EcsExprMatch);
        script_lex(parser);
        match->expr = script_parse_expression(parser, 1);
        script_expect(parser, '{');
        script_skip_lines(parser);
        while (!parser->failed && parser->token.kind && (int)parser->token.kind != '}') {
            bool wildcard =
                parser->token.kind == EcsTokIdentifier && !strcmp(parser->token.value, "_");
            ecs_expr_match_element_t arm = { 0 };
            if (wildcard)
                script_lex(parser);
            else
                arm.compare = script_parse_expression(parser, 1);
            if (!script_take(parser, EcsTokMatch))
                script_expect(parser, ':');
            arm.expr = script_parse_expression(parser, 1);
            if (wildcard) {
                if (match->any.expr)
                    script_parse_error(parser, "duplicate match wildcard");
                match->any = arm;
            } else
                *(
                    ecs_expr_match_element_t *
                )script_vec_append(&parser->script->allocator, &match->elements, sizeof(arm)) = arm;
            script_skip_lines(parser);
            if (!script_take(parser, ','))
                break;
            script_skip_lines(parser);
        }
        script_expect(parser, '}');
        return (ecs_expr_node_t *)match;
    }
    if (kind == EcsTokIdentifier && !strcmp(token.value, "script")) {
        ecs_expr_script_t *value = expression(parser, ecs_expr_script_t, EcsExprScript);
        script_lex(parser);
        if ((int)parser->token.kind != '{') {
            script_parse_error(parser, "expected '{' after script");
            return NULL;
        }
        const char *begin = parser->token.end;
        unsigned depth = 1;
        script_lex(parser);
        while (!parser->failed && parser->token.kind && depth) {
            if ((int)parser->token.kind == '{')
                depth++;
            if ((int)parser->token.kind == '}')
                depth--;
            if (depth)
                script_lex(parser);
        }
        if (depth) {
            script_parse_error(parser, "unexpected end of nested script");
            return NULL;
        }
        size_t length = (size_t)(parser->token.begin - begin);
        char *source = malloc(length + 1);
        if (!source) {
            script_parse_error(parser, "out of memory");
            return NULL;
        }
        memcpy(source, begin, length);
        source[length] = 0;
        value->script = ecs_script_parse(parser->name, source, NULL, NULL);
        free(source);
        if (!value->script) {
            parser->failed = true;
            return NULL;
        }
        script_adopt_nested(parser->script, value->script);
        script_lex(parser);
        return (ecs_expr_node_t *)value;
    }
    if (kind == EcsTokIdentifier) {
        ecs_expr_node_t *value;
        if (*token.value == '$') {
            ecs_expr_variable_t *variable =
                expression(parser, ecs_expr_variable_t, EcsExprVariable);
            variable->name = token.value + 1;
            value = (ecs_expr_node_t *)variable;
        } else {
            ecs_expr_identifier_t *identifier =
                expression(parser, ecs_expr_identifier_t, EcsExprIdentifier);
            identifier->value = token.value;
            value = (ecs_expr_node_t *)identifier;
        }
        script_lex(parser);
        return value;
    }
    script_parse_error(
        parser,
        parser->token.kind ? "expected expression" : "unexpected end of script"
    );
    return NULL;
}

ecs_expr_node_t *script_parse_expression(ecs_parser_t *parser, int minimum) {
    if (parser->failed)
        return NULL;
    if (++parser->depth > 64) {
        script_parse_error(parser, "maximum expression nesting depth exceeded");
        parser->depth--;
        return NULL;
    }
    ecs_expr_node_t *left = primary(parser);
    while (left && !parser->failed) {
        int token = parser->token.kind;
        if (token == '.') {
            ecs_expr_member_t *member = expression(parser, ecs_expr_member_t, EcsExprMember);
            member->node.pos = left->pos;
            member->left = left;
            script_lex(parser);
            member->member_name = script_parse_name(parser);
            left = (ecs_expr_node_t *)member;
        } else if (token == '[' || token == EcsTokHasBracketOpen) {
            script_lex(parser);
            ecs_expr_node_t *index = script_parse_expression(parser, 1);
            script_expect(parser, ']');
            if (token == '[') {
                ecs_expr_element_t *element =
                    expression(parser, ecs_expr_element_t, EcsExprElement);
                element->node.pos = left->pos;
                element->left = left;
                element->index = index;
                left = (ecs_expr_node_t *)element;
            } else {
                ecs_expr_has_t *has = expression(parser, ecs_expr_has_t, EcsExprHas);
                has->node.pos = left->pos;
                has->left = left;
                has->first = index;
                left = (ecs_expr_node_t *)has;
            }
        } else if (token == '(') {
            ecs_expr_function_t *call = expression(parser, ecs_expr_function_t, EcsExprFunction);
            call->node.pos = left->pos;
            call->left = left;
            if (left->kind == EcsExprIdentifier)
                call->function_name = ((ecs_expr_identifier_t *)left)->value;
            script_lex(parser);
            call->args = script_parse_arguments(parser, ')');
            left = (ecs_expr_node_t *)call;
        } else {
            int binding = precedence(token);
            if (binding < minimum || !binding)
                break;
            const char *begin = left->pos;
            script_lex(parser);
            ecs_expr_node_t *right = script_parse_expression(parser, binding + 1);
            if (token == EcsTokRange) {
                ecs_expr_range_t *range = expression(parser, ecs_expr_range_t, EcsExprRange);
                range->from = left;
                range->to = right;
                left = (ecs_expr_node_t *)range;
            } else {
                ecs_expr_binary_t *binary = expression(parser, ecs_expr_binary_t, EcsExprBinary);
                binary->left = left;
                binary->right = right;
                binary->operator = (ecs_token_kind_t)token;
                left = (ecs_expr_node_t *)binary;
            }
            left->pos = begin;
        }
    }
    if (left)
        left->end = parser->token.begin;
    parser->depth--;
    return parser->failed ? NULL : left;
}
