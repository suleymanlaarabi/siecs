#include "../script.h"

void script_parse_error(ecs_parser_t *parser, const char *message) {
    if (!parser->failed) {
        const char *at = parser->token.begin ? parser->token.begin : parser->pos;
        ecs_parser_error(parser->name, parser->code, at - parser->code, "%s", message);
    }
    parser->failed = true;
}

static const char *token_text(ecs_parser_t *parser, const char *begin, const char *end) {
    size_t length = (size_t)(end - begin);
    char *text = script_alloc(&parser->script->allocator, length + 1);
    memcpy(text, begin, length);
    return text;
}

void script_lex(ecs_parser_t *parser) {
    const char *cursor = parser->pos;
    for (;;) {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r')
            cursor++;
        if (cursor[0] == '/' && cursor[1] == '/') {
            while (*cursor && *cursor != '\n')
                cursor++;
        } else if (cursor[0] == '/' && cursor[1] == '*') {
            const char *close = strstr(cursor + 2, "*/");
            if (!close) {
                parser->token.begin = cursor;
                script_parse_error(parser, "unterminated comment");
                cursor += strlen(cursor);
                break;
            }
            cursor = close + 2;
        } else {
            break;
        }
    }
    const char *begin = cursor;
    int kind = (unsigned char)*cursor;
    if (*cursor)
        cursor++;
    if (isalpha((unsigned char)*begin) || *begin == '_' || *begin == '$') {
        while (isalnum((unsigned char)*cursor) || *cursor == '_' || *cursor == '$')
            cursor++;
        kind = EcsTokIdentifier;
    } else if (isdigit((unsigned char)*begin)) {
        if (begin[0] == '0' && (cursor[0] == 'x' || cursor[0] == 'X')) {
            cursor++;
            while (isxdigit((unsigned char)*cursor))
                cursor++;
        } else {
            while (isdigit((unsigned char)*cursor))
                cursor++;
            if (*cursor == '.' && cursor[1] != '.') {
                cursor++;
                while (isdigit((unsigned char)*cursor))
                    cursor++;
            }
            if (*cursor == 'e' || *cursor == 'E') {
                cursor++;
                if (*cursor == '+' || *cursor == '-')
                    cursor++;
                while (isdigit((unsigned char)*cursor))
                    cursor++;
            }
        }
        if (*cursor == 'u' || *cursor == 'f')
            cursor++;
        kind = EcsTokNumber;
    } else if (*begin == '"' || *begin == '\'' || *begin == '`') {
        char delimiter = *begin;
        const char *contents = cursor;
        while (*cursor && *cursor != delimiter) {
            if (*cursor == '\\' && cursor[1])
                cursor++;
            cursor++;
        }
        parser->token = (ecs_token_t){ .begin = begin };
        if (!*cursor)
            script_parse_error(parser, "unterminated string");
        const char *text = token_text(parser, contents, cursor);
        if (*cursor)
            cursor++;
        parser->token =
            (ecs_token_t){ delimiter == '\'' ? EcsTokChar : EcsTokString, text, begin, cursor };
        parser->pos = cursor;
        return;
    } else {
        static const struct {
            const char *text;
            int kind;
        } pairs[] = { { "==", EcsTokEq },         { "!=", EcsTokNeq },
                      { ">=", EcsTokGtEq },       { "<=", EcsTokLtEq },
                      { "&&", EcsTokAnd },        { "||", EcsTokOr },
                      { "..", EcsTokRange },      { "<<", EcsTokShiftLeft },
                      { ">>", EcsTokShiftRight }, { "->", EcsTokArrow },
                      { "=>", EcsTokMatch },      { "?[", EcsTokHasBracketOpen } };
        for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
            if (begin[0] && begin[1] && begin[0] == pairs[i].text[0] &&
                begin[1] == pairs[i].text[1]) {
                cursor = begin + 2;
                kind = pairs[i].kind;
                break;
            }
        }
    }
    parser->token =
        (ecs_token_t){ (ecs_token_kind_t)kind, token_text(parser, begin, cursor), begin, cursor };
    parser->pos = cursor;
}

bool script_take(ecs_parser_t *parser, int token) {
    if ((int)parser->token.kind != token || parser->failed)
        return false;
    script_lex(parser);
    return true;
}

bool script_expect(ecs_parser_t *parser, int token) {
    if (script_take(parser, token))
        return true;
    char error[96];
    if (!parser->token.kind)
        snprintf(error, sizeof error, "unexpected end of script");
    else if (token < 128)
        snprintf(error, sizeof error, "expected '%c'", token);
    else
        snprintf(error, sizeof error, "expected identifier or value");
    script_parse_error(parser, error);
    return false;
}

void script_skip_lines(ecs_parser_t *parser) {
    while (script_take(parser, '\n') || script_take(parser, ';')) {
    }
}

const char *script_parse_name(ecs_parser_t *parser) {
    if (parser->token.kind != EcsTokIdentifier && parser->token.kind != EcsTokString) {
        script_parse_error(parser, "expected name");
        return NULL;
    }
    const char *name = parser->token.value;
    script_lex(parser);
    return name;
}
