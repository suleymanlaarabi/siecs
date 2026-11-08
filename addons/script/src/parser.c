#include "script.h"

#define statement(parser, T, kind) ((T *)script_node(parser, sizeof(T), kind, true))

static bool keyword(ecs_parser_t *parser, const char *word) {
    return parser->token.kind == EcsTokIdentifier && !strcmp(parser->token.value, word);
}

static bool consume_word(ecs_parser_t *parser, const char *word) {
    if (!keyword(parser, word))
        return false;
    script_lex(parser);
    return true;
}

static ecs_expr_node_t *name_expression(ecs_parser_t *parser, const char *name) {
    if (!name || !siecs_string_is_interpolated(name))
        return NULL;
    ecs_expr_interpolated_string_t *value =
        script_node(parser, sizeof(*value), EcsExprInterpolatedString, false);
    value->node.pos = parser->stmt_pos;
    value->value = name;
    if (script_prepare_interpolation(&parser->script->pub, value))
        parser->failed = true;
    return (ecs_expr_node_t *)value;
}

static ecs_script_scope_t *body(ecs_parser_t *parser) {
    ecs_script_scope_t *scope = siecs_script_scope_new(parser);
    script_skip_lines(parser);
    script_parse_scope(parser, scope, true);
    return scope;
}

static void parse_statement(ecs_parser_t *parser);

static const char *qualified_name(ecs_parser_t *parser) {
    const char *name = script_parse_name(parser);
    while (!parser->failed && (int)parser->token.kind == '.') {
        script_lex(parser);
        const char *part = script_parse_name(parser);
        if (!part)
            break;
        size_t a = strlen(name), b = strlen(part);
        char *joined = script_alloc(&parser->script->allocator, a + b + 2);
        memcpy(joined, name, a);
        joined[a] = '.';
        memcpy(joined + a + 1, part, b + 1);
        name = joined;
    }
    return name;
}

static void parse_variable(ecs_parser_t *parser, int kind) {
    ecs_script_var_node_t *var = statement(parser, ecs_script_var_node_t, kind);
    script_lex(parser);
    var->name = script_parse_name(parser);
    if (var->name && *var->name == '$')
        var->name++;
    if (script_take(parser, ':')) {
        var->type = qualified_name(parser);
        if (script_take(parser, '[')) {
            var->type_is_vector = true;
            script_expect(parser, ']');
        }
    }
    if (script_take(parser, '=')) {
        var->is_await = consume_word(parser, "await");
        if (var->is_await && !parser->async_scope)
            script_parse_error(parser, "await requires an async scope");
        if (var->is_await)
            parser->script->has_async = true;
        var->expr = script_parse_expression(parser, 1);
    } else if (!var->type) {
        script_parse_error(parser, "untyped variable requires an initializer");
    }
}

static ecs_script_id_t parse_pair(ecs_parser_t *parser) {
    ecs_script_id_t id = { 0 };
    script_expect(parser, '(');
    id.first = qualified_name(parser);
    id.first_expr = name_expression(parser, id.first);
    script_expect(parser, ',');
    id.second = qualified_name(parser);
    id.second_expr = name_expression(parser, id.second);
    script_expect(parser, ')');
    return id;
}

static void parse_component(ecs_parser_t *parser, ecs_script_id_t id) {
    int operation = parser->token.kind;
    if (operation == ':' || operation == '=' || operation == '(') {
        ecs_script_component_t *component =
            statement(parser, ecs_script_component_t, EcsAstComponent);
        component->id = id;
        component->is_partial = operation == '=';
        script_lex(parser);
        if (operation == '(') {
            component->expr = (ecs_expr_node_t *)script_parse_arguments(parser, ')');
        } else {
            component->expr = script_parse_expression(parser, 1);
        }
    } else {
        ecs_script_tag_t *tag = statement(parser, ecs_script_tag_t, EcsAstTag);
        tag->id = id;
    }
}

static void parse_entity(ecs_parser_t *parser, const char *kind, const char *name) {
    ecs_script_entity_t *entity = statement(parser, ecs_script_entity_t, EcsAstEntity);
    entity->kind = kind;
    entity->name = name && strcmp(name, "_") ? name : NULL;
    if (!entity->name) {
        entity->anonymous_id = ++siecs_script_impl(parser->script)->anonymous_count;
        entity->next_anonymous = parser->script->anonymous;
        parser->script->anonymous = entity;
    }
    entity->name_expr = name_expression(parser, entity->name);
    entity->scope = siecs_script_scope_new(parser);
    ecs_script_scope_t *previous = parser->scope;
    parser->scope = entity->scope;
    if (script_take(parser, '(')) {
        entity->kind_w_expr = true;
        ecs_script_component_t *call = statement(parser, ecs_script_component_t, EcsAstComponent);
        call->id.first = kind;
        call->expr = (ecs_expr_node_t *)script_parse_arguments(parser, ')');
    }
    if (script_take(parser, ':')) {
        ecs_script_tag_t *base = statement(parser, ecs_script_tag_t, EcsAstTag);
        base->id.first = "IsA";
        base->id.second = qualified_name(parser);
        base->id.second_expr = name_expression(parser, base->id.second);
    }
    parser->scope = previous;
    ecs_parser_t look = *parser;
    script_skip_lines(&look);
    if ((int)look.token.kind == '{') {
        *parser = look;
        script_parse_scope(parser, entity->scope, true);
    }
}

ecs_script_entity_t *script_parse_new_entity(ecs_parser_t *parser) {
    ecs_script_scope_t *scope = parser->scope;
    uint32_t count = scope->stmts.size;
    const char *begin = parser->token.begin;
    if ((int)parser->token.kind == '{')
        parse_entity(parser, NULL, NULL);
    else {
        const char *first = script_parse_name(parser);
        if ((int)parser->token.kind == '(')
            parse_entity(parser, first, NULL);
        else if (parser->token.kind == EcsTokIdentifier || parser->token.kind == EcsTokString)
            parse_entity(parser, first, script_parse_name(parser));
        else
            parse_entity(parser, NULL, first);
    }
    if (parser->failed || scope->stmts.size != count + 1)
        return NULL;
    ecs_script_entity_t *entity = ((ecs_script_entity_t **)scope->stmts.data)[count];
    scope->stmts.size = count;
    entity->node.pos = begin;
    entity->node.end = parser->token.begin;
    entity->identity_hash = script_source_hash(entity->node.pos, entity->node.end);
    return entity;
}

static void parse_if(ecs_parser_t *parser) {
    ecs_script_if_t *condition = statement(parser, ecs_script_if_t, EcsAstIf);
    script_lex(parser);
    condition->expr = script_parse_expression(parser, 1);
    condition->if_true = body(parser);
    condition->if_false = siecs_script_scope_new(parser);
    ecs_parser_t look = *parser;
    script_skip_lines(&look);
    if (keyword(&look, "else")) {
        *parser = look;
        script_lex(parser);
        if (keyword(parser, "if")) {
            ecs_script_scope_t *previous = parser->scope;
            parser->scope = condition->if_false;
            parse_if(parser);
            parser->scope = previous;
        } else {
            script_skip_lines(parser);
            script_parse_scope(parser, condition->if_false, true);
        }
    }
}

static void parse_function(ecs_parser_t *parser) {
    ecs_script_function_node_t *function =
        statement(parser, ecs_script_function_node_t, EcsAstFunction);
    script_lex(parser);
    function->name = script_parse_name(parser);
    script_expect(parser, '(');
    script_skip_lines(parser);
    while (!parser->failed && (int)parser->token.kind != ')') {
        ecs_script_fn_param_t param = { .name = script_parse_name(parser) };
        script_expect(parser, ':');
        param.type = qualified_name(parser);
        *(
            ecs_script_fn_param_t *
        )script_vec_append(&parser->script->allocator, &function->params, sizeof(param)) = param;
        if (!script_take(parser, ','))
            break;
        script_skip_lines(parser);
    }
    script_expect(parser, ')');
    script_expect(parser, EcsTokArrow);
    function->return_type = qualified_name(parser);
    function->body = siecs_script_scope_new(parser);
    ecs_script_scope_t *previous_scope = parser->scope;
    bool previous_async = parser->async_scope;
    parser->scope = function->body;
    parser->async_scope = false;
    script_skip_lines(parser);
    script_expect(parser, '{');
    script_skip_lines(parser);
    while (!parser->failed && parser->token.kind && (int)parser->token.kind != '}') {
        bool declaration = keyword(parser, "const") || keyword(parser, "mut") ||
                           keyword(parser, "if") || keyword(parser, "for") ||
                           keyword(parser, "while") || keyword(parser, "try") ||
                           keyword(parser, "fn") || keyword(parser, "async");
        ecs_parser_t look = *parser;
        script_lex(&look);
        if (declaration || (int)look.token.kind == '=') {
            parse_statement(parser);
        } else {
            consume_word(parser, "return");
            function->return_expr = script_parse_expression(parser, 1);
            script_skip_lines(parser);
            break;
        }
        script_skip_lines(parser);
    }
    script_expect(parser, '}');
    parser->scope = previous_scope;
    parser->async_scope = previous_async;
}

static void parse_statement(ecs_parser_t *parser) {
    if (parser->failed)
        return;
    parser->stmt_pos = parser->token.begin;
    if (keyword(parser, "const")) {
        parse_variable(parser, EcsAstConst);
    } else if (keyword(parser, "mut")) {
        parse_variable(parser, EcsAstMut);
    } else if (keyword(parser, "prop")) {
        parse_variable(parser, EcsAstProp);
    } else if (consume_word(parser, "export")) {
        if (keyword(parser, "const"))
            parse_variable(parser, EcsAstExportConst);
        else if (keyword(parser, "mut"))
            parse_variable(parser, EcsAstExportMut);
        else
            script_parse_error(parser, "export requires const or mut");
    } else if (keyword(parser, "if")) {
        parse_if(parser);
    } else if (consume_word(parser, "for")) {
        ecs_script_for_t *loop = statement(parser, ecs_script_for_t, EcsAstFor);
        bool multiple = script_take(parser, '(');
        do {
            if (loop->loop_var_count == 3) {
                script_parse_error(parser, "too many loop variables");
                return;
            }
            loop->loop_vars[loop->loop_var_count++] = script_parse_name(parser);
        } while (multiple && script_take(parser, ','));
        if (multiple)
            script_expect(parser, ')');
        if (!consume_word(parser, "in"))
            script_parse_error(parser, "expected in");
        loop->expr = script_parse_expression(parser, 1);
        if (loop->expr && loop->expr->kind == EcsExprRange) {
            loop->from = ((ecs_expr_range_t *)loop->expr)->from;
            loop->to = ((ecs_expr_range_t *)loop->expr)->to;
        }
        loop->scope = body(parser);
    } else if (consume_word(parser, "template")) {
        ecs_script_template_node_t *template =
            statement(parser, ecs_script_template_node_t, EcsAstTemplate);
        template->name = script_parse_name(parser);
        if (script_take(parser, ':'))
            template->base = qualified_name(parser);
        template->scope = body(parser);
    } else if (keyword(parser, "using") || keyword(parser, "module")) {
        int kind = keyword(parser, "using") ? EcsAstUsing : EcsAstModule;
        ecs_script_using_t *use = statement(parser, ecs_script_using_t, kind);
        script_lex(parser);
        use->name = qualified_name(parser);
    } else if (consume_word(parser, "include")) {
        ecs_script_include_t *include = statement(parser, ecs_script_include_t, EcsAstInclude);
        const char *begin = parser->token.begin;
        if (parser->token.kind == EcsTokString) {
            include->filename = parser->token.value;
            script_lex(parser);
        } else {
            while (parser->token.kind && (int)parser->token.kind != '\n' &&
                   (int)parser->token.kind != ';')
                script_lex(parser);
            size_t length = (size_t)(parser->token.begin - begin);
            while (length && isspace((unsigned char)begin[length - 1]))
                length--;
            char *name = script_alloc(&parser->script->allocator, length + 1);
            memcpy(name, begin, length);
            include->filename = name;
        }
        if (!*include->filename)
            script_parse_error(parser, "expected include filename");
        if (!parser->failed) {
            if (parser->scope != parser->script->root)
                script_parse_error(parser, "include must be declared in the root scope");
            const char *slash = strrchr(parser->name, '/');
            size_t prefix =
                slash && include->filename[0] != '/' ? (size_t)(slash - parser->name + 1) : 0;
            size_t length = strlen(include->filename);
            char *path = malloc(prefix + length + 1);
            if (!path) {
                script_parse_error(parser, "out of memory");
                return;
            }
            if (prefix)
                memcpy(path, parser->name, prefix);
            memcpy(path + prefix, include->filename, length + 1);
            include->program = ecs_script_parse_file(path, NULL, NULL);
            free(path);
            if (!include->program)
                parser->failed = true;
            else {
                script_adopt_nested(parser->script, include->program);
                parser->script->has_async |= siecs_script_impl(include->program)->has_async;
            }
        }
    } else if (consume_word(parser, "async")) {
        parser->script->has_async = true;
        ecs_script_async_t *block = statement(parser, ecs_script_async_t, EcsAstAsync);
        bool previous = parser->async_scope;
        parser->async_scope = true;
        block->scope = body(parser);
        parser->async_scope = previous;
    } else if (consume_word(parser, "await")) {
        if (!parser->async_scope)
            script_parse_error(parser, "await requires an async scope");
        parser->script->has_async = true;
        ecs_script_await_t *wait = statement(parser, ecs_script_await_t, EcsAstAwait);
        wait->expr = script_parse_expression(parser, 1);
    } else if (consume_word(parser, "while")) {
        ecs_script_while_t *loop = statement(parser, ecs_script_while_t, EcsAstWhile);
        loop->expr = script_parse_expression(parser, 1);
        loop->scope = body(parser);
    } else if (consume_word(parser, "continue")) {
        statement(parser, ecs_script_node_t, EcsAstContinue);
    } else if (keyword(parser, "fn")) {
        parse_function(parser);
    } else if (consume_word(parser, "try")) {
        ecs_script_try_t *attempt = statement(parser, ecs_script_try_t, EcsAstTry);
        attempt->try_scope = body(parser);
        script_skip_lines(parser);
        while (consume_word(parser, "catch")) {
            ecs_script_catch_t clause = { 0 };
            if (script_take(parser, '(')) {
                clause.error = qualified_name(parser);
                script_expect(parser, ')');
            }
            clause.scope = body(parser);
            *(
                ecs_script_catch_t *
            )script_vec_append(&parser->script->allocator, &attempt->catches, sizeof(clause)) =
                clause;
            script_skip_lines(parser);
        }
        if (!attempt->catches.size)
            script_parse_error(parser, "expected catch");
    } else if (consume_word(parser, "with")) {
        ecs_script_with_t *with = statement(parser, ecs_script_with_t, EcsAstWith);
        with->expressions = siecs_script_scope_new(parser);
        ecs_script_scope_t *previous = parser->scope;
        parser->scope = with->expressions;
        do {
            ecs_script_id_t id = { 0 };
            if ((int)parser->token.kind == '(')
                id = parse_pair(parser);
            else
                id.first = qualified_name(parser);
            parse_component(parser, id);
        } while (script_take(parser, ','));
        parser->scope = previous;
        with->scope = body(parser);
    } else if (script_take(parser, '@')) {
        ecs_script_annot_t *annotation = statement(parser, ecs_script_annot_t, EcsAstAnnotation);
        annotation->name = qualified_name(parser);
        const char *begin = parser->token.begin;
        bool quoted = parser->token.kind == EcsTokString;
        annotation->expr = quoted ? parser->token.value : NULL;
        if (quoted) {
            if (!siecs_string_escape((char *)annotation->expr))
                script_parse_error(parser, "invalid annotation string escape");
            script_lex(parser);
        } else {
            while (parser->token.kind && (int)parser->token.kind != '\n' &&
                   (int)parser->token.kind != ';' && (int)parser->token.kind != '}')
                script_lex(parser);
            size_t length = (size_t)(parser->token.begin - begin);
            while (length && isspace((unsigned char)begin[length - 1]))
                length--;
            char *text = script_alloc(&parser->script->allocator, length + 1);
            memcpy(text, begin, length);
            annotation->expr = text;
        }

    } else if ((int)parser->token.kind == '(') {
        ecs_script_id_t id = parse_pair(parser);
        if ((int)parser->token.kind == '{') {
            ecs_script_pair_scope_t *pair =
                statement(parser, ecs_script_pair_scope_t, EcsAstPairScope);
            pair->id = id;
            pair->scope = body(parser);
        } else {
            parse_component(parser, id);
        }
    } else if (parser->token.kind == EcsTokIdentifier || parser->token.kind == EcsTokString) {
        const char *first = script_parse_name(parser);
        if (!first)
            return;
        if ((int)parser->token.kind == '|' && !strcmp(first, "auto_override")) {
            script_lex(parser);
            ecs_script_id_t id = { 0 };
            if ((int)parser->token.kind == '(')
                id = parse_pair(parser);
            else
                id.first = qualified_name(parser);
            id.auto_override = true;
            parse_component(parser, id);
        } else if ((int)parser->token.kind == '=') {
            script_lex(parser);
            ecs_script_assign_t *assign = statement(parser, ecs_script_assign_t, EcsAstAssign);
            assign->name = *first == '$' ? first + 1 : first;
            assign->expr = script_parse_expression(parser, 1);
        } else if (
            (int)parser->token.kind == ':' || (int)parser->token.kind == '=' ||
            (int)parser->token.kind == '('
        ) {
            ecs_script_id_t id = { .first = first, .first_expr = name_expression(parser, first) };
            if ((int)parser->token.kind == '(') {
                parse_entity(parser, first, NULL);
            } else {
                parse_component(parser, id);
            }
        } else if (parser->token.kind == EcsTokIdentifier || parser->token.kind == EcsTokString) {
            const char *name = script_parse_name(parser);
            parse_entity(parser, first, name);
        } else if ((int)parser->token.kind == '{') {
            parse_entity(parser, NULL, first);
        } else {
            parse_component(
                parser,
                (ecs_script_id_t){ .first = first, .first_expr = name_expression(parser, first) }
            );
        }
    } else {
        script_parse_error(parser, "expected statement");
    }
}

bool script_parse_scope(ecs_parser_t *parser, ecs_script_scope_t *scope, bool braces) {
    if (++parser->depth > 64) {
        script_parse_error(parser, "maximum scope nesting depth exceeded");
        parser->depth--;
        return false;
    }
    ecs_script_scope_t *previous = parser->scope;
    parser->scope = scope;
    if (braces)
        script_expect(parser, '{');
    script_skip_lines(parser);
    while (!parser->failed && parser->token.kind && (int)parser->token.kind != '}') {
        const char *before = parser->token.begin;
        parse_statement(parser);
        if (scope->stmts.size) {
            ecs_script_node_t **nodes = scope->stmts.data;
            nodes[scope->stmts.size - 1]->end = parser->token.begin;
            ecs_script_node_t *node = nodes[scope->stmts.size - 1];
            if (node->kind == EcsAstEntity) {
                ecs_script_entity_t *entity = (ecs_script_entity_t *)node;
                if (!entity->name)
                    entity->identity_hash = script_source_hash(node->pos, node->end);
            }
        }
        if (before == parser->token.begin)
            script_parse_error(parser, "expected statement separator");
        script_skip_lines(parser);
    }
    scope->node.end = parser->token.end;
    if (braces)
        script_expect(parser, '}');
    else if (parser->token.kind)
        script_parse_error(parser, "unexpected closing scope");
    parser->scope = previous;
    parser->depth--;
    return !parser->failed;
}

ecs_script_t *ecs_script_parse(
    const char *name,
    const char *code,
    const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result
) {
    if (result) {
        *result = (ecs_script_eval_result_t){ 0 };
        siecs_log_capture_push(true);
    }
    ecs_script_t *script = NULL;
    if (!code || strlen(code) > (size_t)(INT32_MAX - 1) / 2) {
        ecs_parser_error(name, NULL, 0, "invalid or oversized script source");
    } else {
        script = siecs_script_new();
        ecs_script_impl_t *impl = siecs_script_impl(script);
        impl->pub.name = script_strdup(name ? name : "<script>");
        impl->pub.code = script_strdup(code);
        impl->skip_unknown = desc && desc->skip_unknown;
        impl->ir_enabled = desc && desc->ir;
        ecs_parser_t parser = { .script = impl,
                                .name = script->name,
                                .code = script->code,
                                .pos = script->code };
        script_lex(&parser);
        if (!script_parse_scope(&parser, impl->root, false) || script_ir_compile(script)) {
            ecs_script_free(script);
            script = NULL;
        }
    }
    if (result) {
        result->error = siecs_log_capture_pop();
        siecs_log_get_captured_error_pos(&result->line, &result->column);
    }
    return script;
}
