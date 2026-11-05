#include "../script.h"

static const char *fragment_text(ecs_script_impl_t *impl, const char *begin, size_t length) {
    char *text = script_alloc(&impl->allocator, length + 1);
    memcpy(text, begin, length);
    return siecs_string_escape(text) ? text : NULL;
}

static bool valid_format(const char *format) {
    if (!format || strlen(format) > 24)
        return false;
    if (!*format)
        return true;
    const char *cursor = format;
    const char *flag_names = "-+ #0";
    uint32_t flags = 0;
    while (*cursor && strchr(flag_names, *cursor)) {
        uint32_t flag = 1u << (strchr(flag_names, *cursor) - flag_names);
        if (flags & flag)
            return false;
        flags |= flag;
        cursor++;
    }
    for (int part = 0; part < 2; part++) {
        if (part == 1) {
            if (*cursor != '.')
                break;
            cursor++;
        }
        uint32_t number = 0;
        while (isdigit((unsigned char)*cursor)) {
            number = number * 10 + (uint32_t)(*cursor++ - '0');
            if (number > 1024)
                return false;
        }
    }
    /* A precision is optional, but every nonempty format ends in a type. */
    const char *type = format + strlen(format) - 1;
    if (cursor != type || !strchr("duxXofFeEgGs", *type))
        return false;
    return *type != 's' || !(flags & ~1u);
}

int script_prepare_interpolation(ecs_script_t *script, ecs_expr_interpolated_string_t *node) {
    if (node->prepared)
        return 0;
    ecs_script_impl_t *impl = siecs_script_impl(script);
    const char *cursor = node->value;
    const char *literal = cursor;
    while (*cursor) {
        if (*cursor == '\\' && cursor[1]) {
            cursor += 2;
            continue;
        }
        bool variable = *cursor == '$' && (isalpha((unsigned char)cursor[1]) || cursor[1] == '_');
        if (!variable && *cursor != '{') {
            cursor++;
            continue;
        }
        ecs_expr_fragment_t fragment = { 0 };
        fragment.text = fragment_text(impl, literal, (size_t)(cursor - literal));
        if (!fragment.text)
            goto invalid;
        ecs_parser_t parser = { .script = impl,
                                .name = script->name,
                                .code = node->value,
                                .pos = variable ? cursor : cursor + 1 };
        script_lex(&parser);
        fragment.expr = script_parse_expression(&parser, variable ? 11 : 1);
        if (!fragment.expr || parser.failed)
            return -1;
        cursor = parser.token.begin;
        if (!variable) {
            if (*cursor == ':') {
                const char *format = cursor + 1;
                cursor = strchr(format, '}');
                if (!cursor)
                    goto invalid;
                fragment.format = fragment_text(impl, format, (size_t)(cursor - format));
                if (!valid_format(fragment.format))
                    goto invalid;
            }
            if (*cursor != '}')
                goto invalid;
            cursor++;
        }
        *(
            ecs_expr_fragment_t *
        )script_vec_append(&impl->allocator, &node->fragments, sizeof(fragment)) = fragment;
        literal = cursor;
    }
    if (cursor != literal) {
        ecs_expr_fragment_t fragment = {
            .text = fragment_text(impl, literal, (size_t)(cursor - literal))
        };
        if (!fragment.text)
            goto invalid;
        *(
            ecs_expr_fragment_t *
        )script_vec_append(&impl->allocator, &node->fragments, sizeof(fragment)) = fragment;
    }
    node->prepared = true;
    return 0;
invalid:
    ecs_parser_error(
        script->name,
        script->code,
        node->node.pos ? node->node.pos - script->code : 0,
        "invalid string interpolation"
    );
    return -1;
}
