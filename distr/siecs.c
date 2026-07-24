#include "siecs.h"
/* Embedded dependency implementations for standalone distribution. */

#ifndef NDEBUG
#include <stdio.h>
#include <stdlib.h>

void sireflect_assert_fail(
    const char *condition,
    const char *message,
    const char *file,
    int line,
    const char *function
) {
    fprintf(stderr, "sireflect assertion failed: %s\n", message != NULL ? message : condition);
    fprintf(stderr, "  condition: %s\n", condition != NULL ? condition : "(unknown)");
    fprintf(stderr, "  location: %s:%d\n", file != NULL ? file : "(unknown)", line);
    fprintf(stderr, "  function: %s\n", function != NULL ? function : "(unknown)");
    abort();
}
#endif

#ifndef SIREFLECT_ERROR_H
#define SIREFLECT_ERROR_H

void sireflect_error_clear(void);
void sireflect_error_set(const char *message);

#endif

#include <stdlib.h>
#include <string.h>

static char *sireflect_current_error = NULL;

static char *sireflect_error_dup(const char *message) {
    sireflect_assert(message != NULL, "error message must not be NULL");

    const size_t len = strlen(message);
    char *copy = malloc(len + 1);
    sireflect_assert(copy != NULL, "failed to allocate error message");

    memcpy(copy, message, len + 1);
    return copy;
}

void sireflect_error_clear(void) {
    free(sireflect_current_error);
    sireflect_current_error = NULL;
}

void sireflect_error_set(const char *message) {
    sireflect_error_clear();

    if (message == NULL) {
        return;
    }

    sireflect_current_error = sireflect_error_dup(message);
}

const char *sireflect_error(void) {
    return sireflect_current_error;
}

const sireflect_field_info_t *
sireflect_field_info(const sireflect_registry_t *reg, sireflect_handle_t type, const char *field) {
    sireflect_error_clear();

    sireflect_assert(field != NULL, "field name must not be NULL");

    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);
    for (size_t i = 0; i < fields->field_count; i++) {
        if (strcmp(fields->fields[i].name, field) == 0) {
            return &fields->fields[i];
        }
    }

    return NULL;
}

sireflect_handle_t
sireflect_field_type(const sireflect_registry_t *reg, sireflect_handle_t type, const char *field) {
    sireflect_error_clear();

    const sireflect_field_info_t *info = sireflect_field_info(reg, type, field);
    sireflect_assert(info != NULL, "field must exist");
    return info->type;
}

size_t
sireflect_field_size(const sireflect_registry_t *reg, sireflect_handle_t ref, const char *field) {
    sireflect_error_clear();

    const sireflect_field_info_t *info = sireflect_field_info(reg, ref, field);
    sireflect_assert(info != NULL, "field must exist");
    return info->size;
}

const void *sireflect_field_ptr(
    const sireflect_registry_t *reg,
    sireflect_handle_t type,
    const void *obj,
    const char *field
) {
    sireflect_error_clear();

    sireflect_assert(obj != NULL, "object pointer must not be NULL");

    const sireflect_field_info_t *info = sireflect_field_info(reg, type, field);
    sireflect_assert(info != NULL, "field must exist");

    return (const unsigned char *)obj + info->offset;
}

void *sireflect_field_mut_ptr(
    const sireflect_registry_t *reg,
    sireflect_handle_t type,
    void *obj,
    const char *field
) {
    sireflect_error_clear();

    sireflect_assert(obj != NULL, "object pointer must not be NULL");

    const sireflect_field_info_t *info = sireflect_field_info(reg, type, field);
    sireflect_assert(info != NULL, "field must exist");

    return (unsigned char *)obj + info->offset;
}

int sireflect_field_copy(
    const sireflect_registry_t *reg,
    sireflect_handle_t type,
    void *obj,
    const char *field,
    const void *value
) {
    sireflect_error_clear();

    sireflect_assert(value != NULL, "source value pointer must not be NULL");

    const sireflect_field_info_t *info = sireflect_field_info(reg, type, field);
    if (info == NULL) {
        return -1;
    }

    memcpy(sireflect_field_mut_ptr(reg, type, obj, field), value, info->size);
    return 0;
}

#ifndef SIREFLECT_PARSER_H
#define SIREFLECT_PARSER_H

bool sireflect_parse_struct_fields(
    sireflect_registry_t *reg,
    const char *struct_name,
    const char *fields_src,
    sireflect_field_info_t **out_fields,
    size_t *out_field_count,
    size_t struct_size,
    size_t struct_align,
    bool fail_fast
);

#endif

#ifndef SIREFLECT_REGISTRY_H
#define SIREFLECT_REGISTRY_H

struct sireflect_registry_t {
    sireflect_type_info_t *types;
    size_t type_count;
    size_t type_cap;
};

sireflect_handle_t sireflect_registry_add_type(
    sireflect_registry_t *reg,
    const char *name,
    sireflect_kind_t kind,
    size_t size,
    size_t align,
    sireflect_field_info_t *fields,
    size_t field_count
);

sireflect_handle_t sireflect_registry_get_or_add_array_type(
    sireflect_registry_t *reg,
    sireflect_handle_t element_type,
    size_t element_count
);

sireflect_handle_t
sireflect_registry_get_or_add_pointer_type(sireflect_registry_t *reg, sireflect_handle_t pointee_type);

sireflect_type_info_t *
sireflect_registry_type_at(sireflect_registry_t *reg, sireflect_handle_t handle);

const sireflect_type_info_t *
sireflect_registry_const_type_at(const sireflect_registry_t *reg, sireflect_handle_t handle);

#endif

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#define SIREFLECT_MAX_ARRAY_DIMS 16

typedef enum {
    sireflect_token_ident,
    sireflect_token_integer,
    sireflect_token_lbrace,
    sireflect_token_rbrace,
    sireflect_token_lbracket,
    sireflect_token_rbracket,
    sireflect_token_star,
    sireflect_token_comma,
    sireflect_token_semicolon,
    sireflect_token_unknown,
    sireflect_token_end
} sireflect_token_kind_t;

typedef struct {
    sireflect_token_kind_t kind;
    const char *start;
    size_t len;
    size_t offset;
    size_t line;
    size_t column;
} sireflect_token_t;

typedef struct {
    const char *src;
    const char *struct_name;
    const char *field_start;
    size_t field_len;
    size_t pos;
    size_t line;
    size_t column;
    sireflect_token_t current;
    char message[512];
    bool failed;
    bool fail_fast;
} sireflect_parser_t;

typedef struct {
    const char *start;
    size_t len;
    char name[64];
    int has_name;
    size_t line;
    size_t column;
} sireflect_type_spec_t;

static inline int sireflect_is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }

static inline int sireflect_is_ident_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

static inline int sireflect_token_is_ident(sireflect_token_t token, const char *text) {
    return token.kind == sireflect_token_ident && strlen(text) == token.len &&
           strncmp(token.start, text, token.len) == 0;
}

static inline int sireflect_token_is_qualifier(sireflect_token_t token) {
    return sireflect_token_is_ident(token, "const") || sireflect_token_is_ident(token, "volatile");
}

static inline void
sireflect_type_spec_set(sireflect_type_spec_t *type, sireflect_token_t token) {
    type->start = token.start;
    type->len = token.len;
    type->name[0] = '\0';
    type->has_name = 0;
    type->line = token.line;
    type->column = token.column;
}

static inline void sireflect_type_spec_set2(
    sireflect_type_spec_t *type,
    sireflect_token_t first,
    sireflect_token_t second
) {
    const int len = snprintf(
        type->name,
        sizeof(type->name),
        "%.*s %.*s",
        (int)first.len,
        first.start,
        (int)second.len,
        second.start
    );
    (void)len;
    sireflect_indebug(
        sireflect_assert(len > 0 && (size_t)len < sizeof(type->name), "type specifier is too long");
    )
    type->start = NULL;
    type->len = 0;
    type->has_name = 1;
    type->line = first.line;
    type->column = first.column;
}

static inline void sireflect_type_spec_set3(
    sireflect_type_spec_t *type,
    sireflect_token_t first,
    sireflect_token_t second,
    sireflect_token_t third
) {
    const int len = snprintf(
        type->name,
        sizeof(type->name),
        "%.*s %.*s %.*s",
        (int)first.len,
        first.start,
        (int)second.len,
        second.start,
        (int)third.len,
        third.start
    );
    (void)len;
    sireflect_indebug(
        sireflect_assert(len > 0 && (size_t)len < sizeof(type->name), "type specifier is too long");
    );
    type->start = NULL;
    type->len = 0;
    type->has_name = 1;
    type->line = first.line;
    type->column = first.column;
}

static inline const char *sireflect_token_kind_name(sireflect_token_kind_t kind) {
    switch (kind) {
    case sireflect_token_ident:
        return "identifier";
    case sireflect_token_integer:
        return "integer";
    case sireflect_token_lbrace:
        return "'{'";
    case sireflect_token_rbrace:
        return "'}'";
    case sireflect_token_lbracket:
        return "'['";
    case sireflect_token_rbracket:
        return "']'";
    case sireflect_token_star:
        return "'*'";
    case sireflect_token_comma:
        return "','";
    case sireflect_token_semicolon:
        return "';'";
    case sireflect_token_unknown:
        return "unsupported token";
    case sireflect_token_end:
        return "end of input";
    }

    return "unknown token";
}

static inline void
sireflect_token_display(sireflect_token_t token, char *buffer, size_t buffer_size) {
    if (token.kind == sireflect_token_end) {
        snprintf(buffer, buffer_size, "end of input");
        return;
    }

    if (token.len == 0) {
        snprintf(buffer, buffer_size, "%s", sireflect_token_kind_name(token.kind));
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "%s '%.*s'",
        sireflect_token_kind_name(token.kind),
        (int)token.len,
        token.start
    );
}

static inline void
sireflect_parser_context(sireflect_parser_t *parser, char *buffer, size_t buffer_size) {
    if (parser->field_start != NULL) {
        snprintf(
            buffer,
            buffer_size,
            "struct '%s', field '%.*s'",
            parser->struct_name != NULL ? parser->struct_name : "<unknown>",
            (int)parser->field_len,
            parser->field_start
        );
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "struct '%s'",
        parser->struct_name != NULL ? parser->struct_name : "<unknown>"
    );
}

static inline void
sireflect_parser_fail_at(sireflect_parser_t *parser, sireflect_token_t token, const char *message) {
    char actual[96];
    char context[160];

    sireflect_token_display(token, actual, sizeof(actual));
    sireflect_parser_context(parser, context, sizeof(context));

    snprintf(
        parser->message,
        sizeof(parser->message),
        "%s in %s at line %zu, column %zu: actual %s",
        message,
        context,
        token.line,
        token.column,
        actual
    );

    parser->failed = true;
    if (parser->fail_fast) {
        sireflect_assert(false, parser->message);
    }
    sireflect_error_set(parser->message);
}

static inline void sireflect_parser_unexpected(
    sireflect_parser_t *parser,
    sireflect_token_kind_t expected,
    const char *context
) {
    char actual[96];
    char parser_context[160];

    sireflect_token_display(parser->current, actual, sizeof(actual));
    sireflect_parser_context(parser, parser_context, sizeof(parser_context));

    snprintf(
        parser->message,
        sizeof(parser->message),
        "unexpected token while parsing %s in %s at line %zu, column %zu: expected %s, actual %s",
        context,
        parser_context,
        parser->current.line,
        parser->current.column,
        sireflect_token_kind_name(expected),
        actual
    );

    parser->failed = true;
    if (parser->fail_fast) {
        sireflect_assert(false, parser->message);
    }
    sireflect_error_set(parser->message);
}

static inline void sireflect_parser_advance(sireflect_parser_t *parser) {
    if (parser->src[parser->pos] == '\n') {
        parser->line++;
        parser->column = 1;
    } else {
        parser->column++;
    }

    parser->pos++;
}

static inline void sireflect_parser_next(sireflect_parser_t *parser) {
    const char *src = parser->src;

    while (isspace((unsigned char)src[parser->pos])) {
        sireflect_parser_advance(parser);
    }

    const size_t start = parser->pos;
    const size_t line = parser->line;
    const size_t column = parser->column;
    const char c = src[start];

    if (c == '\0') {
        parser->current = (sireflect_token_t){ sireflect_token_end, &src[start], 0, start, line, column };
        return;
    }

    if (sireflect_is_ident_start(c)) {
        sireflect_parser_advance(parser);
        while (sireflect_is_ident_char(src[parser->pos])) {
            sireflect_parser_advance(parser);
        }

        parser->current = (sireflect_token_t){
            sireflect_token_ident,
            &src[start],
            parser->pos - start,
            start,
            line,
            column,
        };
        return;
    }

    if (isdigit((unsigned char)c)) {
        sireflect_parser_advance(parser);
        while (isdigit((unsigned char)src[parser->pos])) {
            sireflect_parser_advance(parser);
        }

        parser->current = (sireflect_token_t){
            sireflect_token_integer,
            &src[start],
            parser->pos - start,
            start,
            line,
            column,
        };
        return;
    }

    sireflect_parser_advance(parser);

    switch (c) {
    case '{':
        parser->current = (sireflect_token_t){ sireflect_token_lbrace, &src[start], 1, start, line, column };
        return;
    case '}':
        parser->current = (sireflect_token_t){ sireflect_token_rbrace, &src[start], 1, start, line, column };
        return;
    case '[':
        parser->current =
            (sireflect_token_t){ sireflect_token_lbracket, &src[start], 1, start, line, column };
        return;
    case ']':
        parser->current =
            (sireflect_token_t){ sireflect_token_rbracket, &src[start], 1, start, line, column };
        return;
    case '*':
        parser->current = (sireflect_token_t){ sireflect_token_star, &src[start], 1, start, line, column };
        return;
    case ',':
        parser->current = (sireflect_token_t){ sireflect_token_comma, &src[start], 1, start, line, column };
        return;
    case ';':
        parser->current =
            (sireflect_token_t){ sireflect_token_semicolon, &src[start], 1, start, line, column };
        return;
    default:
        parser->current = (sireflect_token_t){ sireflect_token_unknown, &src[start], 1, start, line, column };
        sireflect_parser_fail_at(
            parser,
            parser->current,
            "unsupported syntax in reflected struct; supported fields are '<type> <name>;', '<type> <name>, <name>;', '<type> *<name>;', '<type> <name>[N][M];', and '<type> *<name>[N];'"
        );
    }
}

static inline void sireflect_parser_init(
    sireflect_parser_t *parser,
    const char *struct_name,
    const char *src,
    bool fail_fast
) {
    sireflect_assert(parser != NULL, "parser must not be NULL");
    sireflect_assert(struct_name != NULL, "parser struct name must not be NULL");
    sireflect_assert(src != NULL, "parser source must not be NULL");

    parser->src = src;
    parser->struct_name = struct_name;
    parser->field_start = NULL;
    parser->field_len = 0;
    parser->pos = 0;
    parser->line = 1;
    parser->column = 1;
    parser->message[0] = '\0';
    parser->failed = false;
    parser->fail_fast = fail_fast;
    sireflect_parser_next(parser);
}

static inline sireflect_token_t
sireflect_expect(sireflect_parser_t *parser, sireflect_token_kind_t kind, const char *context) {
    sireflect_token_t token = parser->current;
    if (token.kind != kind) {
        sireflect_parser_unexpected(parser, kind, context);
        return token;
    }
    sireflect_parser_next(parser);
    return token;
}

static inline sireflect_token_t sireflect_expect_field_name(sireflect_parser_t *parser) {
    sireflect_token_t token = parser->current;
    if (token.kind != sireflect_token_ident || sireflect_token_is_qualifier(token)) {
        sireflect_parser_unexpected(parser, sireflect_token_ident, "field name");
        return token;
    }

    parser->field_start = token.start;
    parser->field_len = token.len;
    sireflect_parser_next(parser);
    return token;
}

static inline uint32_t sireflect_parse_qualifiers(sireflect_parser_t *parser) {
    uint32_t qualifiers = 0;

    for (;;) {
        if (sireflect_token_is_ident(parser->current, "const")) {
            qualifiers |= SIREFLECT_QUAL_CONST;
            sireflect_parser_next(parser);
            continue;
        }

        if (sireflect_token_is_ident(parser->current, "volatile")) {
            qualifiers |= SIREFLECT_QUAL_VOLATILE;
            sireflect_parser_next(parser);
            continue;
        }

        return qualifiers;
    }
}

static inline void
sireflect_fail_unsupported_type_specifier(sireflect_parser_t *parser, sireflect_token_t token) {
    sireflect_parser_fail_at(
        parser,
        token,
        "unsupported type specifier sequence; supported multi-token types are 'signed char', 'unsigned char', 'unsigned short', 'unsigned int', 'unsigned long', 'long long', and 'unsigned long long'"
    );
}

static inline sireflect_type_spec_t sireflect_parse_type_specifier(sireflect_parser_t *parser) {
    sireflect_token_t first = sireflect_expect(parser, sireflect_token_ident, "field type");
    sireflect_type_spec_t type;
    sireflect_type_spec_set(&type, first);
    if (parser->failed) {
        return type;
    }

    if (sireflect_token_is_ident(first, "signed")) {
        if (!sireflect_token_is_ident(parser->current, "char")) {
            sireflect_fail_unsupported_type_specifier(parser, parser->current);
            return type;
        }

        sireflect_token_t second = parser->current;
        sireflect_parser_next(parser);
        sireflect_type_spec_set2(&type, first, second);
        return type;
    }

    if (sireflect_token_is_ident(first, "long")) {
        if (!sireflect_token_is_ident(parser->current, "long")) {
            return type;
        }

        sireflect_token_t second = parser->current;
        sireflect_parser_next(parser);
        sireflect_type_spec_set2(&type, first, second);
        return type;
    }

    if (sireflect_token_is_ident(first, "unsigned")) {
        sireflect_token_t second = parser->current;

        if (sireflect_token_is_ident(second, "char") || sireflect_token_is_ident(second, "short") ||
            sireflect_token_is_ident(second, "int")) {
            sireflect_parser_next(parser);
            sireflect_type_spec_set2(&type, first, second);
            return type;
        }

        if (sireflect_token_is_ident(second, "long")) {
            sireflect_parser_next(parser);

            if (sireflect_token_is_ident(parser->current, "long")) {
                sireflect_token_t third = parser->current;
                sireflect_parser_next(parser);
                sireflect_type_spec_set3(&type, first, second, third);
                return type;
            }

            sireflect_type_spec_set2(&type, first, second);
            return type;
        }

        sireflect_fail_unsupported_type_specifier(parser, second);
        return type;
    }

    return type;
}

static inline char *sireflect_dup_range(const char *start, size_t len) {
    char *result = malloc(len + 1);
    sireflect_assert(result != NULL, "failed to allocate parser string");

    memcpy(result, start, len);
    result[len] = '\0';

    return result;
}

static inline size_t
sireflect_parse_array_count(sireflect_parser_t *parser, sireflect_token_t token) {
    size_t count = 0;

    for (size_t i = 0; i < token.len; i++) {
        const unsigned int digit = (unsigned int)(token.start[i] - '0');
        if (count > (SIZE_MAX - digit) / 10) {
            sireflect_parser_fail_at(parser, token, "array element count overflows size_t");
            return 0;
        }
        count = count * 10 + digit;
    }

    if (count == 0) {
        sireflect_parser_fail_at(parser, token, "array element count must be greater than zero");
        return 0;
    }

    return count;
}

static inline size_t
sireflect_parse_array_dimensions(sireflect_parser_t *parser, size_t *counts, size_t max_count) {
    size_t count = 0;

    while (parser->current.kind == sireflect_token_lbracket) {
        sireflect_parser_next(parser);

        if (parser->current.kind == sireflect_token_rbracket) {
            sireflect_parser_fail_at(parser, parser->current, "array element count is required");
            return count;
        }

        if (parser->current.kind != sireflect_token_integer) {
            sireflect_parser_fail_at(
                parser,
                parser->current,
                "array element count must be a positive integer literal"
            );
            return count;
        }

        sireflect_token_t count_token = parser->current;
        sireflect_parser_next(parser);

        if (parser->current.kind != sireflect_token_rbracket) {
            sireflect_parser_fail_at(parser, parser->current, "expected ']' after array element count");
            return count;
        }

        if (count >= max_count) {
            sireflect_parser_fail_at(parser, count_token, "too many array dimensions");
            return count;
        }

        counts[count++] = sireflect_parse_array_count(parser, count_token);
        if (parser->failed) {
            return count;
        }
        sireflect_parser_next(parser);
    }

    return count;
}

static inline void sireflect_parse_declarator_shape(sireflect_parser_t *parser) {
    size_t counts[SIREFLECT_MAX_ARRAY_DIMS];

    parser->field_start = NULL;
    parser->field_len = 0;

    if (parser->current.kind == sireflect_token_star) {
        sireflect_parser_next(parser);
    }

    sireflect_expect_field_name(parser);
    if (parser->failed) {
        return;
    }
    (void)sireflect_parse_array_dimensions(parser, counts, SIREFLECT_MAX_ARRAY_DIMS);
}

static inline size_t sireflect_parse_declaration_shape(sireflect_parser_t *parser) {
    size_t count = 0;

    (void)sireflect_parse_qualifiers(parser);
    (void)sireflect_parse_type_specifier(parser);
    if (parser->failed) {
        return 0;
    }

    for (;;) {
        sireflect_parse_declarator_shape(parser);
        if (parser->failed) {
            return 0;
        }
        count++;

        if (parser->current.kind != sireflect_token_comma) {
            break;
        }

        sireflect_parser_next(parser);
    }

    sireflect_expect(parser, sireflect_token_semicolon, "field terminator");
    if (parser->failed) {
        return 0;
    }
    return count;
}

static inline bool sireflect_count_fields(
    const char *struct_name,
    const char *fields_src,
    bool fail_fast,
    size_t *out_count
) {
    sireflect_parser_t parser;
    size_t count = 0;

    sireflect_parser_init(&parser, struct_name, fields_src, fail_fast);
    sireflect_expect(&parser, sireflect_token_lbrace, "struct field list start");
    if (parser.failed) {
        return false;
    }

    while (parser.current.kind != sireflect_token_rbrace) {
        count += sireflect_parse_declaration_shape(&parser);
        if (parser.failed) {
            return false;
        }
    }

    sireflect_expect(&parser, sireflect_token_rbrace, "struct field list end");
    if (parser.failed) {
        return false;
    }
    sireflect_expect(&parser, sireflect_token_end, "trailing input after struct field list");
    if (parser.failed) {
        return false;
    }

    *out_count = count;
    return true;
}

static inline size_t sireflect_align_up(size_t value, size_t align) {
    sireflect_assert(align != 0, "alignment must not be zero");

    const size_t remainder = value % align;
    if (remainder == 0) {
        return value;
    }

    return value + align - remainder;
}

static inline sireflect_handle_t sireflect_resolve_field_type(
    sireflect_registry_t *reg,
    sireflect_parser_t *parser,
    sireflect_type_spec_t type
) {
    char *owned_name = NULL;
    const char *type_name = type.name;

    if (!type.has_name) {
        owned_name = sireflect_dup_range(type.start, type.len);
        type_name = owned_name;
    }

    sireflect_handle_t field_type = sireflect_type_by_name(reg, type_name);
    if (field_type == SIREFLECT_INVALID_HANDLE) {
        char context[160];

        sireflect_parser_context(parser, context, sizeof(context));
        snprintf(
            parser->message,
            sizeof(parser->message),
            "unknown field type '%s' in %s at line %zu, column %zu; register the type before this struct or use a supported primitive alias",
            type_name,
            context,
            type.line,
            type.column
        );
        free(owned_name);
        parser->failed = true;
        if (parser->fail_fast) {
            sireflect_assert(false, parser->message);
        }
        sireflect_error_set(parser->message);
        return SIREFLECT_INVALID_HANDLE;
    }

    free(owned_name);
    return field_type;
}

static inline void sireflect_parse_declarator(
    sireflect_registry_t *reg,
    sireflect_parser_t *parser,
    sireflect_field_info_t *field,
    sireflect_type_spec_t type,
    uint32_t qualifiers,
    size_t *offset,
    size_t *max_align
) {
    parser->field_start = NULL;
    parser->field_len = 0;

    int is_pointer = 0;
    size_t array_counts[SIREFLECT_MAX_ARRAY_DIMS];
    size_t array_dim_count = 0;

    if (parser->current.kind == sireflect_token_star) {
        is_pointer = 1;
        sireflect_parser_next(parser);
    }

    sireflect_token_t name_token = sireflect_expect_field_name(parser);
    if (parser->failed) {
        return;
    }
    array_dim_count =
        sireflect_parse_array_dimensions(parser, array_counts, SIREFLECT_MAX_ARRAY_DIMS);
    if (parser->failed) {
        return;
    }

    sireflect_handle_t field_type = sireflect_resolve_field_type(reg, parser, type);
    if (parser->failed) {
        return;
    }

    if (is_pointer) {
        field_type = sireflect_registry_get_or_add_pointer_type(reg, field_type);
    }

    for (size_t i = array_dim_count; i > 0; i--) {
        field_type = sireflect_registry_get_or_add_array_type(reg, field_type, array_counts[i - 1]);
    }

    const sireflect_type_info_t *type_info = sireflect_type_info(reg, field_type);
    sireflect_assert(type_info != NULL, "field type metadata must exist");

    field->name = sireflect_dup_range(name_token.start, name_token.len);
    field->type = field_type;
    field->size = type_info->size;
    field->align = type_info->align;
    field->offset = sireflect_align_up(*offset, field->align);
    field->qualifiers = qualifiers;

    *offset = field->offset + field->size;
    if (field->align > *max_align) {
        *max_align = field->align;
    }
}

static inline size_t sireflect_parse_declaration(
    sireflect_registry_t *reg,
    sireflect_parser_t *parser,
    sireflect_field_info_t *fields,
    size_t *offset,
    size_t *max_align
) {
    size_t count = 0;
    uint32_t qualifiers = sireflect_parse_qualifiers(parser);
    sireflect_type_spec_t type = sireflect_parse_type_specifier(parser);
    if (parser->failed) {
        return 0;
    }

    for (;;) {
        sireflect_parse_declarator(
            reg,
            parser,
            &fields[count],
            type,
            qualifiers,
            offset,
            max_align
        );
        if (parser->failed) {
            return 0;
        }
        count++;

        if (parser->current.kind != sireflect_token_comma) {
            break;
        }

        sireflect_parser_next(parser);
    }

    sireflect_expect(parser, sireflect_token_semicolon, "field terminator");
    if (parser->failed) {
        return 0;
    }
    return count;
}

static inline void sireflect_free_parsed_fields(sireflect_field_info_t *fields, size_t field_count) {
    if (fields == NULL) {
        return;
    }

    for (size_t i = 0; i < field_count; i++) {
        free((char *)fields[i].name);
    }

    free(fields);
}

bool sireflect_parse_struct_fields(
    sireflect_registry_t *reg,
    const char *struct_name,
    const char *fields_src,
    sireflect_field_info_t **out_fields,
    size_t *out_field_count,
    size_t struct_size,
    size_t struct_align,
    bool fail_fast
) {
    (void)struct_size;
    (void)struct_align;

    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(struct_name != NULL, "struct name must not be NULL");
    sireflect_assert(fields_src != NULL, "field source must not be NULL");
    sireflect_assert(out_fields != NULL, "output field pointer must not be NULL");
    sireflect_assert(out_field_count != NULL, "output field count pointer must not be NULL");

    size_t field_count = 0;
    if (!sireflect_count_fields(struct_name, fields_src, fail_fast, &field_count)) {
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

    sireflect_field_info_t *fields = NULL;

    if (field_count != 0) {
        fields = calloc(field_count, sizeof(*fields));
        sireflect_assert(fields != NULL, "failed to allocate field metadata");
    }

    sireflect_parser_t parser;
    sireflect_parser_init(&parser, struct_name, fields_src, fail_fast);
    sireflect_expect(&parser, sireflect_token_lbrace, "struct field list start");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

    size_t offset = 0;
    size_t max_align = 1;

    for (size_t i = 0; i < field_count;) {
        const size_t parsed_count =
            sireflect_parse_declaration(reg, &parser, &fields[i], &offset, &max_align);
        if (parser.failed) {
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }
        i += parsed_count;
    }

    sireflect_expect(&parser, sireflect_token_rbrace, "struct field list end");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }
    sireflect_expect(&parser, sireflect_token_end, "trailing input after struct field list");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

#ifndef NDEBUG
    {
        const size_t computed_size = sireflect_align_up(offset, struct_align);
        if (computed_size != struct_size) {
            if (fail_fast) {
                sireflect_assert(false, "computed struct size does not match C layout");
            }
            sireflect_error_set("computed struct size does not match C layout");
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }

        if (max_align > struct_align) {
            if (fail_fast) {
                sireflect_assert(false, "computed field alignment exceeds struct alignment");
            }
            sireflect_error_set("computed field alignment exceeds struct alignment");
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }
    }
#endif

    *out_fields = fields;
    *out_field_count = field_count;
    return true;
}

static char *sireflect_dup_cstr(const char *str) {
    sireflect_assert(str != NULL, "string must not be NULL");

    const size_t len = strlen(str);
    char *result = malloc(len + 1);
    sireflect_assert(result != NULL, "failed to allocate string");

    memcpy(result, str, len + 1);
    return result;
}

static char *
sireflect_format_array_type_name(const sireflect_type_info_t *element, size_t element_count) {
    sireflect_assert(element != NULL, "array element metadata must exist");

    const char *suffix = strchr(element->name, '[');
    if (element->kind != sireflect_kind_array || suffix == NULL) {
        const int name_len = snprintf(NULL, 0, "%s[%zu]", element->name, element_count);
        sireflect_assert(name_len > 0, "failed to format array type name");

        char *name = malloc((size_t)name_len + 1);
        sireflect_assert(name != NULL, "failed to allocate array type name");
        snprintf(name, (size_t)name_len + 1, "%s[%zu]", element->name, element_count);
        return name;
    }

    const size_t prefix_len = (size_t)(suffix - element->name);
    const int count_len = snprintf(NULL, 0, "[%zu]", element_count);
    sireflect_assert(count_len > 0, "failed to format array dimension");

    const size_t suffix_len = strlen(suffix);
    char *name = malloc(prefix_len + (size_t)count_len + suffix_len + 1);
    sireflect_assert(name != NULL, "failed to allocate array type name");

    memcpy(name, element->name, prefix_len);
    snprintf(name + prefix_len, (size_t)count_len + 1, "[%zu]", element_count);
    memcpy(name + prefix_len + (size_t)count_len, suffix, suffix_len + 1);

    return name;
}

static sireflect_handle_t sireflect_handle_from_index(size_t index) {
    return (sireflect_handle_t)(index + 1);
}

static size_t sireflect_index_from_handle(sireflect_handle_t handle) {
    sireflect_assert(handle != SIREFLECT_INVALID_HANDLE, "type handle must be valid");
    return (size_t)(handle - 1);
}

static void sireflect_registry_reserve(sireflect_registry_t *reg, size_t min_cap) {
    sireflect_assert(reg != NULL, "registry must not be NULL");

    if (reg->type_cap >= min_cap) {
        return;
    }

    size_t new_cap = reg->type_cap == 0 ? 16 : reg->type_cap * 2;
    while (new_cap < min_cap) {
        new_cap *= 2;
    }

    sireflect_type_info_t *types = realloc(reg->types, new_cap * sizeof(*types));
    sireflect_assert(types != NULL, "failed to allocate type metadata");

    reg->types = types;
    reg->type_cap = new_cap;
}

sireflect_handle_t sireflect_registry_add_type(
    sireflect_registry_t *reg,
    const char *name,
    sireflect_kind_t kind,
    size_t size,
    size_t align,
    sireflect_field_info_t *fields,
    size_t field_count
) {
    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(name != NULL, "type name must not be NULL");
    sireflect_assert(size != 0 || kind == sireflect_kind_struct, "non-struct type size must not be zero");
    sireflect_assert(align != 0, "type alignment must not be zero");

    sireflect_registry_reserve(reg, reg->type_count + 1);

    const size_t index = reg->type_count++;
    reg->types[index] = (sireflect_type_info_t){
        .name = sireflect_dup_cstr(name),
        .kind = kind,
        .size = size,
        .align = align,
        .fields =
            {
                .fields = fields,
                .field_count = field_count,
            },
        .element_type = SIREFLECT_INVALID_HANDLE,
        .element_count = 0,
    };

    return sireflect_handle_from_index(index);
}

sireflect_handle_t
sireflect_registry_get_or_add_pointer_type(sireflect_registry_t *reg, sireflect_handle_t pointee_type) {
    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(pointee_type != SIREFLECT_INVALID_HANDLE, "pointer pointee type must be valid");

    for (size_t i = 0; i < reg->type_count; i++) {
        const sireflect_type_info_t *type = &reg->types[i];
        if (type->kind == sireflect_kind_pointer && type->element_type == pointee_type) {
            return sireflect_handle_from_index(i);
        }
    }

    const sireflect_type_info_t *pointee = sireflect_registry_const_type_at(reg, pointee_type);
    sireflect_assert(pointee != NULL, "pointer pointee metadata must exist");

    const int name_len = snprintf(NULL, 0, "%s*", pointee->name);
    sireflect_assert(name_len > 0, "failed to format pointer type name");

    char *name = malloc((size_t)name_len + 1);
    sireflect_assert(name != NULL, "failed to allocate pointer type name");
    snprintf(name, (size_t)name_len + 1, "%s*", pointee->name);

    sireflect_handle_t pointer_type = sireflect_registry_add_type(
        reg,
        name,
        sireflect_kind_pointer,
        sizeof(ptr),
        _Alignof(ptr),
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *pointer_info = sireflect_registry_type_at(reg, pointer_type);
    pointer_info->element_type = pointee_type;

    return pointer_type;
}

sireflect_handle_t sireflect_registry_get_or_add_array_type(
    sireflect_registry_t *reg,
    sireflect_handle_t element_type,
    size_t element_count
) {
    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(element_type != SIREFLECT_INVALID_HANDLE, "array element type must be valid");
    sireflect_assert(element_count != 0, "array element count must not be zero");

    for (size_t i = 0; i < reg->type_count; i++) {
        const sireflect_type_info_t *type = &reg->types[i];
        if (type->kind == sireflect_kind_array && type->element_type == element_type &&
            type->element_count == element_count) {
            return sireflect_handle_from_index(i);
        }
    }

    const sireflect_type_info_t *element = sireflect_registry_const_type_at(reg, element_type);
    sireflect_assert(element != NULL, "array element metadata must exist");
    sireflect_assert(element->size <= SIZE_MAX / element_count, "array type size overflows size_t");

    char *name = sireflect_format_array_type_name(element, element_count);

    sireflect_handle_t array_type = sireflect_registry_add_type(
        reg,
        name,
        sireflect_kind_array,
        element->size * element_count,
        element->align,
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *array_info = sireflect_registry_type_at(reg, array_type);
    array_info->element_type = element_type;
    array_info->element_count = element_count;

    return array_type;
}

#define add_type(name, kind)                                                                       \
    sireflect_registry_add_type(reg, #name, kind, sizeof(name), _Alignof(name), NULL, 0)

#define add_named_type(c_type, reflected_name, kind)                                               \
    sireflect_registry_add_type(reg, reflected_name, kind, sizeof(c_type), _Alignof(c_type), NULL, 0)

static inline void sireflect_register_builtin_types(sireflect_registry_t *reg) {
    add_type(u8, sireflect_kind_u8);
    add_type(u16, sireflect_kind_u16);
    add_type(u32, sireflect_kind_u32);
    add_type(u64, sireflect_kind_u64);
    add_type(i8, sireflect_kind_i8);
    add_type(i16, sireflect_kind_i16);
    add_type(i32, sireflect_kind_i32);
    add_type(i64, sireflect_kind_i64);
    add_type(f32, sireflect_kind_f32);
    add_type(f64, sireflect_kind_f64);
    add_type(bool, sireflect_kind_bool);
    add_type(char, sireflect_kind_char);
    add_type(ptr, sireflect_kind_ptr);

    add_type(uint8_t, sireflect_kind_u8);
    add_type(uint16_t, sireflect_kind_u16);
    add_type(uint32_t, sireflect_kind_u32);
    add_type(uint64_t, sireflect_kind_u64);
    add_type(int8_t, sireflect_kind_i8);
    add_type(int16_t, sireflect_kind_i16);
    add_type(int32_t, sireflect_kind_i32);
    add_type(int64_t, sireflect_kind_i64);

    add_type(float, sireflect_kind_f32);
    add_type(double, sireflect_kind_f64);
    add_type(short, sireflect_kind_short);
    add_type(int, sireflect_kind_int);
    add_type(long, sireflect_kind_long);

    add_named_type(signed char, "signed char", sireflect_kind_signed_char);
    add_named_type(unsigned char, "unsigned char", sireflect_kind_unsigned_char);
    add_named_type(unsigned short, "unsigned short", sireflect_kind_unsigned_short);
    add_named_type(unsigned int, "unsigned int", sireflect_kind_unsigned_int);
    add_named_type(unsigned long, "unsigned long", sireflect_kind_unsigned_long);
    add_named_type(long long, "long long", sireflect_kind_long_long);
    add_named_type(unsigned long long, "unsigned long long", sireflect_kind_unsigned_long_long);
}

sireflect_registry_t *sireflect_registry_init(void) {
    sireflect_error_clear();

    sireflect_registry_t *reg = calloc(1, sizeof(*reg));
    sireflect_assert(reg != NULL, "registry must not be NULL");

    sireflect_register_builtin_types(reg);
    return reg;
}

void sireflect_registry_fini(sireflect_registry_t *reg) {
    sireflect_error_clear();

    if (reg == NULL) {
        return;
    }

    for (size_t i = 0; i < reg->type_count; i++) {
        sireflect_type_info_t *type = &reg->types[i];

        free((char *)type->name);

        for (size_t f = 0; f < type->fields.field_count; f++) {
            free((char *)type->fields.fields[f].name);
        }

        free(type->fields.fields);
    }

    free(reg->types);
    free(reg);
}

sireflect_handle_t sireflect_type_by_name(const sireflect_registry_t *reg, const char *name) {
    sireflect_error_clear();

    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(name != NULL, "type name must not be NULL");

    for (size_t i = 0; i < reg->type_count; i++) {
        if (strcmp(reg->types[i].name, name) == 0) {
            return sireflect_handle_from_index(i);
        }
    }

    return SIREFLECT_INVALID_HANDLE;
}

const sireflect_type_info_t *
sireflect_registry_const_type_at(const sireflect_registry_t *reg, sireflect_handle_t handle) {
    sireflect_assert(reg != NULL, "registry must not be NULL");

    const size_t index = sireflect_index_from_handle(handle);
    sireflect_assert(index < reg->type_count, "type handle is out of range");

    return &reg->types[index];
}

sireflect_type_info_t *
sireflect_registry_type_at(sireflect_registry_t *reg, sireflect_handle_t handle) {
    return (sireflect_type_info_t *)sireflect_registry_const_type_at(reg, handle);
}

sireflect_handle_t
sireflect_try_register_struct(sireflect_registry_t *reg, const sireflect_struct_desc_t *desc) {
    sireflect_error_clear();

    if (reg == NULL || desc == NULL || desc->name == NULL || desc->fields == NULL ||
        desc->align == 0) {
        sireflect_error_set("invalid struct descriptor");
        return SIREFLECT_INVALID_HANDLE;
    }

    sireflect_handle_t existing = sireflect_type_by_name(reg, desc->name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        const sireflect_type_info_t *type = sireflect_type_info(reg, existing);
        if (type->kind != sireflect_kind_struct || type->size != desc->size ||
            type->align != desc->align) {
            sireflect_error_set("existing type is incompatible with struct descriptor");
            return SIREFLECT_INVALID_HANDLE;
        }
        return existing;
    }

    sireflect_field_info_t *parsed_fields = NULL;
    size_t field_count = 0;

    if (!sireflect_parse_struct_fields(
        reg,
        desc->name,
        desc->fields,
        &parsed_fields,
        &field_count,
        desc->size,
        desc->align,
        false
    )) {
        return SIREFLECT_INVALID_HANDLE;
    }

    return sireflect_registry_add_type(
        reg,
        desc->name,
        sireflect_kind_struct,
        desc->size,
        desc->align,
        parsed_fields,
        field_count
    );
}

sireflect_handle_t
sireflect_register_struct(sireflect_registry_t *reg, const sireflect_struct_desc_t *desc) {
    sireflect_error_clear();

    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(desc != NULL, "struct descriptor must not be NULL");
    sireflect_assert(desc->name != NULL, "struct descriptor name must not be NULL");
    sireflect_assert(desc->fields != NULL, "struct descriptor fields must not be NULL");
    sireflect_assert(desc->align != 0, "struct descriptor alignment must not be zero");

    sireflect_handle_t handle = SIREFLECT_INVALID_HANDLE;

    if (reg != NULL && desc != NULL && desc->name != NULL && desc->fields != NULL &&
        desc->align != 0) {
        sireflect_handle_t existing = sireflect_type_by_name(reg, desc->name);
        if (existing != SIREFLECT_INVALID_HANDLE) {
            const sireflect_type_info_t *type = sireflect_type_info(reg, existing);
            if (type->kind != sireflect_kind_struct || type->size != desc->size ||
                type->align != desc->align) {
                sireflect_assert(type->kind == sireflect_kind_struct, "existing type must be a struct");
                sireflect_assert(
                    type->size == desc->size,
                    "existing struct size must match descriptor"
                );
                sireflect_assert(
                    type->align == desc->align,
                    "existing struct alignment must match descriptor"
                );
                return SIREFLECT_INVALID_HANDLE;
            }
            return existing;
        }

        sireflect_field_info_t *parsed_fields = NULL;
        size_t field_count = 0;

        if (sireflect_parse_struct_fields(
                reg,
                desc->name,
                desc->fields,
                &parsed_fields,
                &field_count,
                desc->size,
                desc->align,
                true
            )) {
            handle = sireflect_registry_add_type(
                reg,
                desc->name,
                sireflect_kind_struct,
                desc->size,
                desc->align,
                parsed_fields,
                field_count
            );
        }
    }

    sireflect_assert(handle != SIREFLECT_INVALID_HANDLE, "failed to register struct");
    return handle;
}

const char *sireflect_kind_name(sireflect_kind_t kind) {
    sireflect_error_clear();

    switch (kind) {
    case sireflect_kind_u8:
        return "u8";
    case sireflect_kind_u16:
        return "u16";
    case sireflect_kind_u32:
        return "u32";
    case sireflect_kind_u64:
        return "u64";
    case sireflect_kind_i8:
        return "i8";
    case sireflect_kind_i16:
        return "i16";
    case sireflect_kind_i32:
        return "i32";
    case sireflect_kind_i64:
        return "i64";
    case sireflect_kind_f32:
        return "f32";
    case sireflect_kind_f64:
        return "f64";
    case sireflect_kind_bool:
        return "bool";
    case sireflect_kind_char:
        return "char";
    case sireflect_kind_short:
        return "short";
    case sireflect_kind_int:
        return "int";
    case sireflect_kind_long:
        return "long";
    case sireflect_kind_ptr:
        return "ptr";
    case sireflect_kind_pointer:
        return "pointer";
    case sireflect_kind_struct:
        return "struct";
    case sireflect_kind_array:
        return "array";
    case sireflect_kind_signed_char:
        return "signed char";
    case sireflect_kind_unsigned_char:
        return "unsigned char";
    case sireflect_kind_unsigned_short:
        return "unsigned short";
    case sireflect_kind_unsigned_int:
        return "unsigned int";
    case sireflect_kind_unsigned_long:
        return "unsigned long";
    case sireflect_kind_long_long:
        return "long long";
    case sireflect_kind_unsigned_long_long:
        return "unsigned long long";
    }

    return "unknown";
}

bool sireflect_is_numeric(sireflect_kind_t kind) {
    sireflect_error_clear();

    switch (kind) {
    case sireflect_kind_u8:
    case sireflect_kind_u16:
    case sireflect_kind_u32:
    case sireflect_kind_u64:
    case sireflect_kind_i8:
    case sireflect_kind_i16:
    case sireflect_kind_i32:
    case sireflect_kind_i64:
    case sireflect_kind_f32:
    case sireflect_kind_f64:
    case sireflect_kind_char:
    case sireflect_kind_short:
    case sireflect_kind_int:
    case sireflect_kind_long:
    case sireflect_kind_signed_char:
    case sireflect_kind_unsigned_char:
    case sireflect_kind_unsigned_short:
    case sireflect_kind_unsigned_int:
    case sireflect_kind_unsigned_long:
    case sireflect_kind_long_long:
    case sireflect_kind_unsigned_long_long:
        return true;
    case sireflect_kind_bool:
    case sireflect_kind_ptr:
    case sireflect_kind_pointer:
    case sireflect_kind_struct:
    case sireflect_kind_array:
        return false;
    }

    return false;
}

const sireflect_type_info_t *
sireflect_type_info(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    sireflect_error_clear();

    return sireflect_registry_const_type_at(reg, ref);
}

const sireflect_fields_t *
sireflect_type_fields(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(reg, ref);
    return &type->fields;
}

size_t sireflect_type_size(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    sireflect_error_clear();

    return sireflect_type_info(reg, ref)->size;
}

const char *sireflect_type_name(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    sireflect_error_clear();

    return sireflect_type_info(reg, ref)->name;
}

bool sireflect_type_is_struct(const sireflect_type_info_t *info) {
    sireflect_error_clear();

    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_struct;
}

bool sireflect_type_is_array(const sireflect_type_info_t *info) {
    sireflect_error_clear();

    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_array;
}

bool sireflect_type_is_pointer(const sireflect_type_info_t *info) {
    sireflect_error_clear();

    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_pointer;
}

sireflect_handle_t
sireflect_type_element(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(reg, ref);
    sireflect_assert(type->kind == sireflect_kind_array, "type must be an array");
    return type->element_type;
}

size_t
sireflect_type_element_count(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(reg, ref);
    sireflect_assert(type->kind == sireflect_kind_array, "type must be an array");
    return type->element_count;
}

sireflect_handle_t
sireflect_type_pointee(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(reg, ref);
    sireflect_assert(type->kind == sireflect_kind_pointer, "type must be a typed pointer");
    return type->element_type;
}

#ifndef SIJSON_INTERNAL_H
#define SIJSON_INTERNAL_H


#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define SIJSON_INTERNAL_API __attribute__((visibility("hidden")))
#else
#define SIJSON_INTERNAL_API
#endif

typedef struct sijson_member {
    char *key;
    sijson_value_t value;
} sijson_member_t;

typedef struct sijson_array {
    sijson_value_t *items;
    size_t len;
    size_t cap;
} sijson_array_t;

typedef struct sijson_object {
    sijson_member_t *items;
    size_t len;
    size_t cap;
} sijson_object_t;

struct sijson_value {
    sijson_type_t type;
    union {
        bool boolean;
        double number;
        char *string;
        sijson_array_t array;
        sijson_object_t object;
    } as;
};

typedef struct sijson_writer {
    char *data;
    size_t len;
    size_t cap;
} sijson_writer_t;

typedef struct sijson_parser {
    const char *cur;
} sijson_parser_t;

SIJSON_INTERNAL_API void sijson_clear_error(void);
SIJSON_INTERNAL_API bool sijson_set_error(const char *message);
SIJSON_INTERNAL_API bool sijson_set_error_at(const char *message, const char *at);

SIJSON_INTERNAL_API char *sijson_dup_range(const char *start, size_t len);
SIJSON_INTERNAL_API char *sijson_dup_cstr(const char *str);

SIJSON_INTERNAL_API void *sijson_arena_alloc(size_t size, size_t align);
SIJSON_INTERNAL_API char *sijson_arena_dup_range(const char *start, size_t len);
SIJSON_INTERNAL_API char *sijson_arena_dup_cstr(const char *str);
SIJSON_INTERNAL_API size_t sijson_arena_mark(void);
SIJSON_INTERNAL_API void sijson_arena_rewind(size_t mark);

SIJSON_INTERNAL_API bool sijson_reserve_array(sijson_array_t *array, size_t need);
SIJSON_INTERNAL_API bool sijson_reserve_object(sijson_object_t *object, size_t need);
SIJSON_INTERNAL_API sijson_value_t sijson_new_value(sijson_type_t type);

SIJSON_INTERNAL_API bool sijson_writer_reserve(sijson_writer_t *writer, size_t extra);
SIJSON_INTERNAL_API bool sijson_writer_putc(sijson_writer_t *writer, char c);
SIJSON_INTERNAL_API bool sijson_writer_write(sijson_writer_t *writer, const char *data, size_t len);
SIJSON_INTERNAL_API bool sijson_writer_cstr(sijson_writer_t *writer, const char *str);
SIJSON_INTERNAL_API bool sijson_writer_string(sijson_writer_t *writer, const char *str);
SIJSON_INTERNAL_API bool sijson_write_value(sijson_writer_t *writer, sijson_value_t value);

#endif

static char g_error[256];

void sijson_clear_error(void) { g_error[0] = '\0'; }

bool sijson_set_error(const char *message) {
    if (message == NULL) {
        message = "unknown sijson error";
    }

    snprintf(g_error, sizeof(g_error), "%s", message);
    return false;
}

bool sijson_set_error_at(const char *message, const char *at) {
    if (at == NULL) {
        return sijson_set_error(message);
    }

    snprintf(g_error, sizeof(g_error), "%s near '%.24s'", message, at);
    return false;
}

const char *sijson_error(void) { return g_error[0] != '\0' ? g_error : NULL; }

static void sijson_skip_ws(sijson_parser_t *parser) {
    while (isspace((unsigned char)*parser->cur)) {
        parser->cur++;
    }
}

static bool sijson_take(sijson_parser_t *parser, char c) {
    sijson_skip_ws(parser);
    if (*parser->cur != c) {
        return false;
    }
    parser->cur++;
    return true;
}

static int sijson_hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + c - 'a';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + c - 'A';
    }
    return -1;
}

static bool sijson_writer_utf8(sijson_writer_t *writer, unsigned codepoint) {
    char out[4];
    size_t len = 0;

    if (codepoint <= 0x7f) {
        out[len++] = (char)codepoint;
    } else if (codepoint <= 0x7ff) {
        out[len++] = (char)(0xc0 | (codepoint >> 6));
        out[len++] = (char)(0x80 | (codepoint & 0x3f));
    } else {
        out[len++] = (char)(0xe0 | (codepoint >> 12));
        out[len++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        out[len++] = (char)(0x80 | (codepoint & 0x3f));
    }

    return sijson_writer_write(writer, out, len);
}

static char *sijson_parse_string_raw(sijson_parser_t *parser) {
    sijson_skip_ws(parser);
    if (*parser->cur != '"') {
        sijson_set_error_at("expected JSON string", parser->cur);
        return NULL;
    }
    parser->cur++;

    sijson_writer_t writer = { 0 };
    const char *chunk = parser->cur;
    while (*parser->cur != '\0') {
        unsigned char c = (unsigned char)*parser->cur;
        if (c == '"') {
            if (!sijson_writer_write(&writer, chunk, (size_t)(parser->cur - chunk))) {
                free(writer.data);
                return NULL;
            }
            parser->cur++;
            if (writer.data == NULL) {
                return sijson_arena_dup_cstr("");
            }
            char *result = sijson_arena_dup_cstr(writer.data);
            free(writer.data);
            return result;
        }

        if (c < 0x20) {
            free(writer.data);
            sijson_set_error_at("control character in JSON string", parser->cur);
            return NULL;
        }

        if (c != '\\') {
            parser->cur++;
            continue;
        }

        if (!sijson_writer_write(&writer, chunk, (size_t)(parser->cur - chunk))) {
            free(writer.data);
            return NULL;
        }

        parser->cur++;
        char escaped = *parser->cur++;
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
            if (!sijson_writer_putc(&writer, escaped)) {
                free(writer.data);
                return NULL;
            }
            break;
        case 'b':
            if (!sijson_writer_putc(&writer, '\b')) {
                free(writer.data);
                return NULL;
            }
            break;
        case 'f':
            if (!sijson_writer_putc(&writer, '\f')) {
                free(writer.data);
                return NULL;
            }
            break;
        case 'n':
            if (!sijson_writer_putc(&writer, '\n')) {
                free(writer.data);
                return NULL;
            }
            break;
        case 'r':
            if (!sijson_writer_putc(&writer, '\r')) {
                free(writer.data);
                return NULL;
            }
            break;
        case 't':
            if (!sijson_writer_putc(&writer, '\t')) {
                free(writer.data);
                return NULL;
            }
            break;
        case 'u': {
            unsigned codepoint = 0;
            for (size_t i = 0; i < 4; i++) {
                int digit = sijson_hex_digit(parser->cur[i]);
                if (digit < 0) {
                    free(writer.data);
                    sijson_set_error_at("invalid unicode escape", parser->cur);
                    return NULL;
                }
                codepoint = (codepoint << 4) | (unsigned)digit;
            }
            parser->cur += 4;
            if (!sijson_writer_utf8(&writer, codepoint)) {
                free(writer.data);
                return NULL;
            }
            break;
        }
        default:
            free(writer.data);
            sijson_set_error_at("invalid JSON string escape", parser->cur - 1);
            return NULL;
        }
        chunk = parser->cur;
    }

    free(writer.data);
    sijson_set_error("unterminated JSON string");
    return NULL;
}

static sijson_value_t sijson_parse_value(sijson_parser_t *parser);

static sijson_value_t sijson_parse_array_value(sijson_parser_t *parser) {
    if (!sijson_take(parser, '[')) {
        return NULL;
    }

    sijson_value_t array = sijson_new_value(SIJSON_ARRAY);
    if (array == NULL) {
        return NULL;
    }

    sijson_skip_ws(parser);
    if (*parser->cur == ']') {
        parser->cur++;
        return array;
    }

    for (;;) {
        sijson_value_t item = sijson_parse_value(parser);
        if (item == NULL) {
            return NULL;
        }
        if (!sijson_reserve_array(&array->as.array, array->as.array.len + 1)) {
            return NULL;
        }
        array->as.array.items[array->as.array.len++] = item;

        sijson_skip_ws(parser);
        if (*parser->cur == ']') {
            parser->cur++;
            return array;
        }
        if (*parser->cur != ',') {
            sijson_set_error_at("expected ',' or ']'", parser->cur);
            return NULL;
        }
        parser->cur++;
    }
}

static sijson_value_t sijson_parse_object_value(sijson_parser_t *parser) {
    if (!sijson_take(parser, '{')) {
        return NULL;
    }

    sijson_value_t object = sijson_new_value(SIJSON_OBJECT);
    if (object == NULL) {
        return NULL;
    }

    sijson_skip_ws(parser);
    if (*parser->cur == '}') {
        parser->cur++;
        return object;
    }

    for (;;) {
        char *key = sijson_parse_string_raw(parser);
        if (key == NULL) {
            return NULL;
        }
        if (!sijson_take(parser, ':')) {
            sijson_set_error_at("expected ':'", parser->cur);
            return NULL;
        }

        sijson_value_t item = sijson_parse_value(parser);
        if (item == NULL) {
            return NULL;
        }

        if (!sijson_reserve_object(&object->as.object, object->as.object.len + 1)) {
            return NULL;
        }
        object->as.object.items[object->as.object.len++] = (sijson_member_t){
            .key = key,
            .value = item,
        };

        sijson_skip_ws(parser);
        if (*parser->cur == '}') {
            parser->cur++;
            return object;
        }
        if (*parser->cur != ',') {
            sijson_set_error_at("expected ',' or '}'", parser->cur);
            return NULL;
        }
        parser->cur++;
    }
}

static sijson_value_t sijson_parse_number_value(sijson_parser_t *parser) {
    sijson_skip_ws(parser);
    const char *start = parser->cur;
    const char *scan = start;

    if (*scan == '-') {
        scan++;
    }

    if (*scan == '0') {
        scan++;
    } else if (*scan >= '1' && *scan <= '9') {
        do {
            scan++;
        } while (isdigit((unsigned char)*scan));
    } else {
        sijson_set_error_at("invalid JSON number", start);
        return NULL;
    }

    if (*scan == '.') {
        scan++;
        if (!isdigit((unsigned char)*scan)) {
            sijson_set_error_at("invalid JSON number", start);
            return NULL;
        }
        do {
            scan++;
        } while (isdigit((unsigned char)*scan));
    }

    if (*scan == 'e' || *scan == 'E') {
        scan++;
        if (*scan == '+' || *scan == '-') {
            scan++;
        }
        if (!isdigit((unsigned char)*scan)) {
            sijson_set_error_at("invalid JSON number", start);
            return NULL;
        }
        do {
            scan++;
        } while (isdigit((unsigned char)*scan));
    }

    size_t len = (size_t)(scan - start);
    char stack[128];
    char *number_text = stack;
    if (len >= sizeof(stack)) {
        number_text = malloc(len + 1);
        if (number_text == NULL) {
            sijson_set_error("out of memory");
            return NULL;
        }
    }
    memcpy(number_text, start, len);
    number_text[len] = '\0';

    errno = 0;
    char *end = NULL;
    double number = strtod(number_text, &end);
    if (end == number_text || *end != '\0' || errno == ERANGE || !isfinite(number)) {
        if (number_text != stack) {
            free(number_text);
        }
        sijson_set_error_at("invalid JSON number", start);
        return NULL;
    }
    if (number_text != stack) {
        free(number_text);
    }

    parser->cur = scan;
    sijson_value_t value = sijson_new_value(SIJSON_NUMBER);
    if (value != NULL) {
        value->as.number = number;
    }
    return value;
}

static sijson_value_t
sijson_parse_literal(sijson_parser_t *parser, const char *literal, sijson_value_t value) {
    size_t len = strlen(literal);
    if (strncmp(parser->cur, literal, len) != 0) {
        sijson_set_error_at("invalid JSON literal", parser->cur);
        return NULL;
    }
    parser->cur += len;
    return value;
}

static sijson_value_t sijson_parse_value(sijson_parser_t *parser) {
    sijson_skip_ws(parser);
    switch (*parser->cur) {
    case 'n':
        return sijson_parse_literal(parser, "null", sijson_new_value(SIJSON_NULL));
    case 't': {
        sijson_value_t value = sijson_new_value(SIJSON_BOOL);
        if (value != NULL) {
            value->as.boolean = true;
        }
        return sijson_parse_literal(parser, "true", value);
    }
    case 'f': {
        sijson_value_t value = sijson_new_value(SIJSON_BOOL);
        if (value != NULL) {
            value->as.boolean = false;
        }
        return sijson_parse_literal(parser, "false", value);
    }
    case '"': {
        char *string = sijson_parse_string_raw(parser);
        if (string == NULL) {
            return NULL;
        }
        sijson_value_t value = sijson_new_value(SIJSON_STRING);
        if (value == NULL) {
            return NULL;
        }
        value->as.string = string;
        return value;
    }
    case '[':
        return sijson_parse_array_value(parser);
    case '{':
        return sijson_parse_object_value(parser);
    default:
        if (*parser->cur == '-' || isdigit((unsigned char)*parser->cur)) {
            return sijson_parse_number_value(parser);
        }
        sijson_set_error_at("expected JSON value", parser->cur);
        return NULL;
    }
}

sijson_value_t sijson_parse(const char *json) {
    sijson_clear_error();
    if (json == NULL) {
        sijson_set_error("sijson_parse expects JSON text");
        return NULL;
    }

    size_t mark = sijson_arena_mark();
    sijson_parser_t parser = { .cur = json };
    sijson_value_t value = sijson_parse_value(&parser);
    if (value == NULL) {
        sijson_arena_rewind(mark);
        return NULL;
    }

    sijson_skip_ws(&parser);
    if (*parser.cur != '\0') {
        sijson_set_error_at("trailing characters after JSON value", parser.cur);
        sijson_arena_rewind(mark);
        return NULL;
    }

    return value;
}

#include <limits.h>

static sireflect_registry_t *g_registry;
static void *g_from_json_buffer;
static size_t g_from_json_capacity;

sireflect_registry_t *sijson_default_registry(void) {
    if (g_registry == NULL) {
        g_registry = sireflect_registry_init();
        if (g_registry == NULL) {
            sijson_set_error("failed to initialize sireflect registry");
            return NULL;
        }

        static const sireflect_struct_desc_t value_desc = {
            .name = "sijson_value_t",
            .fields = "{ ptr value; }",
            .size = sizeof(sijson_value_t),
            .align = _Alignof(sijson_value_t),
        };
        sireflect_register_struct(g_registry, &value_desc);
    }

    return g_registry;
}

static sireflect_handle_t
sijson_register_type(sireflect_handle_t *ref, const sireflect_struct_desc_t *desc) {
    if (ref == NULL || desc == NULL) {
        sijson_set_error("missing reflection descriptor");
        return SIREFLECT_INVALID_HANDLE;
    }
    if (*ref != SIREFLECT_INVALID_HANDLE) {
        return *ref;
    }

    sireflect_registry_t *reg = sijson_default_registry();
    if (reg == NULL) {
        return SIREFLECT_INVALID_HANDLE;
    }

    *ref = sireflect_register_struct(reg, desc);
    return *ref;
}

static bool sijson_is_value_type(const sireflect_type_info_t *type) {
    return type != NULL && strcmp(type->name, "sijson_value_t") == 0 &&
           type->size == sizeof(sijson_value_t);
}

static bool sijson_is_char_pointer_type(const sireflect_type_info_t *type) {
    if (type == NULL) {
        return false;
    }
    if (type->kind == sireflect_kind_ptr) {
        return true;
    }
    if (type->kind != sireflect_kind_pointer) {
        return false;
    }

    const sireflect_type_info_t *pointee = sireflect_type_info(g_registry, type->element_type);
    return pointee != NULL && pointee->kind == sireflect_kind_char;
}

static bool
sijson_write_reflected(sijson_writer_t *writer, sireflect_handle_t type, const void *ptr);

static bool sijson_write_reflected_field(
    sijson_writer_t *writer,
    const sireflect_type_info_t *field_type,
    const void *field_ptr
);

static bool sijson_write_reflected_array(
    sijson_writer_t *writer,
    const sireflect_type_info_t *array_type,
    const void *array_ptr
) {
    if (array_type == NULL || array_type->kind != sireflect_kind_array) {
        return sijson_set_error("expected reflected array type");
    }

    const sireflect_type_info_t *element_type =
        sireflect_type_info(g_registry, array_type->element_type);
    if (element_type == NULL || element_type->size == 0) {
        return sijson_set_error("missing reflected array element type");
    }

    if (!sijson_writer_putc(writer, '[')) {
        return false;
    }

    for (size_t i = 0; i < array_type->element_count; i++) {
        const void *element_ptr = (const unsigned char *)array_ptr + i * element_type->size;

        if (i != 0 && !sijson_writer_putc(writer, ',')) {
            return false;
        }
        if (!sijson_write_reflected_field(writer, element_type, element_ptr)) {
            return false;
        }
    }

    return sijson_writer_putc(writer, ']');
}

static bool sijson_write_reflected_field(
    sijson_writer_t *writer,
    const sireflect_type_info_t *field_type,
    const void *field_ptr
) {
    if (field_type == NULL) {
        return sijson_set_error("missing reflected field type");
    }

    switch (field_type->kind) {
    case sireflect_kind_bool:
        return sijson_writer_cstr(writer, *(const bool *)field_ptr ? "true" : "false");
    default:
        break;
    }

    char number[64];
    switch (field_type->kind) {
    case sireflect_kind_signed_char:
        snprintf(number, sizeof(number), "%d", (int)*(const signed char *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_unsigned_char:
        snprintf(number, sizeof(number), "%u", (unsigned)*(const unsigned char *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_u8:
        snprintf(number, sizeof(number), "%u", (unsigned)*(const u8 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_u16:
        snprintf(number, sizeof(number), "%u", (unsigned)*(const u16 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_unsigned_short:
        snprintf(number, sizeof(number), "%u", (unsigned)*(const unsigned short *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_u32:
        snprintf(number, sizeof(number), "%u", *(const u32 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_unsigned_int:
        snprintf(number, sizeof(number), "%u", *(const unsigned int *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_u64:
        snprintf(number, sizeof(number), "%llu", (unsigned long long)*(const u64 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_i8:
        snprintf(number, sizeof(number), "%d", (int)*(const i8 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_i16:
        snprintf(number, sizeof(number), "%d", (int)*(const i16 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_i32:
        snprintf(number, sizeof(number), "%d", *(const i32 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_i64:
        snprintf(number, sizeof(number), "%lld", (long long)*(const i64 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_short:
        snprintf(number, sizeof(number), "%d", (int)*(const short *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_int:
        snprintf(number, sizeof(number), "%d", *(const int *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_long:
        snprintf(number, sizeof(number), "%ld", *(const long *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_unsigned_long:
        snprintf(number, sizeof(number), "%lu", *(const unsigned long *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_long_long:
        snprintf(number, sizeof(number), "%lld", *(const long long *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_unsigned_long_long:
        snprintf(number, sizeof(number), "%llu", *(const unsigned long long *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_f32:
        snprintf(number, sizeof(number), "%.9g", (double)*(const f32 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_f64:
        snprintf(number, sizeof(number), "%.17g", *(const f64 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_char:
        return sijson_writer_string(writer, (char[2]){ *(const char *)field_ptr, '\0' });
    case sireflect_kind_ptr:
        return sijson_writer_string(writer, *(char *const *)field_ptr);
    case sireflect_kind_pointer:
        if (sijson_is_char_pointer_type(field_type)) {
            return sijson_writer_string(writer, *(char *const *)field_ptr);
        }
        break;
    case sireflect_kind_struct:
        if (sijson_is_value_type(field_type)) {
            return sijson_write_value(writer, *(const sijson_value_t *)field_ptr);
        }
        return sijson_write_reflected(
            writer,
            sireflect_type_by_name(g_registry, field_type->name),
            field_ptr
        );
    case sireflect_kind_array:
        return sijson_write_reflected_array(writer, field_type, field_ptr);
    case sireflect_kind_bool:
        break;
    }

    return sijson_set_error("unsupported field type for serialization");
}

static bool
sijson_write_reflected(sijson_writer_t *writer, sireflect_handle_t type, const void *ptr) {
    const sireflect_type_info_t *info = sireflect_type_info(g_registry, type);
    if (info == NULL || info->kind != sireflect_kind_struct) {
        return sijson_set_error("expected reflected struct");
    }

    if (!sijson_writer_putc(writer, '{')) {
        return false;
    }

    const sireflect_fields_t *fields = &info->fields;
    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        const sireflect_type_info_t *field_type = sireflect_type_info(g_registry, field->type);
        const void *field_ptr = (const unsigned char *)ptr + field->offset;

        if (i != 0 && !sijson_writer_putc(writer, ',')) {
            return false;
        }
        if (!sijson_writer_string(writer, field->name) || !sijson_writer_putc(writer, ':')) {
            return false;
        }
        if (!sijson_write_reflected_field(writer, field_type, field_ptr)) {
            return false;
        }
    }

    return sijson_writer_putc(writer, '}');
}

char *
sijson_to_json_impl(sireflect_handle_t *ref, const sireflect_struct_desc_t *desc, const void *ptr) {
    sijson_clear_error();
    if (ptr == NULL) {
        sijson_set_error("sijson_to_json expects a value pointer");
        return NULL;
    }

    sireflect_handle_t type = sijson_register_type(ref, desc);
    if (type == SIREFLECT_INVALID_HANDLE) {
        return NULL;
    }

    sijson_writer_t writer = { 0 };
    if (!sijson_write_reflected(&writer, type, ptr)) {
        free(writer.data);
        return NULL;
    }
    return writer.data;
}

static bool sijson_number_is_integer(double value) {
    if (!isfinite(value) || value < -9007199254740991.0 || value > 9007199254740991.0) {
        return false;
    }

    int64_t integer = (int64_t)value;
    return (double)integer == value;
}

static bool sijson_assign_number(
    const sireflect_type_info_t *field_type,
    void *field_ptr,
    sijson_value_t value
) {
    if (value == NULL || value->type != SIJSON_NUMBER) {
        return sijson_set_error("expected JSON number");
    }

    double number = value->as.number;
    switch (field_type->kind) {
    case sireflect_kind_signed_char:
        if (!sijson_number_is_integer(number) || number < SCHAR_MIN || number > SCHAR_MAX) {
            return sijson_set_error("number out of range for signed char");
        }
        *(signed char *)field_ptr = (signed char)number;
        return true;
    case sireflect_kind_unsigned_char:
        if (!sijson_number_is_integer(number) || number < 0 || number > UCHAR_MAX) {
            return sijson_set_error("number out of range for unsigned char");
        }
        *(unsigned char *)field_ptr = (unsigned char)number;
        return true;
    case sireflect_kind_u8:
        if (!sijson_number_is_integer(number) || number < 0 || number > UINT8_MAX) {
            return sijson_set_error("number out of range for u8");
        }
        *(u8 *)field_ptr = (u8)number;
        return true;
    case sireflect_kind_u16:
        if (!sijson_number_is_integer(number) || number < 0 || number > UINT16_MAX) {
            return sijson_set_error("number out of range for u16");
        }
        *(u16 *)field_ptr = (u16)number;
        return true;
    case sireflect_kind_unsigned_short:
        if (!sijson_number_is_integer(number) || number < 0 || number > USHRT_MAX) {
            return sijson_set_error("number out of range for unsigned short");
        }
        *(unsigned short *)field_ptr = (unsigned short)number;
        return true;
    case sireflect_kind_u32:
        if (!sijson_number_is_integer(number) || number < 0 || number > UINT32_MAX) {
            return sijson_set_error("number out of range for u32");
        }
        *(u32 *)field_ptr = (u32)number;
        return true;
    case sireflect_kind_unsigned_int:
        if (!sijson_number_is_integer(number) || number < 0 || number > UINT_MAX) {
            return sijson_set_error("number out of range for unsigned int");
        }
        *(unsigned int *)field_ptr = (unsigned int)number;
        return true;
    case sireflect_kind_u64:
        if (!sijson_number_is_integer(number) || number < 0) {
            return sijson_set_error("number out of range for u64");
        }
        *(u64 *)field_ptr = (u64)number;
        return true;
    case sireflect_kind_i8:
        if (!sijson_number_is_integer(number) || number < INT8_MIN || number > INT8_MAX) {
            return sijson_set_error("number out of range for i8");
        }
        *(i8 *)field_ptr = (i8)number;
        return true;
    case sireflect_kind_i16:
        if (!sijson_number_is_integer(number) || number < INT16_MIN || number > INT16_MAX) {
            return sijson_set_error("number out of range for i16");
        }
        *(i16 *)field_ptr = (i16)number;
        return true;
    case sireflect_kind_i32:
        if (!sijson_number_is_integer(number) || number < INT32_MIN || number > INT32_MAX) {
            return sijson_set_error("number out of range for i32");
        }
        *(i32 *)field_ptr = (i32)number;
        return true;
    case sireflect_kind_i64:
        if (!sijson_number_is_integer(number)) {
            return sijson_set_error("number out of range for i64");
        }
        *(i64 *)field_ptr = (i64)number;
        return true;
    case sireflect_kind_short:
        if (!sijson_number_is_integer(number)) {
            return sijson_set_error("expected integer for short");
        }
        *(short *)field_ptr = (short)number;
        return true;
    case sireflect_kind_int:
        if (!sijson_number_is_integer(number)) {
            return sijson_set_error("expected integer for int");
        }
        *(int *)field_ptr = (int)number;
        return true;
    case sireflect_kind_long:
        if (!sijson_number_is_integer(number)) {
            return sijson_set_error("expected integer for long");
        }
        *(long *)field_ptr = (long)number;
        return true;
    case sireflect_kind_unsigned_long:
        if (!sijson_number_is_integer(number) || number < 0) {
            return sijson_set_error("number out of range for unsigned long");
        }
        *(unsigned long *)field_ptr = (unsigned long)number;
        return true;
    case sireflect_kind_long_long:
        if (!sijson_number_is_integer(number)) {
            return sijson_set_error("number out of range for long long");
        }
        *(long long *)field_ptr = (long long)number;
        return true;
    case sireflect_kind_unsigned_long_long:
        if (!sijson_number_is_integer(number) || number < 0) {
            return sijson_set_error("number out of range for unsigned long long");
        }
        *(unsigned long long *)field_ptr = (unsigned long long)number;
        return true;
    case sireflect_kind_f32:
        *(f32 *)field_ptr = (f32)number;
        return true;
    case sireflect_kind_f64:
        *(f64 *)field_ptr = number;
        return true;
    default:
        return sijson_set_error("field is not numeric");
    }
}

static bool sijson_assign_reflected(sireflect_handle_t type, void *ptr, sijson_value_t value);

static bool sijson_assign_field(
    const sireflect_type_info_t *field_type,
    void *field_ptr,
    sijson_value_t value
);

static bool sijson_assign_array(
    const sireflect_type_info_t *array_type,
    void *array_ptr,
    sijson_value_t value
) {
    if (array_type == NULL || array_type->kind != sireflect_kind_array) {
        return sijson_set_error("expected reflected array type");
    }
    if (value == NULL || value->type != SIJSON_ARRAY) {
        return sijson_set_error("expected JSON array");
    }
    if (value->as.array.len != array_type->element_count) {
        return sijson_set_error("JSON array length does not match reflected array");
    }

    const sireflect_type_info_t *element_type =
        sireflect_type_info(g_registry, array_type->element_type);
    if (element_type == NULL || element_type->size == 0) {
        return sijson_set_error("missing reflected array element type");
    }

    for (size_t i = 0; i < array_type->element_count; i++) {
        void *element_ptr = (unsigned char *)array_ptr + i * element_type->size;
        if (!sijson_assign_field(element_type, element_ptr, value->as.array.items[i])) {
            return false;
        }
    }

    return true;
}

static bool sijson_assign_field(
    const sireflect_type_info_t *field_type,
    void *field_ptr,
    sijson_value_t value
) {
    if (field_type == NULL) {
        return sijson_set_error("missing reflected field type");
    }

    switch (field_type->kind) {
    case sireflect_kind_bool:
        if (value == NULL || value->type != SIJSON_BOOL) {
            return sijson_set_error("expected JSON bool");
        }
        *(bool *)field_ptr = value->as.boolean;
        return true;
    case sireflect_kind_signed_char:
    case sireflect_kind_unsigned_char:
    case sireflect_kind_u8:
    case sireflect_kind_u16:
    case sireflect_kind_u32:
    case sireflect_kind_u64:
    case sireflect_kind_i8:
    case sireflect_kind_i16:
    case sireflect_kind_i32:
    case sireflect_kind_i64:
    case sireflect_kind_unsigned_short:
    case sireflect_kind_short:
    case sireflect_kind_unsigned_int:
    case sireflect_kind_int:
    case sireflect_kind_unsigned_long:
    case sireflect_kind_long:
    case sireflect_kind_long_long:
    case sireflect_kind_unsigned_long_long:
    case sireflect_kind_f32:
    case sireflect_kind_f64:
        return sijson_assign_number(field_type, field_ptr, value);
    case sireflect_kind_char:
        if (value == NULL || value->type != SIJSON_STRING || value->as.string[0] == '\0') {
            return sijson_set_error("expected non-empty JSON string for char");
        }
        *(char *)field_ptr = value->as.string[0];
        return true;
    case sireflect_kind_ptr:
    case sireflect_kind_pointer:
        if (!sijson_is_char_pointer_type(field_type)) {
            break;
        }
        if (value == NULL || value->type == SIJSON_NULL) {
            *(char **)field_ptr = NULL;
            return true;
        }
        if (value->type != SIJSON_STRING) {
            return sijson_set_error("expected JSON string for pointer field");
        }
        *(char **)field_ptr = sijson_dup_cstr(value->as.string);
        return *(char **)field_ptr != NULL;
    case sireflect_kind_struct:
        if (sijson_is_value_type(field_type)) {
            *(sijson_value_t *)field_ptr = value;
            return true;
        }
        return sijson_assign_reflected(
            sireflect_type_by_name(g_registry, field_type->name),
            field_ptr,
            value
        );
    case sireflect_kind_array:
        return sijson_assign_array(field_type, field_ptr, value);
    }

    return sijson_set_error("unsupported field type for deserialization");
}

static bool sijson_assign_reflected(sireflect_handle_t type, void *ptr, sijson_value_t value) {
    if (value == NULL || value->type != SIJSON_OBJECT) {
        return sijson_set_error("expected JSON object for reflected struct");
    }

    const sireflect_type_info_t *info = sireflect_type_info(g_registry, type);
    if (info == NULL || info->kind != sireflect_kind_struct) {
        return sijson_set_error("expected reflected struct type");
    }

    memset(ptr, 0, info->size);
    const sireflect_fields_t *fields = &info->fields;
    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        sijson_value_t member = sijson_object_get(value, field->name);
        if (member == NULL) {
            continue;
        }

        const sireflect_type_info_t *field_type = sireflect_type_info(g_registry, field->type);
        void *field_ptr = (unsigned char *)ptr + field->offset;
        if (!sijson_assign_field(field_type, field_ptr, member)) {
            return false;
        }
    }

    return true;
}

void *sijson_from_json_impl(
    sireflect_handle_t *ref,
    const sireflect_struct_desc_t *desc,
    const char *json
) {
    sijson_clear_error();
    sireflect_handle_t type = sijson_register_type(ref, desc);
    if (type == SIREFLECT_INVALID_HANDLE) {
        return NULL;
    }

    const sireflect_type_info_t *info = sireflect_type_info(g_registry, type);
    if (info == NULL) {
        sijson_set_error("missing reflected type info");
        return NULL;
    }

    if (g_from_json_capacity < info->size) {
        void *buffer = realloc(g_from_json_buffer, info->size);
        if (buffer == NULL) {
            sijson_set_error("out of memory");
            return NULL;
        }
        g_from_json_buffer = buffer;
        g_from_json_capacity = info->size;
    }

    memset(g_from_json_buffer, 0, info->size);
    sijson_value_t root = sijson_parse(json);
    if (root == NULL) {
        memset(g_from_json_buffer, 0, info->size);
        return g_from_json_buffer;
    }

    if (!sijson_assign_reflected(type, g_from_json_buffer, root)) {
        memset(g_from_json_buffer, 0, info->size);
        return g_from_json_buffer;
    }

    return g_from_json_buffer;
}

static void sijson_free_reflected_field(const sireflect_type_info_t *field_type, void *field_ptr);

static void sijson_free_reflected(sireflect_handle_t type, void *ptr) {
    if (ptr == NULL) {
        return;
    }

    const sireflect_type_info_t *info = sireflect_type_info(g_registry, type);
    if (info == NULL || info->kind != sireflect_kind_struct || sijson_is_value_type(info)) {
        return;
    }

    const sireflect_fields_t *fields = &info->fields;
    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        const sireflect_type_info_t *field_type = sireflect_type_info(g_registry, field->type);
        void *field_ptr = (unsigned char *)ptr + field->offset;

        sijson_free_reflected_field(field_type, field_ptr);
    }
}

static void sijson_free_reflected_field(const sireflect_type_info_t *field_type, void *field_ptr) {
    if (field_type == NULL || field_ptr == NULL) {
        return;
    }

    switch (field_type->kind) {
    case sireflect_kind_ptr:
        free(*(void **)field_ptr);
        *(void **)field_ptr = NULL;
        return;
    case sireflect_kind_pointer:
        if (sijson_is_char_pointer_type(field_type)) {
            free(*(void **)field_ptr);
            *(void **)field_ptr = NULL;
        }
        return;
    case sireflect_kind_struct:
        if (!sijson_is_value_type(field_type)) {
            sijson_free_reflected(sireflect_type_by_name(g_registry, field_type->name), field_ptr);
        }
        return;
    case sireflect_kind_array: {
        const sireflect_type_info_t *element_type =
            sireflect_type_info(g_registry, field_type->element_type);
        if (element_type == NULL || element_type->size == 0) {
            return;
        }
        for (size_t i = 0; i < field_type->element_count; i++) {
            void *element_ptr = (unsigned char *)field_ptr + i * element_type->size;
            sijson_free_reflected_field(element_type, element_ptr);
        }
        return;
    }
    case sireflect_kind_u8:
    case sireflect_kind_u16:
    case sireflect_kind_u32:
    case sireflect_kind_u64:
    case sireflect_kind_i8:
    case sireflect_kind_i16:
    case sireflect_kind_i32:
    case sireflect_kind_i64:
    case sireflect_kind_signed_char:
    case sireflect_kind_unsigned_char:
    case sireflect_kind_unsigned_short:
    case sireflect_kind_unsigned_int:
    case sireflect_kind_unsigned_long:
    case sireflect_kind_long_long:
    case sireflect_kind_unsigned_long_long:
    case sireflect_kind_f32:
    case sireflect_kind_f64:
    case sireflect_kind_bool:
    case sireflect_kind_char:
    case sireflect_kind_short:
    case sireflect_kind_int:
    case sireflect_kind_long:
        return;
    }
}

void sijson_free_impl(sireflect_handle_t *ref, const sireflect_struct_desc_t *desc, void *ptr) {
    sijson_clear_error();
    sireflect_handle_t type = sijson_register_type(ref, desc);
    if (type == SIREFLECT_INVALID_HANDLE) {
        return;
    }
    sijson_free_reflected(type, ptr);
}

#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifndef SIJSON_ARENA_RESERVE
#define SIJSON_ARENA_RESERVE ((size_t)1 << 30)
#endif

#ifndef SIJSON_ARENA_FALLBACK_RESERVE
#define SIJSON_ARENA_FALLBACK_RESERVE ((size_t)1 << 20)
#endif

typedef struct sijson_arena {
    unsigned char *data;
    size_t used;
    size_t cap;
    size_t reserve;
    bool mmap_backed;
} sijson_arena_t;

static sijson_arena_t g_arena;

static size_t sijson_align_forward(size_t value, size_t align) {
    size_t mask = align - 1;
    return (value + mask) & ~mask;
}

static size_t sijson_page_size(void) {
#if defined(__unix__) || defined(__APPLE__)
    long page = sysconf(_SC_PAGESIZE);
    if (page > 0) {
        return (size_t)page;
    }
#endif
    return 4096;
}

static bool sijson_arena_init(size_t need) {
    if (g_arena.data != NULL) {
        return true;
    }

    size_t page_size = sijson_page_size();
    size_t reserve = SIJSON_ARENA_RESERVE;
    if (reserve < need) {
        reserve = sijson_align_forward(need, page_size);
    }

#if defined(__unix__) || defined(__APPLE__)
    void *data = mmap(NULL, reserve, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (data != MAP_FAILED) {
        g_arena.data = data;
        g_arena.reserve = reserve;
        g_arena.mmap_backed = true;
        return true;
    }
#endif

    size_t fallback_reserve = reserve;
    if (need <= SIJSON_ARENA_FALLBACK_RESERVE && fallback_reserve > SIJSON_ARENA_FALLBACK_RESERVE) {
        fallback_reserve = SIJSON_ARENA_FALLBACK_RESERVE;
    }

    g_arena.data = malloc(fallback_reserve);
    if (g_arena.data == NULL) {
        sijson_set_error("out of memory");
        return false;
    }
    g_arena.cap = fallback_reserve;
    g_arena.reserve = fallback_reserve;
    return true;
}

static bool sijson_arena_commit(size_t need) {
    if (!sijson_arena_init(need)) {
        return false;
    }
    if (need <= g_arena.cap) {
        return true;
    }
    if (need > g_arena.reserve) {
        return sijson_set_error("sijson arena capacity exceeded");
    }

    size_t page_size = sijson_page_size();
    size_t cap = sijson_align_forward(need, page_size);

    if (g_arena.mmap_backed) {
#if defined(__unix__) || defined(__APPLE__)
        if (mprotect(g_arena.data + g_arena.cap, cap - g_arena.cap, PROT_READ | PROT_WRITE) != 0) {
            return sijson_set_error("out of memory");
        }
#else
        return sijson_set_error("sijson arena backend unavailable");
#endif
    }

    g_arena.cap = cap;
    return true;
}

void *sijson_arena_alloc(size_t size, size_t align) {
    if (align == 0) {
        align = _Alignof(max_align_t);
    }

    size_t offset = sijson_align_forward(g_arena.used, align);
    if (offset < g_arena.used || size > SIZE_MAX - offset) {
        sijson_set_error("out of memory");
        return NULL;
    }

    size_t need = offset + size;
    if (!sijson_arena_commit(need)) {
        return NULL;
    }

    void *ptr = g_arena.data + offset;
    g_arena.used = need;
    return ptr;
}

char *sijson_arena_dup_range(const char *start, size_t len) {
    char *result = sijson_arena_alloc(len + 1, _Alignof(char));
    if (result == NULL) {
        return NULL;
    }

    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

char *sijson_arena_dup_cstr(const char *str) {
    if (str == NULL) {
        return NULL;
    }

    return sijson_arena_dup_range(str, strlen(str));
}

size_t sijson_arena_mark(void) {
    return g_arena.used;
}

void sijson_arena_rewind(size_t mark) {
    if (mark <= g_arena.used) {
        g_arena.used = mark;
    }
}

void sijson_clean(void) {
    sijson_clear_error();
    g_arena.used = 0;
}

void sijson_release(void) {
    sijson_clear_error();
    if (g_arena.mmap_backed) {
#if defined(__unix__) || defined(__APPLE__)
        munmap(g_arena.data, g_arena.reserve);
#endif
    } else {
        free(g_arena.data);
    }
    g_arena = (sijson_arena_t){ 0 };
}

char *sijson_dup_range(const char *start, size_t len) {
    char *result = malloc(len + 1);
    if (result == NULL) {
        sijson_set_error("out of memory");
        return NULL;
    }

    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

char *sijson_dup_cstr(const char *str) {
    if (str == NULL) {
        return NULL;
    }

    return sijson_dup_range(str, strlen(str));
}

bool sijson_reserve_array(sijson_array_t *array, size_t need) {
    if (array->cap >= need) {
        return true;
    }

    size_t cap = array->cap != 0 ? array->cap * 2 : 8;
    while (cap < need) {
        cap *= 2;
    }

    sijson_value_t *items = sijson_arena_alloc(cap * sizeof(*items), _Alignof(sijson_value_t));
    if (items == NULL) {
        return false;
    }
    if (array->items != NULL) {
        memcpy(items, array->items, array->len * sizeof(*items));
    }

    array->items = items;
    array->cap = cap;
    return true;
}

bool sijson_reserve_object(sijson_object_t *object, size_t need) {
    if (object->cap >= need) {
        return true;
    }

    size_t cap = object->cap != 0 ? object->cap * 2 : 8;
    while (cap < need) {
        cap *= 2;
    }

    sijson_member_t *items = sijson_arena_alloc(cap * sizeof(*items), _Alignof(sijson_member_t));
    if (items == NULL) {
        return false;
    }
    if (object->items != NULL) {
        memcpy(items, object->items, object->len * sizeof(*items));
    }

    object->items = items;
    object->cap = cap;
    return true;
}

sijson_value_t sijson_new_value(sijson_type_t type) {
    sijson_value_t value = sijson_arena_alloc(sizeof(*value), _Alignof(struct sijson_value));
    if (value == NULL) {
        return NULL;
    }

    memset(value, 0, sizeof(*value));
    value->type = type;
    return value;
}

sijson_value_t sijson_make_null(void) {
    sijson_clear_error();
    return sijson_new_value(SIJSON_NULL);
}

sijson_value_t sijson_make_bool(bool value) {
    sijson_clear_error();
    sijson_value_t result = sijson_new_value(SIJSON_BOOL);
    if (result != NULL) {
        result->as.boolean = value;
    }
    return result;
}

sijson_value_t sijson_make_number(double value) {
    sijson_clear_error();
    sijson_value_t result = sijson_new_value(SIJSON_NUMBER);
    if (result != NULL) {
        result->as.number = value;
    }
    return result;
}

sijson_value_t sijson_make_string(const char *value) {
    sijson_clear_error();
    sijson_value_t result = sijson_new_value(SIJSON_STRING);
    if (result == NULL) {
        return NULL;
    }

    result->as.string = sijson_arena_dup_cstr(value != NULL ? value : "");
    if (result->as.string == NULL) {
        return NULL;
    }

    return result;
}

sijson_value_t sijson_make_array(void) {
    sijson_clear_error();
    return sijson_new_value(SIJSON_ARRAY);
}

sijson_value_t sijson_make_object(void) {
    sijson_clear_error();
    return sijson_new_value(SIJSON_OBJECT);
}

sijson_type_t sijson_type(sijson_value_t value) {
    return value != NULL ? value->type : SIJSON_NULL;
}

bool sijson_bool(sijson_value_t value) {
    return value != NULL && value->type == SIJSON_BOOL ? value->as.boolean : false;
}

double sijson_number(sijson_value_t value) {
    return value != NULL && value->type == SIJSON_NUMBER ? value->as.number : 0.0;
}

const char *sijson_string(sijson_value_t value) {
    return value != NULL && value->type == SIJSON_STRING ? value->as.string : NULL;
}

size_t sijson_array_len(sijson_value_t value) {
    return value != NULL && value->type == SIJSON_ARRAY ? value->as.array.len : 0;
}

sijson_value_t sijson_array_get(sijson_value_t value, size_t index) {
    if (value == NULL || value->type != SIJSON_ARRAY || index >= value->as.array.len) {
        return NULL;
    }

    return value->as.array.items[index];
}

size_t sijson_object_len(sijson_value_t value) {
    return value != NULL && value->type == SIJSON_OBJECT ? value->as.object.len : 0;
}

const char *sijson_object_key(sijson_value_t value, size_t index) {
    if (value == NULL || value->type != SIJSON_OBJECT || index >= value->as.object.len) {
        return NULL;
    }

    return value->as.object.items[index].key;
}

sijson_value_t sijson_object_get(sijson_value_t value, const char *key) {
    if (value == NULL || value->type != SIJSON_OBJECT || key == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < value->as.object.len; i++) {
        if (strcmp(value->as.object.items[i].key, key) == 0) {
            return value->as.object.items[i].value;
        }
    }

    return NULL;
}

bool sijson_array_push(sijson_value_t array, sijson_value_t value) {
    sijson_clear_error();
    if (array == NULL || array->type != SIJSON_ARRAY) {
        return sijson_set_error("sijson_array_push expects an array");
    }

    if (!sijson_reserve_array(&array->as.array, array->as.array.len + 1)) {
        return false;
    }

    array->as.array.items[array->as.array.len++] = value;
    return true;
}

bool sijson_object_set(sijson_value_t object, const char *key, sijson_value_t value) {
    sijson_clear_error();
    if (object == NULL || object->type != SIJSON_OBJECT) {
        return sijson_set_error("sijson_object_set expects an object");
    }
    if (key == NULL) {
        return sijson_set_error("sijson_object_set expects a key");
    }

    for (size_t i = 0; i < object->as.object.len; i++) {
        if (strcmp(object->as.object.items[i].key, key) == 0) {
            object->as.object.items[i].value = value;
            return true;
        }
    }

    if (!sijson_reserve_object(&object->as.object, object->as.object.len + 1)) {
        return false;
    }

    char *owned_key = sijson_arena_dup_cstr(key);
    if (owned_key == NULL) {
        return false;
    }

    object->as.object.items[object->as.object.len++] = (sijson_member_t){
        .key = owned_key,
        .value = value,
    };
    return true;
}

bool sijson_writer_reserve(sijson_writer_t *writer, size_t extra) {
    if (writer->len + extra + 1 <= writer->cap) {
        return true;
    }

    size_t cap = writer->cap != 0 ? writer->cap * 2 : 128;
    while (cap < writer->len + extra + 1) {
        cap *= 2;
    }

    char *data = realloc(writer->data, cap);
    if (data == NULL) {
        return sijson_set_error("out of memory");
    }

    writer->data = data;
    writer->cap = cap;
    return true;
}

bool sijson_writer_putc(sijson_writer_t *writer, char c) {
    if (!sijson_writer_reserve(writer, 1)) {
        return false;
    }

    writer->data[writer->len++] = c;
    writer->data[writer->len] = '\0';
    return true;
}

bool sijson_writer_write(sijson_writer_t *writer, const char *data, size_t len) {
    if (!sijson_writer_reserve(writer, len)) {
        return false;
    }

    memcpy(writer->data + writer->len, data, len);
    writer->len += len;
    writer->data[writer->len] = '\0';
    return true;
}

bool sijson_writer_cstr(sijson_writer_t *writer, const char *str) {
    return sijson_writer_write(writer, str, strlen(str));
}

bool sijson_writer_string(sijson_writer_t *writer, const char *str) {
    if (str == NULL) {
        return sijson_writer_cstr(writer, "null");
    }

    if (!sijson_writer_putc(writer, '"')) {
        return false;
    }

    const unsigned char *cur = (const unsigned char *)str;
    const unsigned char *chunk = cur;
    while (*cur != '\0') {
        unsigned char c = *cur;
        if (c == '"' || c == '\\' || c < 0x20) {
            if (!sijson_writer_write(writer, (const char *)chunk, (size_t)(cur - chunk))) {
                return false;
            }

            switch (c) {
            case '"':
                if (!sijson_writer_cstr(writer, "\\\"")) {
                    return false;
                }
                break;
            case '\\':
                if (!sijson_writer_cstr(writer, "\\\\")) {
                    return false;
                }
                break;
            case '\b':
                if (!sijson_writer_cstr(writer, "\\b")) {
                    return false;
                }
                break;
            case '\f':
                if (!sijson_writer_cstr(writer, "\\f")) {
                    return false;
                }
                break;
            case '\n':
                if (!sijson_writer_cstr(writer, "\\n")) {
                    return false;
                }
                break;
            case '\r':
                if (!sijson_writer_cstr(writer, "\\r")) {
                    return false;
                }
                break;
            case '\t':
                if (!sijson_writer_cstr(writer, "\\t")) {
                    return false;
                }
                break;
            default: {
                char escape[7];
                snprintf(escape, sizeof(escape), "\\u%04x", c);
                if (!sijson_writer_cstr(writer, escape)) {
                    return false;
                }
                break;
            }
            }

            cur++;
            chunk = cur;
            continue;
        }
        cur++;
    }

    if (!sijson_writer_write(writer, (const char *)chunk, (size_t)(cur - chunk))) {
        return false;
    }

    return sijson_writer_putc(writer, '"');
}

bool sijson_write_value(sijson_writer_t *writer, sijson_value_t value);

static bool sijson_write_array(sijson_writer_t *writer, sijson_value_t value) {
    if (!sijson_writer_putc(writer, '[')) {
        return false;
    }

    for (size_t i = 0; i < value->as.array.len; i++) {
        if (i != 0 && !sijson_writer_putc(writer, ',')) {
            return false;
        }
        if (!sijson_write_value(writer, value->as.array.items[i])) {
            return false;
        }
    }

    return sijson_writer_putc(writer, ']');
}

static bool sijson_write_object(sijson_writer_t *writer, sijson_value_t value) {
    if (!sijson_writer_putc(writer, '{')) {
        return false;
    }

    for (size_t i = 0; i < value->as.object.len; i++) {
        if (i != 0 && !sijson_writer_putc(writer, ',')) {
            return false;
        }
        if (!sijson_writer_string(writer, value->as.object.items[i].key)) {
            return false;
        }
        if (!sijson_writer_putc(writer, ':')) {
            return false;
        }
        if (!sijson_write_value(writer, value->as.object.items[i].value)) {
            return false;
        }
    }

    return sijson_writer_putc(writer, '}');
}

bool sijson_write_value(sijson_writer_t *writer, sijson_value_t value) {
    if (value == NULL) {
        return sijson_writer_cstr(writer, "null");
    }

    switch (value->type) {
    case SIJSON_NULL:
        return sijson_writer_cstr(writer, "null");
    case SIJSON_BOOL:
        return sijson_writer_cstr(writer, value->as.boolean ? "true" : "false");
    case SIJSON_NUMBER: {
        if (!isfinite(value->as.number)) {
            return sijson_set_error("cannot write non-finite JSON number");
        }
        char number[64];
        int len = snprintf(number, sizeof(number), "%.17g", value->as.number);
        if (len < 0 || (size_t)len >= sizeof(number)) {
            return sijson_set_error("failed to format JSON number");
        }
        return sijson_writer_write(writer, number, (size_t)len);
    }
    case SIJSON_STRING:
        return sijson_writer_string(writer, value->as.string);
    case SIJSON_ARRAY:
        return sijson_write_array(writer, value);
    case SIJSON_OBJECT:
        return sijson_write_object(writer, value);
    }

    return sijson_set_error("unknown JSON value type");
}

char *sijson_stringify(sijson_value_t value) {
    sijson_clear_error();
    sijson_writer_t writer = { 0 };
    if (!sijson_write_value(&writer, value)) {
        free(writer.data);
        return NULL;
    }
    if (writer.data == NULL) {
        return sijson_dup_cstr("");
    }
    return writer.data;
}
#ifndef SIHTTP_BUFFER_H
#define SIHTTP_BUFFER_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} sihttp_buffer_t;

void sihttp_buffer_init(sihttp_buffer_t *buffer);
void sihttp_buffer_fini(sihttp_buffer_t *buffer);
int sihttp_buffer_append(sihttp_buffer_t *buffer, const char *data, size_t len);

#endif
#ifndef SIHTTP_INTERNAL_H
#define SIHTTP_INTERNAL_H


#include <stddef.h>
#include <stdint.h>

#define SIHTTP_MAX_HEADER_BYTES (16u * 1024u)
#define SIHTTP_MAX_BODY_BYTES (1024u * 1024u)
#define SIHTTP_MAX_HEADERS 64u
#define SIHTTP_MAX_PARAMS 16u

typedef struct {
    const char *name;
    const char *value;
} sihttp_pair_t;

typedef struct {
    sihttp_request_t public_req;
    char param_names[SIHTTP_MAX_PARAMS][32];
    char param_values[SIHTTP_MAX_PARAMS][64];
    sihttp_pair_t params[SIHTTP_MAX_PARAMS];
    size_t param_count;
    sihttp_pair_t query[SIHTTP_MAX_PARAMS];
    size_t query_count;
    char *storage;
    size_t storage_len;
} sihttp_request_internal_t;

typedef struct {
    int code;
    size_t expected_len;
} sihttp_parse_result_t;

void sihttp_set_error(const char *fmt, ...) SIHTTP_PRINTF_FORMAT(1, 2);

const char *sihttp_method_name(sihttp_method_t method);
sihttp_method_t sihttp_method_from_name(const char *method, int *ok);

void sihttp_request_internal_init(sihttp_request_internal_t *req);
void sihttp_request_internal_fini(sihttp_request_internal_t *req);
int sihttp_request_add_param(sihttp_request_internal_t *req, const char *name, const char *value);
int sihttp_request_parse(
    sihttp_request_internal_t *req,
    const char *data,
    size_t len,
    sihttp_app_state_t *state
);
sihttp_parse_result_t sihttp_request_parse_state(const char *data, size_t len);

char *sihttp_build_response(sihttp_response_t response, size_t *out_len);
int sihttp_send_response(int fd, sihttp_response_t response);

typedef struct sihttp_route_table_s sihttp_route_table_t;

int sihttp_route_table_init(sihttp_route_table_t *table);
void sihttp_route_table_fini(sihttp_route_table_t *table);
int sihttp_route_table_add(
    sihttp_route_table_t *table,
    sihttp_method_t method,
    const char *path,
    sihttp_handler_t callback
);
sihttp_handler_t sihttp_route_table_match(
    const sihttp_route_table_t *table,
    sihttp_method_t method,
    const char *path,
    sihttp_request_internal_t *req,
    int *method_not_allowed
);

struct sihttp_server_s {
    sihttp_app_state_t *state;
    sihttp_route_table_t *routes;
    int listen_fd;
    uint16_t port;
    int backlog;
    int max_requests_per_poll;
    int running;
};

int sihttp_server_handle_client(sihttp_server_t *server, int client_fd);

#endif
#ifndef SIHTTP_ROUTE_H
#define SIHTTP_ROUTE_H


typedef struct {
    sihttp_method_t method;
    char *path;
    sihttp_handler_t callback;
} sihttp_route_entry_t;

struct sihttp_route_table_s {
    sihttp_route_entry_t *entries;
    size_t count;
    size_t capacity;
};

#endif

#include <stdlib.h>
#include <string.h>

void sihttp_buffer_init(sihttp_buffer_t *buffer) {
    buffer->data = NULL;
    buffer->len = 0;
    buffer->cap = 0;
}

void sihttp_buffer_fini(sihttp_buffer_t *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->len = 0;
    buffer->cap = 0;
}

int sihttp_buffer_append(sihttp_buffer_t *buffer, const char *data, size_t len) {
    size_t required;

    if (len == 0) {
        return 0;
    }

    required = buffer->len + len;
    if (required < buffer->len) {
        return -1;
    }

    if (required > buffer->cap) {
        size_t next = buffer->cap ? buffer->cap : 1024;
        char *new_data;

        while (next < required) {
            size_t doubled = next * 2;
            if (doubled < next) {
                return -1;
            }
            next = doubled;
        }

        new_data = realloc(buffer->data, next);
        if (!new_data) {
            return -1;
        }

        buffer->data = new_data;
        buffer->cap = next;
    }

    memcpy(buffer->data + buffer->len, data, len);
    buffer->len += len;
    return 0;
}

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static const char *sihttp_find_header_end_const(const char *data, size_t len) {
    if (len < 4) {
        return NULL;
    }

    for (size_t i = 0; i + 3 < len; i++) {
        if (data[i] == '\r' && data[i + 1] == '\n' && data[i + 2] == '\r' && data[i + 3] == '\n') {
            return data + i;
        }
    }

    return NULL;
}

static char *sihttp_find_header_end(char *data, size_t len) {
    if (len < 4) {
        return NULL;
    }

    for (size_t i = 0; i + 3 < len; i++) {
        if (data[i] == '\r' && data[i + 1] == '\n' && data[i + 2] == '\r' && data[i + 3] == '\n') {
            return data + i;
        }
    }

    return NULL;
}

static int sihttp_streq_icase(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static char *sihttp_trim(char *str) {
    char *end;

    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    end = str + strlen(str);
    while (end > str && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';

    return str;
}

static int sihttp_parse_size(const char *value, size_t *out) {
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno || end == value || *sihttp_trim(end) != '\0' || parsed > SIZE_MAX) {
        return -1;
    }

    *out = (size_t)parsed;
    return 0;
}

static int sihttp_add_pair(sihttp_pair_t *pairs, size_t *count, const char *name, const char *value) {
    if (*count >= SIHTTP_MAX_PARAMS) {
        return -1;
    }

    pairs[*count].name = name;
    pairs[*count].value = value;
    (*count)++;
    return 0;
}

static void sihttp_parse_query(sihttp_request_internal_t *req, char *query) {
    char *cursor = query;
    while (cursor && *cursor && req->query_count < SIHTTP_MAX_PARAMS) {
        char *next = strchr(cursor, '&');
        char *eq;

        if (next) {
            *next = '\0';
            next++;
        }

        eq = strchr(cursor, '=');
        if (eq) {
            *eq = '\0';
            sihttp_add_pair(req->query, &req->query_count, cursor, eq + 1);
        }

        cursor = next;
    }
}

void sihttp_request_internal_init(sihttp_request_internal_t *req) {
    memset(req, 0, sizeof(*req));
}

void sihttp_request_internal_fini(sihttp_request_internal_t *req) {
    free(req->storage);
    sihttp_request_internal_init(req);
}

int sihttp_request_add_param(sihttp_request_internal_t *req, const char *name, const char *value) {
    size_t name_len;
    size_t value_len;

    if (req->param_count >= SIHTTP_MAX_PARAMS) {
        return -1;
    }

    name_len = strlen(name);
    value_len = strlen(value);
    if (name_len >= sizeof(req->param_names[0]) || value_len >= sizeof(req->param_values[0])) {
        return -1;
    }

    memcpy(req->param_names[req->param_count], name, name_len + 1);
    memcpy(req->param_values[req->param_count], value, value_len + 1);
    req->params[req->param_count].name = req->param_names[req->param_count];
    req->params[req->param_count].value = req->param_values[req->param_count];
    req->param_count++;
    return 0;
}

const char *sihttp_method_name(sihttp_method_t method) {
    switch (method) {
    case SIHTTP_METHOD_GET:
        return "GET";
    case SIHTTP_METHOD_POST:
        return "POST";
    case SIHTTP_METHOD_PUT:
        return "PUT";
    case SIHTTP_METHOD_DELETE:
        return "DELETE";
    case SIHTTP_METHOD_OPTIONS:
        return "OPTIONS";
    }

    return "GET";
}

sihttp_method_t sihttp_method_from_name(const char *method, int *ok) {
    if (strcmp(method, "GET") == 0) {
        *ok = 1;
        return SIHTTP_METHOD_GET;
    }
    if (strcmp(method, "POST") == 0) {
        *ok = 1;
        return SIHTTP_METHOD_POST;
    }
    if (strcmp(method, "PUT") == 0) {
        *ok = 1;
        return SIHTTP_METHOD_PUT;
    }
    if (strcmp(method, "DELETE") == 0) {
        *ok = 1;
        return SIHTTP_METHOD_DELETE;
    }
    if (strcmp(method, "OPTIONS") == 0) {
        *ok = 1;
        return SIHTTP_METHOD_OPTIONS;
    }

    *ok = 0;
    return SIHTTP_METHOD_GET;
}

sihttp_parse_result_t sihttp_request_parse_state(const char *data, size_t len) {
    sihttp_parse_result_t result = { .code = 0, .expected_len = 0 };
    const char *headers_end;
    size_t header_len;
    size_t content_length = 0;
    char *copy;
    char *line;

    headers_end = sihttp_find_header_end_const(data, len);
    if (!headers_end) {
        if (len > SIHTTP_MAX_HEADER_BYTES) {
            result.code = 413;
        }
        return result;
    }

    header_len = (size_t)(headers_end - data) + 4;
    if (header_len > SIHTTP_MAX_HEADER_BYTES) {
        result.code = 413;
        return result;
    }

    copy = malloc(header_len + 1);
    if (!copy) {
        result.code = 500;
        return result;
    }
    memcpy(copy, data, header_len);
    copy[header_len] = '\0';

    line = strstr(copy, "\r\n");
    while (line) {
        char *line_end;
        char *colon;

        line += 2;
        if (*line == '\r' && line[1] == '\n') {
            break;
        }

        line_end = strstr(line, "\r\n");
        if (!line_end) {
            break;
        }
        *line_end = '\0';

        colon = strchr(line, ':');
        if (colon) {
            char *name;
            char *value;

            *colon = '\0';
            name = sihttp_trim(line);
            value = sihttp_trim(colon + 1);
            if (sihttp_streq_icase(name, "Content-Length") && sihttp_parse_size(value, &content_length) != 0) {
                free(copy);
                result.code = 400;
                return result;
            }
        }

        line = line_end;
    }

    free(copy);

    if (content_length > SIHTTP_MAX_BODY_BYTES) {
        result.code = 413;
        return result;
    }

    result.expected_len = header_len + content_length;
    if (len >= result.expected_len) {
        result.code = 200;
    }
    return result;
}

int sihttp_request_parse(
    sihttp_request_internal_t *req,
    const char *data,
    size_t len,
    sihttp_app_state_t *state
) {
    sihttp_parse_result_t state_result;
    char *headers_end;
    char *body;
    char *request_line_end;
    char *method;
    char *target;
    char *version;
    char *query;
    int method_ok = 0;

    sihttp_request_internal_init(req);

    state_result = sihttp_request_parse_state(data, len);
    if (state_result.code != 200) {
        return state_result.code ? state_result.code : 400;
    }

    req->storage = malloc(state_result.expected_len + 1);
    if (!req->storage) {
        return 500;
    }

    memcpy(req->storage, data, state_result.expected_len);
    req->storage[state_result.expected_len] = '\0';
    req->storage_len = state_result.expected_len;

    headers_end = sihttp_find_header_end(req->storage, req->storage_len);
    if (!headers_end) {
        return 400;
    }

    body = headers_end + 4;
    *headers_end = '\0';

    request_line_end = strstr(req->storage, "\r\n");
    if (!request_line_end) {
        return 400;
    }
    *request_line_end = '\0';

    method = req->storage;
    target = strchr(method, ' ');
    if (!target) {
        return 400;
    }
    *target++ = '\0';

    version = strchr(target, ' ');
    if (!version) {
        return 400;
    }
    *version++ = '\0';

    if (strcmp(version, "HTTP/1.1") != 0 && strcmp(version, "HTTP/1.0") != 0) {
        return 400;
    }

    query = strchr(target, '?');
    if (query) {
        *query++ = '\0';
        sihttp_parse_query(req, query);
    }

    sihttp_method_from_name(method, &method_ok);
    if (!method_ok) {
        return 405;
    }

    req->public_req.method = method;
    req->public_req.path = target;
    req->public_req.body = body;
    req->public_req.state = state;
    return 200;
}

SIHTTP_API int64_t sihttp_param(const sihttp_request_t *public_req, const char *name) {
    const sihttp_request_internal_t *req = (const sihttp_request_internal_t *)public_req;

    for (size_t i = 0; i < req->param_count; i++) {
        if (strcmp(req->params[i].name, name) == 0) {
            return strtoll(req->params[i].value, NULL, 10);
        }
    }

    for (size_t i = 0; i < req->query_count; i++) {
        if (strcmp(req->query[i].name, name) == 0) {
            return strtoll(req->query[i].value, NULL, 10);
        }
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static const char *sihttp_status_reason(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 413:
        return "Payload Too Large";
    case 500:
        return "Internal Server Error";
    }

    return status >= 200 && status < 300 ? "OK" : "Error";
}

static const char *sihttp_content_type_name(sihttp_content_type_t content_type) {
    switch (content_type) {
    case SIHTTP_CONTENT_AUTO:
    case SIHTTP_CONTENT_TEXT:
        return "text/plain; charset=utf-8";
    case SIHTTP_CONTENT_JSON:
        return "application/json";
    case SIHTTP_CONTENT_HTML:
        return "text/html; charset=utf-8";
    case SIHTTP_CONTENT_BINARY:
        return "application/octet-stream";
    }

    return "text/plain; charset=utf-8";
}

static int sihttp_send_all(int fd, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t written = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (written <= 0) {
            return -1;
        }
        sent += (size_t)written;
    }

    return 0;
}

char *sihttp_build_response(sihttp_response_t response, size_t *out_len) {
    int status = response.status == 0 ? 200 : response.status;
    const char *reason = sihttp_status_reason(status);
    const char *content_type = sihttp_content_type_name(response.content_type);
    const char *body = response.body ? response.body : "";
    size_t body_len = response.body ? strlen(response.body) : 0;
    int header_len;
    size_t total;
    char *message;

    header_len = snprintf(
        NULL,
        0,
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: %zu\r\n"
        "Content-Type: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "\r\n",
        status,
        reason,
        body_len,
        content_type
    );
    if (header_len < 0) {
        return NULL;
    }

    total = (size_t)header_len + body_len;
    message = malloc(total + 1);
    if (!message) {
        return NULL;
    }

    snprintf(
        message,
        (size_t)header_len + 1,
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: %zu\r\n"
        "Content-Type: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "\r\n",
        status,
        reason,
        body_len,
        content_type
    );
    memcpy(message + header_len, body, body_len);
    message[total] = '\0';

    if (out_len) {
        *out_len = total;
    }
    return message;
}

int sihttp_send_response(int fd, sihttp_response_t response) {
    size_t len = 0;
    char *message = sihttp_build_response(response, &len);
    int result;

    if (!message) {
        free(response.body);
        return -1;
    }

    result = sihttp_send_all(fd, message, len);
    free(message);
    free(response.body);
    return result;
}

#include <stdlib.h>
#include <string.h>

static char *sihttp_strdup(const char *str) {
    size_t len = strlen(str);
    char *copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, str, len + 1);
    return copy;
}

static int
sihttp_route_path_matches(const char *pattern, const char *path, sihttp_request_internal_t *req) {
    const char *p = pattern;
    const char *s = path;

    while (*p && *s) {
        if (*p == ':') {
            const char *name_start;
            const char *value_start;
            size_t name_len;
            size_t value_len;
            char name[32];
            char value[64];
            int ok;

            p++;
            name_start = p;
            while (*p && *p != '/') {
                p++;
            }

            value_start = s;
            while (*s && *s != '/') {
                s++;
            }

            name_len = (size_t)(p - name_start);
            value_len = (size_t)(s - value_start);
            if (name_len == 0 || value_len == 0 || req->param_count >= SIHTTP_MAX_PARAMS) {
                return 0;
            }

            if (name_len >= sizeof(name) || value_len >= sizeof(value)) {
                return 0;
            }
            memcpy(name, name_start, name_len);
            name[name_len] = '\0';
            memcpy(value, value_start, value_len);
            value[value_len] = '\0';

            ok = sihttp_request_add_param(req, name, value) == 0;
            if (!ok) {
                return 0;
            }
        } else if (*p == *s) {
            p++;
            s++;
        } else {
            return 0;
        }
    }

    return *p == '\0' && *s == '\0';
}

int sihttp_route_table_init(sihttp_route_table_t *table) {
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
    return 0;
}

void sihttp_route_table_fini(sihttp_route_table_t *table) {
    for (size_t i = 0; i < table->count; i++) {
        free(table->entries[i].path);
    }
    free(table->entries);
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
}

int sihttp_route_table_add(
    sihttp_route_table_t *table,
    sihttp_method_t method,
    const char *path,
    sihttp_handler_t callback
) {
    char *path_copy;

    if (!path || path[0] != '/' || !callback) {
        return -1;
    }

    if (table->count == table->capacity) {
        size_t next = table->capacity ? table->capacity * 2 : 8;
        sihttp_route_entry_t *entries = realloc(table->entries, next * sizeof(*entries));
        if (!entries) {
            return -1;
        }
        table->entries = entries;
        table->capacity = next;
    }

    path_copy = sihttp_strdup(path);
    if (!path_copy) {
        return -1;
    }

    table->entries[table->count++] = (sihttp_route_entry_t){
        .method = method,
        .path = path_copy,
        .callback = callback,
    };
    return 0;
}

sihttp_handler_t sihttp_route_table_match(
    const sihttp_route_table_t *table,
    sihttp_method_t method,
    const char *path,
    sihttp_request_internal_t *req,
    int *method_not_allowed
) {
    *method_not_allowed = 0;

    for (size_t i = 0; i < table->count; i++) {
        const sihttp_route_entry_t *entry = &table->entries[i];
        size_t saved_count = req->param_count;
        if (!sihttp_route_path_matches(entry->path, path, req)) {
            req->param_count = saved_count;
            continue;
        }

        if (entry->method == method) {
            return entry->callback;
        }

        req->param_count = saved_count;
        *method_not_allowed = 1;
    }

    return NULL;
}

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

SIHTTP_API char *siformat(const char *fmt, ...) {
    va_list args;
    va_list copy;
    int size;
    char *str;

    va_start(args, fmt);
    va_copy(copy, args);

    size = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (size < 0) {
        va_end(args);
        return NULL;
    }

    str = malloc((size_t)size + 1);
    if (!str) {
        va_end(args);
        return NULL;
    }

    vsnprintf(str, (size_t)size + 1, fmt, args);
    va_end(args);

    return str;
}

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static char sihttp_error_buffer[256];

void sihttp_set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(sihttp_error_buffer, sizeof(sihttp_error_buffer), fmt, args);
    va_end(args);
}

SIHTTP_API const char *sihttp_error(void) {
    return sihttp_error_buffer[0] ? sihttp_error_buffer : NULL;
}

static char *sihttp_static_body(const char *body) {
    size_t len = strlen(body);
    char *copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, body, len + 1);
    return copy;
}

enum {
    SIHTTP_DEFAULT_BACKLOG = 128,
    SIHTTP_DEFAULT_MAX_REQUESTS_PER_POLL = 64,
};

static int sihttp_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return -1;
    }

    return 0;
}

static sihttp_response_t sihttp_error_response(int status, const char *body) {
    return (sihttp_response_t){ .status = status, .body = sihttp_static_body(body) };
}

static sihttp_response_t sihttp_preflight_response(void) {
    return (sihttp_response_t){ .status = 204, .body = sihttp_static_body("") };
}

SIHTTP_API sihttp_server_t *sihttp_server_init(const sihttp_server_desc_t *desc) {
    sihttp_server_t *server;
    int port = 0;
    int backlog = SIHTTP_DEFAULT_BACKLOG;
    int max_requests_per_poll = SIHTTP_DEFAULT_MAX_REQUESTS_PER_POLL;

    if (desc) {
        if (desc->port < 0 || desc->port > UINT16_MAX) {
            sihttp_set_error("invalid server port: %d", desc->port);
            return NULL;
        }
        port = desc->port;
        backlog = desc->backlog > 0 ? desc->backlog : SIHTTP_DEFAULT_BACKLOG;
        max_requests_per_poll = desc->max_requests_per_poll > 0
            ? desc->max_requests_per_poll
            : SIHTTP_DEFAULT_MAX_REQUESTS_PER_POLL;
    }

    server = calloc(1, sizeof(*server));
    if (!server) {
        sihttp_set_error("out of memory");
        return NULL;
    }

    server->routes = malloc(sizeof(*server->routes));
    if (!server->routes) {
        free(server);
        sihttp_set_error("out of memory");
        return NULL;
    }

    sihttp_route_table_init(server->routes);
    server->port = (uint16_t)port;
    server->backlog = backlog;
    server->max_requests_per_poll = max_requests_per_poll;
    if (desc) {
        server->state = desc->state;
    }
    server->listen_fd = -1;
    return server;
}

SIHTTP_API void sihttp_server_fini(sihttp_server_t *server) {
    if (!server) {
        return;
    }

    sihttp_server_stop(server);
    sihttp_route_table_fini(server->routes);
    free(server->routes);
    free(server);
}

SIHTTP_API int sihttp_server_listen(sihttp_server_t *server, const char *host, uint16_t port) {
    int fd;
    int yes = 1;
    struct sockaddr_in addr;
    socklen_t addr_len;

    if (!server) {
        sihttp_set_error("server is NULL");
        return -1;
    }

    if (server->listen_fd != -1) {
        sihttp_set_error("server is already listening");
        return -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        sihttp_set_error("socket failed: %s", strerror(errno));
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (!host || strcmp(host, "") == 0 || strcmp(host, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close(fd);
        sihttp_set_error("invalid IPv4 host: %s", host);
        return -1;
    }

    if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        sihttp_set_error("bind failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, server->backlog) != 0) {
        sihttp_set_error("listen failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    addr_len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &addr_len) == 0) {
        server->port = ntohs(addr.sin_port);
    } else {
        server->port = port;
    }

    server->listen_fd = fd;
    return 0;
}

SIHTTP_API uint16_t sihttp_server_port(const sihttp_server_t *server) {
    return server ? server->port : 0;
}

SIHTTP_API void sihttp_server_stop(sihttp_server_t *server) {
    if (!server) {
        return;
    }

    server->running = 0;
    if (server->listen_fd != -1) {
        int fd = server->listen_fd;
        server->listen_fd = -1;
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
}

int sihttp_server_handle_client(sihttp_server_t *server, int client_fd) {
    sihttp_buffer_t buffer;
    int status = 400;
    sihttp_request_internal_t req;
    int method_ok = 0;
    sihttp_method_t method;
    int method_not_allowed = 0;
    sihttp_handler_t handler;
    sihttp_response_t response;

    sihttp_buffer_init(&buffer);

    for (;;) {
        char chunk[4096];
        sihttp_parse_result_t parse_state;
        ssize_t received = recv(client_fd, chunk, sizeof(chunk), 0);

        if (received < 0) {
            status = 400;
            break;
        }
        if (received == 0) {
            parse_state = sihttp_request_parse_state(buffer.data, buffer.len);
            status = parse_state.code == 200 ? 200 : 400;
            break;
        }

        if (sihttp_buffer_append(&buffer, chunk, (size_t)received) != 0) {
            status = 500;
            break;
        }

        parse_state = sihttp_request_parse_state(buffer.data, buffer.len);
        if (parse_state.code == 200) {
            status = 200;
            break;
        }
        if (parse_state.code != 0) {
            status = parse_state.code;
            break;
        }
    }

    if (status != 200) {
        sihttp_send_response(client_fd, sihttp_error_response(status, ""));
        sihttp_buffer_fini(&buffer);
        return -1;
    }

    status = sihttp_request_parse(&req, buffer.data, buffer.len, server->state);
    if (status != 200) {
        sihttp_send_response(client_fd, sihttp_error_response(status, ""));
        sihttp_buffer_fini(&buffer);
        return -1;
    }

    method = sihttp_method_from_name(req.public_req.method, &method_ok);
    if (!method_ok) {
        sihttp_send_response(client_fd, sihttp_error_response(405, ""));
        sihttp_request_internal_fini(&req);
        sihttp_buffer_fini(&buffer);
        return -1;
    }

    if (method == SIHTTP_METHOD_OPTIONS) {
        sihttp_send_response(client_fd, sihttp_preflight_response());
        sihttp_request_internal_fini(&req);
        sihttp_buffer_fini(&buffer);
        return 0;
    }

    handler = sihttp_route_table_match(
        server->routes,
        method,
        req.public_req.path,
        &req,
        &method_not_allowed
    );

    if (!handler) {
        sihttp_send_response(client_fd, sihttp_error_response(method_not_allowed ? 405 : 404, ""));
        sihttp_request_internal_fini(&req);
        sihttp_buffer_fini(&buffer);
        return -1;
    }

    response = handler(&req.public_req);
    if (sihttp_send_response(client_fd, response) != 0) {
        status = 500;
    }

    sihttp_request_internal_fini(&req);
    sihttp_buffer_fini(&buffer);
    return status == 200 ? 0 : -1;
}

SIHTTP_API int sihttp_server_start(sihttp_server_t *server) {
    if (!server) {
        sihttp_set_error("server is NULL");
        return -1;
    }

    if (server->listen_fd == -1 && sihttp_server_listen(server, NULL, server->port) != 0) {
        return -1;
    }

    if (sihttp_set_nonblocking(server->listen_fd) != 0) {
        sihttp_set_error("could not make server socket non-blocking: %s", strerror(errno));
        return -1;
    }

    server->running = 1;
    return 0;
}

SIHTTP_API int sihttp_server_poll(sihttp_server_t *server) {
    int handled = 0;

    if (!server) {
        sihttp_set_error("server is NULL");
        return -1;
    }

    if (!server->running && sihttp_server_start(server) != 0) {
        return -1;
    }

    while (handled < server->max_requests_per_poll) {
        int client_fd = accept(server->listen_fd, NULL, NULL);

        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return handled;
            }
            if (!server->running || server->listen_fd == -1 || errno == EBADF || errno == EINVAL) {
                return handled;
            }

            sihttp_set_error("accept failed: %s", strerror(errno));
            return -1;
        }

        sihttp_server_handle_client(server, client_fd);
        close(client_fd);
        handled++;
    }

    return handled;
}

SIHTTP_API int sihttp_server_run(sihttp_server_t *server) {
    if (!server) {
        sihttp_set_error("server is NULL");
        return -1;
    }

    if (server->listen_fd == -1 && sihttp_server_listen(server, NULL, server->port) != 0) {
        return -1;
    }

    server->running = 1;
    while (server->running) {
        int client_fd = accept(server->listen_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            if (!server->running || server->listen_fd == -1 || errno == EBADF || errno == EINVAL) {
                break;
            }
            sihttp_set_error("accept failed: %s", strerror(errno));
            return -1;
        }

        sihttp_server_handle_client(server, client_fd);
        close(client_fd);
    }

    return 0;
}

SIHTTP_API void
sihttp_route_impl(sihttp_server_t *server, const char *path, const sihttp_handler_desc_t *desc) {
    if (!server || !desc) {
        sihttp_set_error("invalid route descriptor");
        return;
    }

    if (sihttp_route_table_add(server->routes, desc->method, path, desc->callback) != 0) {
        sihttp_set_error("could not add route: %s", path ? path : "(null)");
    }
}

SIHTTP_API void sihttp_get(sihttp_server_t *server, const char *path, sihttp_handler_t callback) {
    sihttp_route(server, path, { .method = SIHTTP_METHOD_GET, .callback = callback });
}

SIHTTP_API void sihttp_post(sihttp_server_t *server, const char *path, sihttp_handler_t callback) {
    sihttp_route(server, path, { .method = SIHTTP_METHOD_POST, .callback = callback });
}

SIHTTP_API void sihttp_put(sihttp_server_t *server, const char *path, sihttp_handler_t callback) {
    sihttp_route(server, path, { .method = SIHTTP_METHOD_PUT, .callback = callback });
}

SIHTTP_API void
sihttp_delete(sihttp_server_t *server, const char *path, sihttp_handler_t callback) {
    sihttp_route(server, path, { .method = SIHTTP_METHOD_DELETE, .callback = callback });
}
#ifndef ECS_ADDONS_H
#define ECS_ADDONS_H

void init_rest();

#endif

#ifndef SIECS_DATASTRUCTURE_VEC_H
#define SIECS_DATASTRUCTURE_VEC_H
#ifndef SIECS_COMPILER_H
#define SIECS_COMPILER_H
#define ECS_LIKELY(x) __builtin_expect(!!(x), 1)
#define ECS_UNLIKELY(x) __builtin_expect(!!(x), 0)

#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *data;
    uint32_t size;
    uint32_t capacity;
} ecs_vec_t;

void ecs_vec_init(ecs_vec_t *vec, const uint32_t element_size);
void ecs_vec_init_w_size(ecs_vec_t *vec, const uint32_t element_size, uint32_t size);
void ecs_vec_fini(ecs_vec_t *vec);
void ecs_vec_grow(ecs_vec_t *vec, const uint32_t element_size);

// Ensure vec has at least `count` elements. New slots are zero-initialized.
void ecs_vec_ensure(ecs_vec_t *vec, uint32_t count, const uint32_t element_size);

// Copy element into the vec (memcpy). The pointer is not retained.
// Safe to call repeatedly — any grow only invalidates the internal buffer, not
// the source pointer.
void ecs_vec_push(ecs_vec_t *vec, const void *element, const uint32_t element_size);

// Reserve one slot and return a pointer to it (uninitialized).
// WARNING: the returned pointer is invalidated by any subsequent push or grow
// on the same vec. Finish all writes through this pointer before pushing again.
static inline void *ecs_vec_push_empty(ecs_vec_t *vec, const uint32_t element_size) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, element_size);
    }
    void *ptr = (uint8_t *)vec->data + (vec->size * element_size);
    vec->size++;
    return ptr;
}

bool ecs_vec_contains_u16(const ecs_vec_t *vec, uint16_t value);
void ecs_vec_remove_u16(ecs_vec_t *vec, uint16_t value);
void ecs_vec_remove_u64(ecs_vec_t *vec, uint64_t value);

// Specialized push for 2-byte types
static inline void ecs_vec_push_u16(ecs_vec_t *vec, const uint16_t value) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, sizeof(uint16_t));
    }
    ((uint16_t *)vec->data)[vec->size++] = value;
}

// Specialized push for 8-byte types
static inline void ecs_vec_push_u64(ecs_vec_t *vec, const uint64_t value) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, sizeof(uint64_t));
    }
    ((uint64_t *)vec->data)[vec->size++] = value;
}

void ecs_vec_remove_fast(ecs_vec_t *vec, uint32_t index, const uint32_t element_size);

// Direct pointer access for fast iteration
#define ecs_vec_get(vec, index, type) (&((const type *)(vec)->data)[index])
#define ecs_vec_get_mut(vec, index, type) (&((type *)(vec)->data)[index])
#define ecs_vec_remove_last(vec) ((vec)->size--)
#define ecs_vec_clear(vec) ((vec)->size = 0)
#define ecs_vec_data(vec, type) ((type *)(vec)->data)

#define ecs_vec_iter(vec, type, value, ...)                                                        \
    const type *__values = (vec)->data;                                                            \
    const uint32_t __count = (vec)->size;                                                          \
    for (uint32_t i = 0; i < __count; i++) {                                                       \
        const type *value = &__values[i];                                                          \
        __VA_ARGS__                                                                                \
    }

#endif

#ifndef SIREFLECT_H
#endif
#ifndef SIECS_STORAGE_TABLE_INDEX_H
#define SIECS_STORAGE_TABLE_INDEX_H
#ifndef SIECS_TABLE_H
#define SIECS_TABLE_H
#ifndef SIECS_DATASTRUCTURE_IDMAP_H
#define SIECS_DATASTRUCTURE_IDMAP_H
#include <stdint.h>

typedef struct {
    uint16_t *ids;
    uint16_t capacity;
} ecs_id_map_t;

void ecs_id_map_init(ecs_id_map_t *map);
void ecs_id_map_fini(ecs_id_map_t *map);

void ecs_id_map_ensure(ecs_id_map_t *map, uint16_t id);

static inline void ecs_id_map_set(ecs_id_map_t *map, uint16_t id, uint16_t value) {
    ecs_id_map_ensure(map, id);
    map->ids[id] = value;
}

static inline uint16_t ecs_id_map_at(const ecs_id_map_t *map, uint16_t id) { return map->ids[id]; }

static inline uint16_t ecs_id_map_at_or_invalid(const ecs_id_map_t *map, uint16_t id) {
    return map->capacity > id ? map->ids[id] : UINT16_MAX;
}

#endif

#ifndef SIECS_ID_H
#define SIECS_ID_H
#include <stdint.h>

#define ecs_entity(index, generation) (((uint64_t)(index) << 32) | (generation & 0xffffffff))

#define ecs_first(id) ((uint32_t)((id) >> 32))
#define ecs_second(id) ((uint32_t)((id) & 0xffffffff))

#endif

#ifndef SIECS_TYPE_H
#define SIECS_TYPE_H
#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t *ids;
    uint16_t count;
    ecs_entity_t base;
} ecs_type_t;

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id);
ecs_type_t ecs_type_with_remove_at(const ecs_type_t *type, uint16_t index);
ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base);

uint64_t ecs_type_bloom(const ecs_type_t *type);

void ecs_type_fini(ecs_type_t *type);

static inline int ecs_type_equals(const ecs_type_t *a, const ecs_type_t *b) {
    if (a->base != b->base)
        return 0;
    if (a->count != b->count)
        return 0;
    if (a->count == 0)
        return 1;
    return memcmp(a->ids, b->ids, (size_t)a->count * sizeof(uint16_t)) == 0;
}

#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *data;
    uint32_t size;
    uint16_t remove_edge; // the table that has the component removed or UINT16_MAX if the edge is
                          // not set
} ecs_column_t;

typedef struct ecs_table_s {
    ecs_id_map_t add_edge; // maps component id to the table that has the component added or column
                           // index if the component is in the table
    uint32_t entity_capacity;
    uint32_t entity_count;
    uint16_t data_count;
    ecs_entity_t *entities;
    ecs_column_t *cls;
    uint16_t *data_columns;
    ecs_type_t type;
    uint64_t bloom;
    ecs_vec_t observers_by_event; // ecs_vec_t per event id; each holds uint16_t observer ids.
} ecs_table_t;

struct ecs_component_index_s;

void ecs_table_init(
    ecs_table_t *table,
    ecs_type_t type,
    const struct ecs_component_index_s *component_index,
    uint16_t table_id
);
void ecs_table_fini(ecs_table_t *table);
uint32_t ecs_table_add_entity(ecs_table_t *table, ecs_entity_t entity);
// if the entity is not the last one, the last entity will be moved to the removed entity's
// position, and the moved entity will be returned
ecs_entity_t
ecs_table_remove_entity(ecs_table_t *table, uint32_t row, bool row_values_live);

void *ecs_table_get_component(ecs_table_t *table, ecs_component_t component_id, uint32_t row);

// Append an observer id to this table's dense list for the given event,
// growing the per-event slot array on demand.
void ecs_table_add_observer(ecs_table_t *table, uint16_t event, uint16_t observer_id);

static inline uint16_t
ecs_table_get_add_edge(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_id_map_at_or_invalid(&table->add_edge, component_id);
}

static inline void *
ecs_table_component_at_column(const ecs_table_t *table, uint16_t column_index, uint32_t row) {
    ecs_column_t *column = &table->cls[column_index];
    return column->size != 0 ? (uint8_t *)column->data + (column->size * row) : NULL;
}

static inline uint16_t
ecs_table_column_or_invalid(const ecs_table_t *table, ecs_component_t component_id) {
    uint16_t column_index = ecs_table_get_add_edge(table, component_id);
    if (column_index < table->type.count && table->type.ids[column_index] == component_id) {
        return column_index;
    }
    return UINT16_MAX;
}

bool ecs_table_has(
    const ecs_table_t *table,
    ecs_component_t component_id
);
bool ecs_table_is_a(const ecs_table_t *table, ecs_entity_t base);

static inline uint16_t
ecs_table_get_column_index(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_id_map_at(&table->add_edge, component_id);
}

static inline bool ecs_table_has_owned(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_table_column_or_invalid(table, component_id) != UINT16_MAX;
}

void *ecs_table_field(
        const ecs_table_t *table,
    ecs_component_t component_id,
    bool *is_shared
);

#endif

#include <stdint.h>

struct ecs_world_s;

typedef struct {
    uint16_t table_index; // UINT16_MAX for empty
    uint16_t hash;
} ecs_type_slot_t;

typedef struct {
    ecs_table_t *tables;
    ecs_type_slot_t *slots;
    uint16_t table_count;
    uint16_t table_capacity;
    uint8_t slot_shift; // slot_count = 1 << slot_shift
} ecs_table_index_t;

void ecs_table_index_init(ecs_table_index_t *map);
void ecs_table_index_fini(ecs_table_index_t *map);

#define ecs_table_index_at(map, index) (&(map)->tables[index])

uint16_t ecs_table_index_get_or_create(
    ecs_type_t type
);

#endif

#ifndef SIECS_WORLD_INTERNAL_H
#define SIECS_WORLD_INTERNAL_H
#ifndef SIECS_COMMAND_BUFFER_H
#define SIECS_COMMAND_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct ecs_world_s ecs_world_t;

typedef struct {
    ecs_component_t id;
    void *data;
} ecs_deferred_set_t;

typedef struct {
    ecs_entity_t entity;
    bool kill;
    bool has_base;
    ecs_entity_t base;
    ecs_vec_t add_ids;
    ecs_vec_t remove_ids;
    ecs_vec_t sets;
} ecs_entity_command_t;

typedef struct ecs_command_buffer_s {
    ecs_vec_t commands;
    uint32_t *entity_to_command;
    uint32_t entity_capacity;
} ecs_command_buffer_t;

void ecs_command_buffer_init(ecs_command_buffer_t *buffer);
void ecs_command_buffer_fini(ecs_command_buffer_t *buffer);

void ecs_command_buffer_add(ecs_entity_t entity, ecs_component_t id);
void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id);
void ecs_command_buffer_set(
        ecs_entity_t entity,
    ecs_component_t id,
    const void *data
);
void ecs_command_buffer_move(ecs_entity_t entity, ecs_component_t id, void *data);
void ecs_command_buffer_kill(ecs_entity_t entity);
void ecs_command_buffer_set_base(ecs_entity_t entity, ecs_entity_t target);
void ecs_command_buffer_flush();

void ecs_add_cid_now(ecs_entity_t entity, ecs_component_t id);
void ecs_remove_cid_now(ecs_entity_t entity, ecs_component_t id);
void ecs_set_cid_now(ecs_entity_t entity, ecs_component_t id, const void *data);
void ecs_move_cid_now(ecs_entity_t entity, ecs_component_t id, void *data);
void ecs_kill_now(ecs_entity_t entity);
void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target);

#endif

#ifndef ECS_ARENA_H
#define ECS_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct ecs_arena_block_s {
    struct ecs_arena_block_s *next;
    uint32_t capacity;
    uint32_t cursor;
    max_align_t data[];
} ecs_arena_block_t;

typedef struct {
    ecs_arena_block_t *first;
    ecs_arena_block_t *current;
    ecs_arena_block_t *last;
} ecs_arena_t;

void ecs_arena_init(ecs_arena_t *allocator);
void ecs_arena_fini(ecs_arena_t *allocator);
void *ecs_arena_alloc_slow(ecs_arena_t *allocator, uint32_t size);

static inline void *ecs_arena_alloc(ecs_arena_t *allocator, uint32_t size) {
    ecs_arena_block_t *block = allocator->current;
    const uint32_t alignment = (uint32_t)_Alignof(max_align_t);
    const uint32_t cursor = (block->cursor + alignment - 1u) & ~(alignment - 1u);
    if (ECS_LIKELY(cursor <= block->capacity && size <= block->capacity - cursor)) {
        block->cursor = cursor + size;
        return (uint8_t *)block->data + cursor;
    }
    return ecs_arena_alloc_slow(allocator, size);
}

static inline void ecs_arena_reset(ecs_arena_t *allocator) {
    for (ecs_arena_block_t *block = allocator->first; block; block = block->next) {
        block->cursor = 0;
    }
    allocator->current = allocator->first;
}

#endif

#ifndef SIHTTP_H
#endif
#ifndef SIREFLECT_H
#endif
#ifndef SIECS_STORAGE_COMPONENT_INDEX_H
#define SIECS_STORAGE_COMPONENT_INDEX_H
#ifndef SIREFLECT_H
#endif
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t *required;
    uint32_t required_count;
    uint32_t size;
    ecs_type_ops_t ops;
    ecs_component_on_set_t on_set;
    ecs_component_on_remove_t on_remove;
    ecs_component_on_add_t on_add;
    uint32_t relation_flags;
    ecs_vec_t tables; // uint16_t
    sireflect_handle_t reflection;
    const sireflect_struct_desc_t *reflection_desc;
} ecs_component_record_t;

typedef struct ecs_component_index_s {
    ecs_vec_t components; // ecs_component_record_t
} ecs_component_index_t;

void ecs_component_index_register(
    ecs_component_index_t *index,
    ecs_component_t id,
    uint64_t size,
    ecs_type_ops_t ops,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    uint32_t relation_flags,
    sireflect_handle_t reflection,
    const sireflect_struct_desc_t *reflection_desc
);

#define ecs_component_index_get(index, id)                                                         \
    ecs_vec_get(&(index)->components, id, ecs_component_record_t)
#define ecs_component_index_get_mut(index, id)                                                     \
    ecs_vec_get_mut(&(index)->components, id, ecs_component_record_t)

void ecs_component_index_init(ecs_component_index_t *index);
void ecs_component_index_fini(ecs_component_index_t *index);

void ecs_component_value_ctor(const ecs_component_record_t *record, void *dst, uint32_t count);
void ecs_component_value_dtor(const ecs_component_record_t *record, void *ptr, uint32_t count);
void ecs_component_value_copy_ctor(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
);
void ecs_component_value_copy(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
);
void ecs_component_value_move_ctor(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
);
void ecs_component_value_move(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
);

#endif

#ifndef SIECS_STORAGE_ENTITY_INDEX_H
#define SIECS_STORAGE_ENTITY_INDEX_H
#include <stdint.h>

typedef struct {
    uint16_t generation;
    uint16_t table_id;
    // Alive records store the row in their table. Dead records reuse this field
    // as the next entity id in the free list headed by first_available.
    uint32_t table_row;
} ecs_entity_record_t;

typedef struct {
    ecs_vec_t entities;       // ecs_entity_record_t
    uint32_t first_available; // UINT32_MAX when no dead entity can be reused
} ecs_entity_index_t;

#define ecs_entity_index_get_record(index, entity_id)                                              \
    ecs_vec_get_mut((&(index)->entities), entity_id, ecs_entity_record_t)

static inline ecs_entity_t ecs_entity_index_create(ecs_entity_index_t *index, uint32_t row) {
    uint32_t entity_id;
    uint32_t generation;
    if (index->first_available != UINT32_MAX) {
        entity_id = index->first_available;
        ecs_entity_record_t *record = ecs_entity_index_get_record(index, entity_id);
        index->first_available = record->table_row;
        generation = record->generation;
        record->table_id = 0;
        record->table_row = row;
    } else {
        entity_id = index->entities.size;
        generation = 0;
        ecs_entity_record_t *record = (ecs_entity_record_t *)
            ecs_vec_push_empty(&index->entities, sizeof(ecs_entity_record_t));
        *record = (ecs_entity_record_t){ .generation = 0, .table_row = row, .table_id = 0 };
    }
    return ecs_entity(entity_id, generation);
}

static inline bool ecs_entity_index_is_alive(const ecs_entity_index_t *index, ecs_entity_t entity) {
    return ecs_entity_index_get_record(index, ecs_first(entity))->generation == ecs_second(entity);
}

static inline void ecs_entity_index_kill(ecs_entity_index_t *index, uint32_t entity_id) {
    ecs_entity_record_t *record = ecs_entity_index_get_record(index, entity_id);
    record->generation += 1;
    record->table_row = index->first_available;
    record->table_id = UINT16_MAX;
    index->first_available = entity_id;
}

void ecs_entity_index_init(ecs_entity_index_t *index);
void ecs_entity_index_fini(ecs_entity_index_t *index);

#endif

#ifndef SIECS_STORAGE_MODULE_INDEX_H
#define SIECS_STORAGE_MODULE_INDEX_H

typedef struct {
    ecs_module_id_t *id;
    const char *name;
    ecs_vec_t observers;  // ecs_observer_id_t
    ecs_vec_t systems;    // ecs_system_id_t
    bool enabled;
} ecs_module_t;

typedef struct {
    ecs_vec_t modules; // ecs_module_t
} ecs_module_index_t;

void ecs_module_index_init(ecs_module_index_t *index);
void ecs_module_index_fini(ecs_module_index_t *index);

ecs_module_id_t ecs_module_index_create(
    ecs_module_index_t *index,
    ecs_module_id_t *id,
    const char *name
);
ecs_module_t *ecs_module_index_get(ecs_module_index_t *index, ecs_module_id_t module);
const ecs_module_t *ecs_module_index_get_const(
    const ecs_module_index_t *index,
    ecs_module_id_t module
);
ecs_module_id_t ecs_module_index_find(const ecs_module_index_t *index, const ecs_module_id_t *id);

#endif

#ifndef SIECS_STORAGE_OBSERVER_INDEX_H
#define SIECS_STORAGE_OBSERVER_INDEX_H
#ifndef SIECS_STORAGE_QUERY_INDEX_H
#define SIECS_STORAGE_QUERY_INDEX_H
#include <stdint.h>

typedef struct {
    uint64_t bloom;
    ecs_entity_t is_a;
    ecs_query_term_t *terms;
    ecs_query_term_t *fields;
    uint16_t term_count;
    uint16_t field_count;
} ecs_query_t;

typedef struct ecs_query_cache_s {
    ecs_query_t query;
    ecs_vec_t table_ids; // uint16_t
    void **fields_ptr;
    ecs_field_kind_t *fields_kind;
    uint16_t field_table_capacity;
    uint32_t active_index;
    uint16_t next_free;
    bool alive;
} ecs_query_cache_t;

typedef struct {
    ecs_vec_t queries;
    ecs_vec_t active_ids; // ecs_query_id_t
    uint16_t first_free;
} ecs_query_index_t;

void ecs_query_index_init(ecs_query_index_t *index);
void ecs_query_index_fini(ecs_query_index_t *index);
uint16_t ecs_query_index_create(ecs_query_index_t *index, const ecs_query_desc_t *desc);
void ecs_query_index_update_matches(ecs_query_cache_t *query_cache);
void ecs_query_index_add_table(const ecs_table_t *table, uint16_t table_id);

// Reusable query helpers shared with the observer index.
void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query);
void ecs_query_index_destroy(ecs_query_t *query);

static inline bool ecs_query_term_requires_owned(ecs_query_term_t term) {
    return term.access == EcsOut || term.access == EcsInOut || term.access == EcsInOutOptional;
}

static inline bool ecs_query_match_table(
    const ecs_query_t *query,
    const ecs_table_t *table
) {
    if (ECS_LIKELY((query->bloom & table->bloom) != query->bloom)) {
        return false;
    }

    if (query->is_a && !ecs_table_is_a(table, query->is_a)) {
        return false;
    }

    for (uint16_t i = 0; i < query->term_count; i++) {
        ecs_query_term_t term = query->terms[i];
        if (term.access == EcsInOptional || term.access == EcsInOutOptional) {
            continue;
        } else if (term.access == EcsNot) {
            if (ecs_table_has(table, term.id)) {
                return false;
            }
        } else if (ecs_query_term_requires_owned(term)) {
            if (ecs_table_column_or_invalid(table, term.id) == UINT16_MAX) {
                return false;
            }
        } else if (!ecs_table_has(table, term.id)) {
            return false;
        }
    }
    return true;
}

#endif

#include <stdint.h>

typedef struct {
    ecs_event_t event;
    ecs_query_t query;
    ecs_observer_callback_t callback;
    uintptr_t user_data;
    bool enabled;
} ecs_observer_t;

typedef struct {
    ecs_vec_t observers;  // ecs_observer_t
    uint16_t event_count; // next free event id; starts past the builtin events
} ecs_observer_index_t;

void ecs_observer_index_init(ecs_observer_index_t *index);
void ecs_observer_index_fini(ecs_observer_index_t *index);

uint16_t ecs_observer_index_create(ecs_observer_index_t *index, const ecs_observer_desc_t *desc);

// Cache a freshly created observer onto every existing table it matches.
void ecs_observer_index_match_tables(
        ecs_table_t *tables,
    uint16_t table_count,
    uint16_t observer_id
);

// Cache every existing observer that matches a freshly created table.
void ecs_observer_index_add_table(ecs_table_t *table);

#endif

#ifndef SIECS_STORAGE_RESOURCE_INDEX_H
#define SIECS_STORAGE_RESOURCE_INDEX_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    ecs_resource_desc_t *records;
    void **data;
    bool *present;
    uint64_t capacity;
    uint64_t count;
} ecs_resource_index_t;

void ecs_resource_index_init(ecs_resource_index_t *index);
void ecs_resource_index_fini(ecs_resource_index_t *index);

ecs_resource_t ecs_resource_index_register(
    ecs_resource_index_t *index,
    ecs_resource_t id,
    const ecs_resource_desc_t *desc
);
ecs_resource_t ecs_resource_index_find(const ecs_resource_index_t *index, const char *name);
bool ecs_resource_index_is_registered(const ecs_resource_index_t *index, ecs_resource_t id);
void ecs_resource_index_set(
    ecs_resource_index_t *index,
        ecs_resource_t id,
    const void *data
);
void ecs_resource_index_move(
    ecs_resource_index_t *index,
        ecs_resource_t id,
    void *data
);
void *ecs_resource_index_get(ecs_resource_index_t *index, ecs_resource_t id);
bool ecs_resource_index_has(const ecs_resource_index_t *index, ecs_resource_t id);
void ecs_resource_index_remove(ecs_resource_index_t *index, ecs_resource_t id);

#endif

#ifndef SIECS_STORAGE_SYSTEM_INDEX_H
#define SIECS_STORAGE_SYSTEM_INDEX_H
#include <stdint.h>

typedef struct {
    const char *name;
    ecs_query_id_t qid;
    void (*callback)(ecs_iter_t *);
    uintptr_t user_data;
    void (*user_data_dtor)(uintptr_t user_data);
    ecs_phase_t phase;
    ecs_system_id_t after[ECS_SYSTEM_AFTER_CAPACITY];
    bool enabled;
} ecs_system_t;

typedef struct {
    ecs_vec_t systems;
    ecs_vec_t phase_order[EcsPhaseCount];
    bool plan_dirty;
} ecs_system_index_t;

void ecs_system_index_init(ecs_system_index_t *index);
void ecs_system_index_fini(ecs_system_index_t *index);

ecs_system_id_t ecs_system_index_create(ecs_system_index_t *index, const ecs_system_t *system);
ecs_system_t *ecs_system_index_get(ecs_system_index_t *index, ecs_system_id_t system);
void ecs_system_index_build_plan(ecs_system_index_t *index);

#endif

typedef struct ecs_world_s ecs_world_t;

struct ecs_world_s {
    ecs_entity_index_t entity_index;
    ecs_component_index_t component_index;
    ecs_table_index_t table_index;
    ecs_query_index_t query_index;
    ecs_observer_index_t observer_index;
    ecs_system_index_t system_index;
    ecs_module_index_t module_index;
    ecs_resource_index_t resource_index;
    ecs_module_id_t active_module;
    sireflect_registry_t *sireflect_registry;
    sihttp_server_t *server;
    sihttp_app_state_t *server_state;
    ecs_world_feat_desc_t features;
    ecs_arena_t arena_allocator;
    ecs_command_buffer_t commands;
    uint32_t defer_depth;
    bool flushing_commands;
    bool did_start;
    bool exit;
    double delta_time;
    double last_time;
};

extern ecs_world_t ecs_world;

struct sihttp_app_state_s {};

typedef struct {
    ecs_entity_t target;
} RelationTarget;

typedef struct {
    ecs_vec_t entities;
} RelationSource;

#define ecs_get_record(entity)                                                              \
    ecs_vec_get_mut(&ecs_world.entity_index.entities, ecs_first(entity), ecs_entity_record_t)
#define ecs_get_table(tid) ecs_table_index_at(&ecs_world.table_index, tid)

static inline void ecs_emit(
    ecs_table_t *table,
    ecs_entity_t entity,
    ecs_event_t event,
    const void *trigger_data
) {
    if (table->observers_by_event.size <= event) {
        return;
    }
    const ecs_vec_t *list = ecs_vec_get(&table->observers_by_event, event, ecs_vec_t);
    uint32_t n = list->size;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t oid = *ecs_vec_get(list, i, uint16_t);
        ecs_observer_t *obs =
            ecs_vec_get_mut(&ecs_world.observer_index.observers, oid, ecs_observer_t);
        if (!obs->enabled) {
            continue;
        }
        ecs_observer_event_t observer_event = {
            .entity = entity,
            .event = event,
            .user_data = obs->user_data,
            .trigger_data = trigger_data,
        };
        obs->callback(&observer_event);
    }
}

void ecs_bootstrap(void);

#endif

ECS_RELATION_DEFINE(ChildOf, EcsRelationCascadeDelete);
ECS_COMPONENT_DEFINE(Name);
ECS_COMPONENT_DEFINE(Disabled);
ECS_COMPONENT_DEFINE(Abstract);

void ecs_bootstrap() {
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create((ecs_type_t){ 0 });
    ecs_vec_push_u64(&ecs_world.entity_index.entities, 0);
    ecs_component({ .name = "Invalid" });

    // Register the ecs_entity_t struct reflection.
    sireflect_register_struct(
        ecs_world.sireflect_registry,
        &(sireflect_struct_desc_t){
            .name = "ecs_entity_t",
            .fields = "{ uint32_t id; uint32_t generation; }",
            .size = sizeof(ecs_entity_t),
            .align = _Alignof(ecs_entity_t),
        }
    );

    ECS_COMPONENT_REGISTER(ChildOf);
    ECS_COMPONENT_REGISTER(Name);
    ECS_COMPONENT_REGISTER(Disabled);
    ECS_COMPONENT_REGISTER(Abstract);

    init_rest();
}

#ifndef SIECS_TABLE_MIGRATION_H
#define SIECS_TABLE_MIGRATION_H
#include <stdint.h>

#define ECS_ADD_PLAN_MAX_COMPONENTS 64

ecs_type_t ecs_type_with_requirements(
        ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec
);

#ifndef NDEBUG
bool ecs_component_requires(
    const     ecs_component_t component,
    ecs_component_t require
);
#endif

void ecs_migrate_to_table(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
);

void *ecs_migrate_add(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    uint16_t to_table_id,
    ecs_component_t added_id
);

void *ecs_migrate_add_many(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    uint16_t to_table_id,
    ecs_component_t requested_id
);

void ecs_migrate_remove(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id,
    uint16_t col_idx
);

#endif

#ifndef SIECS_UTILS_H
#define SIECS_UTILS_H
#ifndef NDEBUG
#include <stdio.h>
#include <stdlib.h>
#define ecs_cid_valid(id) ((id) != 0)
#define ecs_entity_valid(entity) (ecs_first(entity) != 0)

#define ecs_assert(condition, ...) \
    if (!(condition)) { \
        fprintf(stderr, __VA_ARGS__); \
        abort(); \
    }

#define ecs_assert_id_valid(id) ecs_assert(ecs_cid_valid(id), "invalid id: %d, id must be registered\n", id)
#define ecs_assert_not_null(ptr) ecs_assert((ptr) != NULL, "null pointer: %s\n", #ptr)
#define ecs_assert_entity_valid(entity) ecs_assert(ecs_entity_valid(entity), "invalid entity: %d, entity must be registered\n", ecs_first(entity))
#define ecs_assert_is_alive(entity) ecs_assert(ecs_is_alive(entity), "entity is dead: %d\n", ecs_first(entity))

#else
#define ecs_assert(condition, ...)
#define ecs_assert_id_valid(id)
#define ecs_assert_not_null(ptr)
#define ecs_assert_entity_valid(entity)
#define ecs_assert_is_alive(entity)
#endif

#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ECS_COMMAND_NONE UINT32_MAX

static inline void id_vec_push_unique(ecs_vec_t *vec, ecs_component_t id) {
    if (!ecs_vec_contains_u16(vec, id)) {
        ecs_vec_push_u16(vec, id);
    }
}

static inline void deferred_set_fini(ecs_deferred_set_t *set) {
    if (!set->data) {
        return;
    }
    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, set->id);
    ecs_component_value_dtor(record, set->data, 1);
    set->data = NULL;
}

static inline void set_vec_remove(ecs_vec_t *vec, ecs_component_t id) {
    ecs_deferred_set_t *sets = ecs_vec_data(vec, ecs_deferred_set_t);
    for (uint32_t i = 0; i < vec->size; i++) {
        if (sets[i].id == id) {
            deferred_set_fini(&sets[i]);
            ecs_vec_remove_fast(vec, i, sizeof(ecs_deferred_set_t));
            return;
        }
    }
}

static inline ecs_deferred_set_t *set_vec_find(ecs_vec_t *vec, ecs_component_t id) {
    ecs_deferred_set_t *sets = ecs_vec_data(vec, ecs_deferred_set_t);
    for (uint32_t i = 0; i < vec->size; i++) {
        if (sets[i].id == id) {
            return &sets[i];
        }
    }
    return NULL;
}

static inline void command_init(ecs_entity_command_t *command, ecs_entity_t entity) {
    *command = (ecs_entity_command_t){ .entity = entity };
    ecs_vec_init(&command->add_ids, sizeof(ecs_component_t));
    ecs_vec_init(&command->remove_ids, sizeof(ecs_component_t));
    ecs_vec_init(&command->sets, sizeof(ecs_deferred_set_t));
}

static inline void command_fini(ecs_entity_command_t *command) {
    ecs_deferred_set_t *sets = ecs_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size; i++) {
        deferred_set_fini(&sets[i]);
    }
    ecs_vec_fini(&command->add_ids);
    ecs_vec_fini(&command->remove_ids);
    ecs_vec_fini(&command->sets);
}

void ecs_command_buffer_init(ecs_command_buffer_t *buffer) {
    ecs_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));
    buffer->entity_to_command = NULL;
    buffer->entity_capacity = 0;
}

void ecs_command_buffer_fini(ecs_command_buffer_t *buffer) {
    ecs_entity_command_t *commands = ecs_vec_data(&buffer->commands, ecs_entity_command_t);
    for (uint32_t i = 0; i < buffer->commands.size; i++) {
        command_fini(&commands[i]);
    }
    ecs_vec_fini(&buffer->commands);
    free(buffer->entity_to_command);
}

static void command_buffer_ensure_entity(ecs_command_buffer_t *buffer, uint32_t entity_id) {
    if (entity_id < buffer->entity_capacity) {
        return;
    }

    uint32_t new_capacity = buffer->entity_capacity ? buffer->entity_capacity : 256;
    while (new_capacity <= entity_id) {
        new_capacity *= 2;
    }

    buffer->entity_to_command = realloc(buffer->entity_to_command, sizeof(uint32_t) * new_capacity);
    for (uint32_t i = buffer->entity_capacity; i < new_capacity; i++) {
        buffer->entity_to_command[i] = ECS_COMMAND_NONE;
    }
    buffer->entity_capacity = new_capacity;
}

static ecs_entity_command_t *command_for_entity(ecs_entity_t entity) {
    ecs_command_buffer_t *buffer = &ecs_world.commands;
    uint32_t entity_id = ecs_first(entity);
    command_buffer_ensure_entity(buffer, entity_id);

    uint32_t command_index = buffer->entity_to_command[entity_id];
    if (command_index != ECS_COMMAND_NONE) {
        return ecs_vec_get_mut(&buffer->commands, command_index, ecs_entity_command_t);
    }

    command_index = buffer->commands.size;
    ecs_entity_command_t *command =
        ecs_vec_push_empty(&buffer->commands, sizeof(ecs_entity_command_t));
    command_init(command, entity);
    buffer->entity_to_command[entity_id] = command_index;
    return command;
}

void ecs_command_buffer_add(ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_command_t *command = command_for_entity(entity);
    ecs_vec_remove_u16(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);
}

void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_command_t *command = command_for_entity(entity);
    ecs_vec_remove_u16(&command->add_ids, id);
    set_vec_remove(&command->sets, id);
    id_vec_push_unique(&command->remove_ids, id);
}

void ecs_command_buffer_set(
        ecs_entity_t entity,
    ecs_component_t id,
    const void *data
) {
    ecs_entity_command_t *command = command_for_entity(entity);
    const ecs_component_record_t *record = ecs_component_index_get(&ecs_world.component_index, id);

    ecs_vec_remove_u16(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);

    ecs_deferred_set_t *set = set_vec_find(&command->sets, id);
    if (set) {
        ecs_component_value_dtor(record, set->data, 1);
        ecs_component_value_copy_ctor(record, set->data, data, 1);
        return;
    }

    uint32_t size = record->size ? record->size : 1;
    void *copy = ecs_arena_alloc(&ecs_world.arena_allocator, size);
    ecs_component_value_copy_ctor(record, copy, data, 1);
    ecs_deferred_set_t new_set = { .id = id, .data = copy };
    ecs_vec_push(&command->sets, &new_set, sizeof(ecs_deferred_set_t));
}

void ecs_command_buffer_move(
        ecs_entity_t entity,
    ecs_component_t id,
    void *data
) {
    ecs_entity_command_t *command = command_for_entity(entity);
    const ecs_component_record_t *record = ecs_component_index_get(&ecs_world.component_index, id);

    ecs_vec_remove_u16(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);

    ecs_deferred_set_t *set = set_vec_find(&command->sets, id);
    if (set) {
        ecs_component_value_dtor(record, set->data, 1);
        ecs_component_value_move_ctor(record, set->data, data, 1);
        return;
    }

    uint32_t size = record->size ? record->size : 1;
    void *copy = ecs_arena_alloc(&ecs_world.arena_allocator, size);
    ecs_component_value_move_ctor(record, copy, data, 1);
    ecs_deferred_set_t new_set = { .id = id, .data = copy };
    ecs_vec_push(&command->sets, &new_set, sizeof(ecs_deferred_set_t));
}

void ecs_command_buffer_kill(ecs_entity_t entity) {
    ecs_entity_command_t *command = command_for_entity(entity);
    command->kill = true;
    command->has_base = false;
    ecs_vec_clear(&command->add_ids);
    ecs_vec_clear(&command->remove_ids);
    ecs_deferred_set_t *sets = ecs_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size; i++) {
        deferred_set_fini(&sets[i]);
    }
    ecs_vec_clear(&command->sets);
}

void ecs_command_buffer_set_base(ecs_entity_t entity, ecs_entity_t target) {
    ecs_entity_command_t *command = command_for_entity(entity);
    command->has_base = true;
    command->base = target;
}

static void final_ids_push_sorted(ecs_vec_t *final_ids, ecs_component_t id) {
    ecs_component_t *ids = ecs_vec_data(final_ids, ecs_component_t);
    uint32_t i = 0;
    while (i < final_ids->size && ids[i] < id) {
        i++;
    }
    if (i < final_ids->size && ids[i] == id) {
        return;
    }

    ecs_vec_push_empty(final_ids, sizeof(ecs_component_t));
    ids = ecs_vec_data(final_ids, ecs_component_t);
    for (uint32_t j = final_ids->size - 1; j > i; j--) {
        ids[j] = ids[j - 1];
    }
    ids[i] = id;
}

static void
final_ids_collect_requirements(ecs_vec_t *final_ids, ecs_component_t id) {
    const ecs_component_record_t *record = ecs_component_index_get(&ecs_world.component_index, id);
    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t required = record->required[i];
        if (ecs_vec_contains_u16(final_ids, required)) {
            continue;
        }
        final_ids_collect_requirements(final_ids, required);
        final_ids_push_sorted(final_ids, required);
    }
}

static ecs_type_t command_build_type(
        const ecs_table_t *table,
    const ecs_entity_command_t *command
) {
    ecs_vec_t final_ids;
    ecs_vec_init(&final_ids, sizeof(ecs_component_t));

    for (uint16_t i = 0; i < table->type.count; i++) {
        ecs_component_t id = table->type.ids[i];
        if (!ecs_vec_contains_u16(&command->remove_ids, id)) {
            ecs_vec_push_u16(&final_ids, id);
        }
    }

    const ecs_component_t *adds = ecs_vec_data(&command->add_ids, ecs_component_t);
    for (uint32_t i = 0; i < command->add_ids.size; i++) {
        final_ids_collect_requirements(&final_ids, adds[i]);
        final_ids_push_sorted(&final_ids, adds[i]);
    }

    return (ecs_type_t){
        .ids = ecs_vec_data(&final_ids, ecs_component_t),
        .count = (uint16_t)final_ids.size,
        .base = command->has_base ? command->base : table->type.base,
    };
}

static void command_emit_removed(
        ecs_table_t *table,
    ecs_entity_t entity,
    uint32_t row,
    const ecs_type_t *final_type
) {
    uint16_t final_i = 0;
    for (uint16_t i = 0; i < table->type.count; i++) {
        ecs_component_t id = table->type.ids[i];
        while (final_i < final_type->count && final_type->ids[final_i] < id) {
            final_i++;
        }
        if (final_i < final_type->count && final_type->ids[final_i] == id) {
            continue;
        }

        void *data = ecs_table_component_at_column(table, i, row);
        const ecs_component_record_t *record = ecs_component_index_get(&ecs_world.component_index, id);
        if (record->on_remove) {
            record->on_remove(entity, id, data);
        }
        ecs_emit(table, entity, EcsOnRemove, data);
    }
}

static void command_emit_added(
        const ecs_table_t *old_table,
    ecs_table_t *new_table,
    ecs_entity_t entity,
    uint32_t row
) {
    uint16_t old_i = 0;
    for (uint16_t new_i = 0; new_i < new_table->type.count; new_i++) {
        ecs_component_t id = new_table->type.ids[new_i];
        while (old_i < old_table->type.count && old_table->type.ids[old_i] < id) {
            old_i++;
        }
        if (old_i < old_table->type.count && old_table->type.ids[old_i] == id) {
            continue;
        }

        void *data = ecs_table_component_at_column(new_table, new_i, row);
        const ecs_component_record_t *record = ecs_component_index_get(&ecs_world.component_index, id);
        if (record->on_add) {
            record->on_add(entity, id, data);
        }
        ecs_emit(new_table, entity, EcsOnAdd, data);
    }
}

static bool command_type_unchanged(const ecs_table_t *table, const ecs_entity_command_t *command) {
    if (command->remove_ids.size != 0) {
        return false;
    }
    if (command->has_base && command->base != table->type.base) {
        return false;
    }

    const ecs_component_t *adds = ecs_vec_data(&command->add_ids, ecs_component_t);
    for (uint32_t i = 0; i < command->add_ids.size; i++) {
        if (!ecs_table_has_owned(table, adds[i])) {
            return false;
        }
    }
    return true;
}

static void command_apply_sets(ecs_entity_command_t *command) {
    ecs_deferred_set_t *sets = ecs_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size && ecs_is_alive(command->entity); i++) {
        ecs_component_t id = sets[i].id;
        const ecs_component_record_t *record = ecs_component_index_get(&ecs_world.component_index, id);
        ecs_entity_record_t *entity_record = ecs_get_record(command->entity);
        ecs_table_t *table = ecs_get_table(entity_record->table_id);
        uint16_t column = ecs_table_get_column_index(table, id);
        void *dst = ecs_table_component_at_column(table, column, entity_record->table_row);

        if (record->on_set) {
            record->on_set(command->entity, id, sets[i].data, dst);
            if (!ecs_is_alive(command->entity)) {
                return;
            }
            entity_record = ecs_get_record(command->entity);
            table = ecs_get_table(entity_record->table_id);
            column = ecs_table_get_column_index(table, id);
            dst = ecs_table_component_at_column(table, column, entity_record->table_row);
        }
        ecs_emit(table, command->entity, EcsOnSet, sets[i].data);
        ecs_component_value_move(record, dst, sets[i].data, 1);
        sets[i].data = NULL;
    }
}

static void command_apply(ecs_entity_command_t *command) {
    if (!ecs_is_alive(command->entity)) {
        return;
    }

    if (command->kill) {
        ecs_kill_now(command->entity);
        return;
    }

    ecs_entity_record_t *record = ecs_get_record(command->entity);
    uint16_t old_table_id = record->table_id;
    ecs_table_t *old_table = ecs_get_table(old_table_id);
    if (command_type_unchanged(old_table, command)) {
        command_apply_sets(command);
        return;
    }

    ecs_type_t final_type = command_build_type(old_table, command);

    if (!ecs_type_equals(&old_table->type, &final_type)) {
        uint32_t old_row = record->table_row;
        command_emit_removed(old_table, command->entity, old_row, &final_type);
        if (!ecs_is_alive(command->entity)) {
            ecs_type_fini(&final_type);
            return;
        }

        uint16_t new_table_id = ecs_table_index_get_or_create(final_type);
        record = ecs_get_record(command->entity);
        old_table = ecs_get_table(old_table_id);
        const ecs_table_t *emit_old_table = old_table;
        ecs_migrate_to_table(record, command->entity, old_table, new_table_id);
        record = ecs_get_record(command->entity);
        ecs_table_t *new_table = ecs_get_table(record->table_id);
        command_emit_added(emit_old_table, new_table, command->entity, record->table_row);
    } else {
        ecs_type_fini(&final_type);
    }

    command_apply_sets(command);
}

void ecs_command_buffer_flush() {
    ecs_command_buffer_t *buffer = &ecs_world.commands;
    if (buffer->commands.size == 0) {
        ecs_arena_reset(&ecs_world.arena_allocator);
        return;
    }

    ecs_world.flushing_commands = true;
    while (buffer->commands.size != 0) {
        ecs_vec_t commands = buffer->commands;
        ecs_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));

        ecs_entity_command_t *items = ecs_vec_data(&commands, ecs_entity_command_t);
        for (uint32_t i = 0; i < commands.size; i++) {
            uint32_t entity_id = ecs_first(items[i].entity);
            buffer->entity_to_command[entity_id] = ECS_COMMAND_NONE;
        }

        for (uint32_t i = 0; i < commands.size; i++) {
            command_apply(&items[i]);
            command_fini(&items[i]);
        }
        ecs_vec_fini(&commands);
    }
    ecs_world.flushing_commands = false;
    ecs_arena_reset(&ecs_world.arena_allocator);
}

void ecs_defer_begin(void) {
        ecs_world.defer_depth++;
}

void ecs_defer_end(void) {
        ecs_assert(ecs_world.defer_depth > 0, "ecs_defer_end called without ecs_defer_begin\n");
    ecs_world.defer_depth--;
    if (ecs_world.defer_depth == 0) {
        ecs_command_buffer_flush();
    }
}

bool ecs_is_deferred(void) {
        return ecs_world.defer_depth != 0 || ecs_world.flushing_commands;
}

#include <stdio.h>
#ifndef SIREFLECT_H
#endif

static ecs_component_t ecs_component_alloc_ids(uint16_t count) {
    uint32_t id = ecs_world.component_index.components.size;
    if (id == 0) id = 1;
    ecs_assert(id + count <= UINT16_MAX, "component id overflow\n");
    return id;
}

void RelationOnSet(
        ecs_entity_t entity,
    ecs_component_t target_component,
    const void *new_value,
    void *current_value
) {
    const RelationTarget *target_data = new_value;
    ecs_component_t source_component = target_component + 1;

    const RelationTarget *old_target_data = current_value;

    ecs_assert_entity_valid(target_data->target);
    ecs_assert_is_alive(target_data->target);

    if (old_target_data->target == target_data->target) {
        return;
    }

    if (old_target_data->target) {
        RelationSource *source = ecs_get_cid(old_target_data->target, source_component);

        ecs_vec_remove_u64(&source->entities, entity);
        if (source->entities.size == 0) {
            ecs_remove_cid(old_target_data->target, source_component);
        }
    }

    if (ecs_has_cid(target_data->target, source_component)) {
        RelationSource *source_data = ecs_get_cid(target_data->target, source_component);
        ecs_vec_push_u64(&source_data->entities, entity);
    } else {
        RelationSource source_data = {};
        ecs_vec_init(&source_data.entities, sizeof(ecs_entity_t));
        ecs_vec_push_u64(&source_data.entities, entity);
        ecs_set_cid(target_data->target, source_component, &source_data);
    }
}

void RelationOnRemove(
        ecs_entity_t entity,
    ecs_component_t component,
    void *ptr
) {
    const RelationTarget *target_data = ptr;
    ecs_component_t source_component = component + 1;
    RelationSource *target_source_data = ecs_get_cid(target_data->target, source_component);

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    if (target_source_data->entities.size == UINT32_MAX) {
        return;
    }

    ecs_vec_remove_u64(&target_source_data->entities, entity);

    if (target_source_data->entities.size == 0) {
        ecs_remove_cid(target_data->target, source_component);
    }
}

void RelationSourceOnRemove(
        ecs_entity_t,
    ecs_component_t component,
    void *ptr
) {
    RelationSource *source_data = ptr;

    const ecs_entity_t *entities = source_data->entities.data;
    const uint32_t count = source_data->entities.size;
    const ecs_component_record_t *crec =
        ecs_component_index_get(&ecs_world.component_index, component);
    const bool cascade_delete = crec->relation_flags & EcsRelationCascadeDelete;

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    source_data->entities.size = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        if (cascade_delete) {
            ecs_kill(entities[i]);
        } else {
            ecs_remove_cid(entities[i], component - 1);
        }
    }

    ecs_vec_fini(&source_data->entities);
}

ecs_component_t
ecs_component_register(ecs_component_t *id, const ecs_component_desc_t *desc) {
    ecs_assert_not_null(id);
    ecs_assert_not_null(desc);

    if (*id != 0 && *id < ecs_world.component_index.components.size) {
        const ecs_component_record_t *existing =
            ecs_component_index_get(&ecs_world.component_index, *id);
        if (existing->tables.data) {
            return *id;
        }
    }

    sireflect_handle_t reflection = SIREFLECT_INVALID_HANDLE;

    if (ECS_LIKELY(desc->struct_desc)) {
        reflection = sireflect_try_register_struct(ecs_world.sireflect_registry, desc->struct_desc);

        if (ECS_UNLIKELY(reflection == SIREFLECT_INVALID_HANDLE)) {
            puts(sireflect_error());
        }
    }

    if (ECS_UNLIKELY(desc->relation_flags & EcsRelationTarget)) {
        if (*id == 0) {
            *id = ecs_component_alloc_ids(2);
        }

        ecs_component_t component = *id;
        ecs_component_index_register(
            &ecs_world.component_index,
            component,
            desc->size,
            desc->ops,
            RelationOnSet,
            RelationOnRemove,
            desc->on_add,
            desc->relation_flags,
            reflection,
            desc->struct_desc
        );

        ecs_component_t source = component + 1;
        ecs_component_index_register(
            &ecs_world.component_index,
            source,
            desc->relation_flags & EcsRelationOneToOne ? sizeof(RelationTarget)
                                                       : sizeof(RelationSource),
            (ecs_type_ops_t){ 0 },
            NULL,
            RelationSourceOnRemove,
            desc->on_add,
            (desc->relation_flags & ~EcsRelationTarget) | EcsRelationSource,
            SIREFLECT_INVALID_HANDLE,
            NULL
        );
        return component;
    } else {
        if (*id == 0) {
            *id = ecs_component_alloc_ids(1);
        }

        ecs_component_t component = *id;
        ecs_component_index_register(
            &ecs_world.component_index,
            component,
            desc->size,
            desc->ops,
            desc->on_set,
            desc->on_remove,
            desc->on_add,
            0,
            reflection,
            desc->struct_desc
        );
        return component;
    }
}

ecs_component_t ecs_component_init(const ecs_component_desc_t *desc) {
    ecs_component_t id = 0;
    return ecs_component_register(&id, desc);
}

#define ecs_assert_can_be_updated(entity, ...)                                              \
    ecs_assert(!ecs_has_cid_owned(entity, ecs_id(Abstract)), __VA_ARGS__)

static void ecs_emit_added_components(
        ecs_table_t *from_table,
    ecs_table_t *to_table,
    ecs_entity_t entity,
    uint32_t row
) {
    uint16_t from_i = 0;
    for (uint16_t to_i = 0; to_i < to_table->type.count; to_i++) {
        ecs_component_t added = to_table->type.ids[to_i];
        while (from_i < from_table->type.count && from_table->type.ids[from_i] < added) {
            from_i++;
        }
        if (from_i < from_table->type.count && from_table->type.ids[from_i] == added) {
            continue;
        }

        void *data = ecs_table_component_at_column(to_table, to_i, row);
        const ecs_component_record_t *crec =
            ecs_component_index_get(&ecs_world.component_index, added);
        if (crec->on_add) {
            crec->on_add(entity, added, data);
        }
        ecs_emit(to_table, entity, EcsOnAdd, data);
    }
}

void ecs_add_cid_now(ecs_entity_t entity, ecs_component_t cid) {
        ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    ecs_assert_can_be_updated(entity, "An abstract entity cannot be updated.");

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(from_id);
    uint16_t edge = ecs_table_get_add_edge(table, cid);

    if (ECS_UNLIKELY(edge < table->type.count && table->type.ids[edge] == cid)) {
        return;
    }

    const ecs_component_record_t *crec = ecs_component_index_get(&ecs_world.component_index, cid);

    if (crec->required_count == 0) {
        if (edge == UINT16_MAX) {
            ecs_type_t new_type = ecs_type_with_add(&table->type, cid);
            edge = ecs_table_index_get_or_create(new_type);

            table = ecs_get_table(from_id);
            ecs_id_map_set(&table->add_edge, cid, edge);
        }

        ecs_table_t *new_table = ecs_get_table(edge);
        void *component_data = ecs_migrate_add(record, entity, table, new_table, edge, cid);

        if (crec->on_add) {
            crec->on_add(entity, cid, component_data);
        }
        ecs_emit(new_table, entity, EcsOnAdd, component_data);
        return;
    }

    if (edge == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_requirements(table, cid, crec);
        edge = ecs_table_index_get_or_create(new_type);

        table = ecs_get_table(from_id);
        ecs_id_map_set(&table->add_edge, cid, edge);
    }

    ecs_table_t *new_table = ecs_get_table(edge);
    bool add_many = new_table->type.count > table->type.count + 1;

    void *component_data =
        add_many ? ecs_migrate_add_many(record, entity, table, new_table, edge, cid)
                 : ecs_migrate_add(record, entity, table, new_table, edge, cid);

    if (add_many) {
        ecs_emit_added_components(table, new_table, entity, record->table_row);
        return;
    }
    if (crec->on_add) {
        crec->on_add(entity, cid, component_data);
    }

    ecs_emit(new_table, entity, EcsOnAdd, component_data);
}

void ecs_add_cid(ecs_entity_t entity, ecs_component_t cid) {
        ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    ecs_assert_can_be_updated(entity, "An abstract entity cannot be updated.");

    if (ecs_is_deferred()) {
        ecs_command_buffer_add(entity, cid);
        return;
    }

    ecs_add_cid_now(entity, cid);
}

void ecs_remove_cid_now(ecs_entity_t entity, ecs_component_t cid) {
        ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(from_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);

    if (ECS_UNLIKELY(col_idx == UINT16_MAX)) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_remove_at(&table->type, col_idx);
        new_table_id = ecs_table_index_get_or_create(new_type);
        table = ecs_get_table(from_id);
        table->cls[col_idx].remove_edge = new_table_id;
    }

    void *removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);

    const ecs_component_record_t *crec = ecs_component_index_get(&ecs_world.component_index, cid);
    if (crec->on_remove) {
        crec->on_remove(entity, cid, removed_data);
        table = ecs_get_table(from_id);
    }
    ecs_emit(table, entity, EcsOnRemove, removed_data);

    ecs_migrate_remove(record, entity, table, new_table_id, (uint16_t)col_idx);
}

void ecs_remove_cid(ecs_entity_t entity, ecs_component_t cid) {
        ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_remove(entity, cid);
        return;
    }

    ecs_remove_cid_now(entity, cid);
}

void *ecs_get_cid(ecs_entity_t entity, ecs_component_t cid) {
        ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    const ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);
    if (col_idx != UINT16_MAX) {
        return ecs_table_component_at_column(table, col_idx, record->table_row);
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *base_record = ecs_get_record(base);
        ecs_table_t *base_table = ecs_get_table(base_record->table_id);

        col_idx = ecs_table_column_or_invalid(base_table, cid);
        if (col_idx != UINT16_MAX) {
            return ecs_table_component_at_column(base_table, col_idx, base_record->table_row);
        }

        base = base_table->type.base;
    }

    return NULL;
}

void *ecs_try_get_cid(ecs_entity_t entity, ecs_component_t cid) {
        ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    const ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);
    if (col_idx != UINT16_MAX) {
        return ecs_table_component_at_column(table, col_idx, record->table_row);
    }
    return NULL;
}

void ecs_set_cid_now(ecs_entity_t entity, ecs_component_t cid, const void *data) {
        ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_add_cid_now(entity, cid);
    const ecs_component_record_t *crec = ecs_component_index_get(&ecs_world.component_index, cid);
    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    uint16_t col_idx = ecs_table_get_column_index(table, cid);
    void *dst = ecs_table_component_at_column(table, col_idx, record->table_row);

    if (crec->on_set) {
        crec->on_set(entity, cid, data, dst);
        record = ecs_get_record(entity);
        table = ecs_get_table(record->table_id);
        col_idx = ecs_table_get_column_index(table, cid);
        dst = ecs_table_component_at_column(table, col_idx, record->table_row);
    }
    ecs_emit(table, entity, EcsOnSet, data);
    ecs_component_value_copy(crec, dst, data, 1);
}

void ecs_set_cid(ecs_entity_t entity, ecs_component_t cid, const void *data) {
        ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_set(entity, cid, data);
        return;
    }

    ecs_set_cid_now(entity, cid, data);
}

void ecs_move_cid_now(ecs_entity_t entity, ecs_component_t cid, void *data) {
        ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    bool had_value = ecs_has_cid_owned(entity, cid);
    ecs_add_cid_now(entity, cid);
    const ecs_component_record_t *crec = ecs_component_index_get(&ecs_world.component_index, cid);
    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    uint16_t col_idx = ecs_table_get_column_index(table, cid);
    void *dst = ecs_table_component_at_column(table, col_idx, record->table_row);

    if (crec->on_set) {
        crec->on_set(entity, cid, data, dst);
        record = ecs_get_record(entity);
        table = ecs_get_table(record->table_id);
        col_idx = ecs_table_get_column_index(table, cid);
        dst = ecs_table_component_at_column(table, col_idx, record->table_row);
    }
    ecs_emit(table, entity, EcsOnSet, data);
    if (had_value || crec->ops.ctor) {
        ecs_component_value_move(crec, dst, data, 1);
    } else {
        ecs_component_value_move_ctor(crec, dst, data, 1);
    }
}

void ecs_move_cid(ecs_entity_t entity, ecs_component_t cid, void *data) {
        ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_move(entity, cid, data);
        return;
    }

    ecs_move_cid_now(entity, cid, data);
}

bool ecs_has_cid(const ecs_entity_t entity, ecs_component_t id) {
        ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    uint16_t tid = ecs_get_record(entity)->table_id;
    return ecs_table_has(ecs_get_table(tid), id);
}

bool ecs_has_cid_owned(const ecs_entity_t entity, ecs_component_t id) {
        ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    uint16_t tid = ecs_get_record(entity)->table_id;
    return ecs_table_has_owned(ecs_get_table(tid), id);
}

void ecs_with(ecs_component_t component, ecs_component_t require) {
        ecs_assert_id_valid(component);
    ecs_assert_id_valid(require);
    ecs_assert(component != require, "component cannot require itself: %d\n", component);
#ifndef NDEBUG
    ecs_assert(
        !ecs_component_requires(require, component),
        "cyclic component requirement: %d requires %d\n",
        component,
        require
    );
#endif

    ecs_component_record_t *record =
        ecs_component_index_get_mut(&ecs_world.component_index, component);

    ecs_assert(record->tables.size == 0, "component already used cannot register requirement");

#ifndef NDEBUG
    for (uint32_t i = 0; i < record->required_count; i++) {
        if (record->required[i] == require) {
            ecs_assert(true, "required component already registered");
        }
    }
#endif

    record->required =
        realloc(record->required, sizeof(ecs_component_t) * (record->required_count + 1));
    record->required[record->required_count++] = require;
}

#include <stdbool.h>

typedef struct {
    ecs_component_t ids[ECS_ADD_PLAN_MAX_COMPONENTS];
    uint16_t count;
} ecs_add_plan_t;

static inline bool ecs_add_plan_has(const ecs_add_plan_t *plan, ecs_component_t id) {
    for (uint16_t i = 0; i < plan->count; i++) {
        if (plan->ids[i] == id) {
            return true;
        }
    }
    return false;
}

static inline void ecs_add_plan_push(ecs_add_plan_t *plan, ecs_component_t id) {
#ifndef NDEBUG
    if (plan->count == ECS_ADD_PLAN_MAX_COMPONENTS) {
        abort();
    }
#endif
    plan->ids[plan->count++] = id;
}

static inline void ecs_add_plan_collect_requirements(
        ecs_table_t *from_table,
    ecs_add_plan_t *plan,
    const ecs_component_record_t *crec
) {
    for (uint32_t i = 0; i < crec->required_count; i++) {
        ecs_component_t required = crec->required[i];
        if (ecs_table_has_owned(from_table, required) || ecs_add_plan_has(plan, required)) {
            continue;
        }

        const ecs_component_record_t *required_rec =
            ecs_component_index_get(&ecs_world.component_index, required);
        if (required_rec->required_count) {
            ecs_add_plan_collect_requirements(from_table, plan, required_rec);
        }
        ecs_add_plan_push(plan, required);
    }
}

static inline void ecs_sort_component_ids(ecs_component_t *ids, uint16_t count) {
    for (uint16_t i = 1; i < count; i++) {
        ecs_component_t id = ids[i];
        uint16_t j = i;
        while (j > 0 && ids[j - 1] > id) {
            ids[j] = ids[j - 1];
            j--;
        }
        ids[j] = id;
    }
}

ecs_type_t ecs_type_with_requirements(
        ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec
) {
    ecs_add_plan_t plan = { 0 };
    ecs_add_plan_collect_requirements(from_table, &plan, crec);
    ecs_add_plan_push(&plan, cid);
    ecs_sort_component_ids(plan.ids, plan.count);

    ecs_type_t type = {
        .ids = malloc(sizeof(ecs_component_t) * (from_table->type.count + plan.count)),
        .count = from_table->type.count + plan.count,
        .base = from_table->type.base,
    };

    uint16_t from_i = 0;
    uint16_t add_i = 0;
    uint16_t out_i = 0;
    while (from_i < from_table->type.count && add_i < plan.count) {
        ecs_component_t from_id = from_table->type.ids[from_i];
        ecs_component_t add_id = plan.ids[add_i];
        if (from_id < add_id) {
            type.ids[out_i++] = from_id;
            from_i++;
        } else {
            type.ids[out_i++] = add_id;
            add_i++;
        }
    }
    while (from_i < from_table->type.count) {
        type.ids[out_i++] = from_table->type.ids[from_i++];
    }
    while (add_i < plan.count) {
        type.ids[out_i++] = plan.ids[add_i++];
    }

    return type;
}

#ifndef NDEBUG
bool ecs_component_requires(
    const     ecs_component_t component,
    ecs_component_t require
) {
    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component);

    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t current = record->required[i];
        if (current == require || ecs_component_requires(current, require)) {
            return true;
        }
    }

    return false;
}
#endif

ecs_entity_t ecs_new(void) {
        ecs_table_t *table = ecs_get_table(0);

    ecs_entity_t entity = ecs_entity_index_create(&ecs_world.entity_index, table->entity_count);
    ecs_table_add_entity(table, entity);

    return entity;
}

bool ecs_is_alive(const ecs_entity_t entity) {
    return ecs_entity_index_is_alive(&ecs_world.entity_index, entity);
}

#ifndef NDEBUG
static inline bool
ecs_would_create_base_cycle(const ecs_entity_t entity, ecs_entity_t target) {
    while (target != 0) {
        if (target == entity) {
            return true;
        }
        const ecs_entity_record_t *target_record = ecs_get_record(target);
        const ecs_table_t *target_table = ecs_get_table(target_record->table_id);
        target = target_table->type.base;
    }
    return false;
}
#endif

static inline void ecs_entity_rebase(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);
    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    for (uint16_t i = 0; i < from_table->data_count; i++) {
        uint16_t col = from_table->data_columns[i];
        ecs_component_t component = from_table->type.ids[col];
        const ecs_component_record_t *crec =
            ecs_component_index_get(&ecs_world.component_index, component);
        void *src = ecs_table_component_at_column(from_table, col, old_row);
        void *dst = ecs_table_component_at_column(to_table, col, new_row);
        ecs_component_value_move_ctor(crec, dst, src, 1);
    }

    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row, false);
    if (moved != entity) {
        ecs_get_record(moved)->table_row = old_row;
    }

    record->table_id = to_table_id;
    record->table_row = new_row;
}

bool ecs_is(ecs_entity_t entity, ecs_entity_t target) {
    ecs_entity_t base = ecs_get_table(ecs_get_record(entity)->table_id)->type.base;
    if (base == target) {
        return true;
    }
    if (base == 0) {
        return false;
    }
    return ecs_is(base, target);
}

void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target) {
        ecs_assert_entity_valid(entity);
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(entity);
    ecs_assert_is_alive(target);
    ecs_assert(entity != target, "entity cannot inherit itself: %d\n", ecs_first(entity));
    ecs_assert(
        !ecs_would_create_base_cycle(entity, target),
        "cyclic inheritance: %d inherits from %d\n",
        ecs_first(entity),
        ecs_first(target)
    );
    ecs_assert(
        ecs_has_cid_owned(target, ecs_id(Abstract)),
        "An entity can only inherit from an abstract entity."
    );

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_table_id = record->table_id;
    ecs_table_t *from_table = ecs_get_table(from_table_id);
    if (from_table->type.base == target) {
        return;
    }

    ecs_type_t new_type = ecs_type_with_base(&from_table->type, target);
    uint16_t to_table_id = ecs_table_index_get_or_create(new_type);
    if (to_table_id == from_table_id) {
        return;
    }

    from_table = ecs_get_table(from_table_id);
    ecs_entity_rebase(record, entity, from_table, to_table_id);
}

void ecs_is_a(ecs_entity_t entity, ecs_entity_t target) {
        ecs_assert_entity_valid(entity);
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(entity);
    ecs_assert_is_alive(target);

    if (ecs_is_deferred()) {
        ecs_command_buffer_set_base(entity, target);
        return;
    }

    ecs_is_a_now(entity, target);
}

void ecs_kill_now(ecs_entity_t entity) {
        ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    uint16_t component_count = table->type.count;
    ecs_component_t *components = NULL;

    if (component_count != 0) {
        components = malloc(sizeof(ecs_component_t) * component_count);
        ecs_assert_not_null(components);
        for (uint16_t i = 0; i < component_count; i++) {
            components[i] = table->type.ids[i];
        }
    }

    for (uint16_t i = 0; i < component_count && ecs_is_alive(entity); i++) {
        ecs_component_t component = components[i];
        record = ecs_get_record(entity);
        table = ecs_get_table(record->table_id);

        uint16_t col_idx = ecs_table_column_or_invalid(table, component);
        if (col_idx == UINT16_MAX) {
            continue;
        }

        void *removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);
        const ecs_component_record_t *crec =
            ecs_component_index_get(&ecs_world.component_index, component);

        if (crec->on_remove) {
            crec->on_remove(entity, component, removed_data);
            if (!ecs_is_alive(entity)) {
                break;
            }
            record = ecs_get_record(entity);
            table = ecs_get_table(record->table_id);
            col_idx = ecs_table_column_or_invalid(table, component);
            if (col_idx == UINT16_MAX) {
                continue;
            }
            removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);
        }
        ecs_emit(table, entity, EcsOnRemove, removed_data);
    }

    free(components);
    if (!ecs_is_alive(entity)) {
        return;
    }

    record = ecs_get_record(entity);
    table = ecs_get_table(record->table_id);

    // Remove from table
    ecs_entity_t moved = ecs_table_remove_entity(table, record->table_row, true);
    if (moved != entity) {
        ecs_get_record(moved)->table_row = record->table_row;
    }

    ecs_entity_index_kill(&ecs_world.entity_index, ecs_first(entity));
}

void ecs_kill(ecs_entity_t entity) {
        ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_kill(entity);
        return;
    }

    ecs_kill_now(entity);
}

#ifndef SIECS_MODULE_H
#define SIECS_MODULE_H

void ecs_module_record_system(ecs_system_id_t system);
void ecs_module_record_observer(ecs_observer_id_t observer);

#endif

ecs_module_id_t ecs_module_init(const ecs_module_desc_t *desc) {
        ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);
    ecs_assert_not_null(desc->import);

    ecs_module_id_t existing = ecs_module_index_find(&ecs_world.module_index, desc->id);
    if (existing) {
        return existing;
    }

    ecs_module_id_t module = ecs_module_index_create(&ecs_world.module_index, desc->id, desc->name);
    if (desc->id) {
        *desc->id = module;
    }

    ecs_module_id_t prev = ecs_world.active_module;
    ecs_world.active_module = module;
    desc->import(desc->desc);
    ecs_world.active_module = prev;

    if (desc->disabled) {
        ecs_module_disable(module);
    }

    return module;
}

void ecs_module_enable(ecs_module_id_t module) {
    
    ecs_module_t *record = ecs_module_index_get(&ecs_world.module_index, module);
    if (record->enabled) {
        return;
    }

    const ecs_system_id_t *systems = ecs_vec_data(&record->systems, ecs_system_id_t);
    for (uint32_t i = 0; i < record->systems.size; i++) {
        ecs_system_enable(systems[i]);
    }

    const ecs_observer_id_t *observers = ecs_vec_data(&record->observers, ecs_observer_id_t);
    for (uint32_t i = 0; i < record->observers.size; i++) {
        ecs_observer_enable(observers[i]);
    }

    record->enabled = true;
}

ecs_module_id_t ecs_module_find(const ecs_module_id_t *id) {
    return ecs_module_index_find(&ecs_world.module_index, id);
}

void ecs_module_disable(ecs_module_id_t module) {
    
    ecs_module_t *record = ecs_module_index_get(&ecs_world.module_index, module);
    if (!record->enabled) {
        return;
    }

    const ecs_system_id_t *systems = ecs_vec_data(&record->systems, ecs_system_id_t);
    for (uint32_t i = 0; i < record->systems.size; i++) {
        ecs_system_disable(systems[i]);
    }

    const ecs_observer_id_t *observers = ecs_vec_data(&record->observers, ecs_observer_id_t);
    for (uint32_t i = 0; i < record->observers.size; i++) {
        ecs_observer_disable(observers[i]);
    }

    record->enabled = false;
}

bool ecs_module_is_enabled(const ecs_module_id_t module) {
        return ecs_module_index_get_const(&ecs_world.module_index, module)->enabled;
}

void ecs_module_record_system(ecs_system_id_t system) {
    ecs_module_id_t module = ecs_world.active_module;
    if (!module) {
        return;
    }

    ecs_module_t *record = ecs_module_index_get(&ecs_world.module_index, module);
    ecs_vec_push_u16(&record->systems, system);
}

void ecs_module_record_observer(ecs_observer_id_t observer) {
    ecs_module_id_t module = ecs_world.active_module;
    if (!module) {
        return;
    }

    ecs_module_t *record = ecs_module_index_get(&ecs_world.module_index, module);
    ecs_vec_push(&record->observers, &observer, sizeof(ecs_observer_id_t));
}

ecs_event_t ecs_event(void) { return ecs_world.observer_index.event_count++; }

ecs_event_t ecs_event_register(ecs_event_t *id) {
        ecs_assert_not_null(id);

    if (*id == UINT16_MAX) {
        *id = ecs_event();
        return *id;
    }

    if (ecs_world.observer_index.event_count <= *id) {
        ecs_world.observer_index.event_count = *id + 1;
    }

    return *id;
}

ecs_observer_id_t ecs_observer_init(const ecs_observer_desc_t *desc) {
        ecs_assert(desc->callback != NULL, "Observer callback cannot be NULL");
    ecs_observer_id_t oid = ecs_observer_index_create(&ecs_world.observer_index, desc);
    ecs_observer_index_match_tables(
                ecs_world.table_index.tables,
        ecs_world.table_index.table_count,
        oid
    );
    ecs_module_record_observer(oid);
    return oid;
}

void ecs_observer_enable(ecs_observer_id_t id) {
    ecs_vec_get_mut(&ecs_world.observer_index.observers, id, ecs_observer_t)->enabled = true;
}

void ecs_observer_disable(ecs_observer_id_t id) {
    ecs_vec_get_mut(&ecs_world.observer_index.observers, id, ecs_observer_t)->enabled = false;
}

void ecs_observer_trigger(
        ecs_entity_t entity,
    ecs_event_t event,
    const void *trigger_data
) {
        ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    ecs_emit(table, entity, event, trigger_data);
}

static void ecs_query_index_remove_active_id(ecs_query_index_t *index, ecs_query_id_t qid) {
    ecs_query_cache_t *cache = ecs_vec_get_mut(&index->queries, qid, ecs_query_cache_t);
    uint32_t active_index = cache->active_index;
    uint32_t last_index = index->active_ids.size - 1;

    if (active_index != last_index) {
        ecs_query_id_t moved = *ecs_vec_get(&index->active_ids, last_index, ecs_query_id_t);
        ((ecs_query_id_t *)index->active_ids.data)[active_index] = moved;
        ecs_vec_get_mut(&index->queries, moved, ecs_query_cache_t)->active_index = active_index;
    }

    ecs_vec_remove_last(&index->active_ids);
}

ecs_query_id_t ecs_query_init(const ecs_query_desc_t *desc) {
        ecs_query_id_t qid = ecs_query_index_create(&ecs_world.query_index, desc);
    ecs_query_index_update_matches(
                ecs_vec_get_mut(&ecs_world.query_index.queries, qid, ecs_query_cache_t)
    );
    return qid;
}

ecs_iter_t ecs_query_iter(ecs_query_id_t query_id) {
        ecs_assert(query_id < ecs_world.query_index.queries.size, "invalid query id: %u\n", query_id);

    ecs_query_cache_t *cache =
        ecs_vec_get_mut(&ecs_world.query_index.queries, query_id, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", query_id);
    return (ecs_iter_t){
                .cache = cache,
        .table_idx = UINT16_MAX,
        .table_count = cache->table_ids.size,
        .count = 0,
    };
}

bool ecs_iter_next(ecs_iter_t *it) {
    uint16_t *tids = it->cache->table_ids.data;
    do {
        if (++it->table_idx >= it->table_count)
            return false;
        it->count = ecs_world.table_index.tables[tids[it->table_idx]].entity_count;
    } while (it->count == 0);
    if (it->cache->query.field_count == 0) {
        it->ptrs = NULL;
        it->field_kinds = NULL;
    } else {
        it->ptrs = &it->cache->fields_ptr[it->table_idx * it->cache->query.field_count];
        it->field_kinds = &it->cache->fields_kind[it->table_idx * it->cache->query.field_count];
    }
    it->entities = ecs_world.table_index.tables[tids[it->table_idx]].entities;
    return true;
}
void ecs_query_fini(ecs_query_id_t qid) {
        ecs_assert(qid < ecs_world.query_index.queries.size, "invalid query id: %u\n", qid);

    ecs_query_cache_t *cache = ecs_vec_get_mut(&ecs_world.query_index.queries, qid, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", qid);

    ecs_query_index_destroy(&cache->query);
    free(cache->fields_ptr);
    free(cache->fields_kind);
    ecs_vec_fini(&cache->table_ids);
    cache->fields_ptr = NULL;
    cache->fields_kind = NULL;
    cache->field_table_capacity = 0;

    ecs_query_index_remove_active_id(&ecs_world.query_index, qid);
    cache->next_free = ecs_world.query_index.first_free;
    cache->alive = false;
    ecs_world.query_index.first_free = qid;
}

static ecs_resource_t ecs_resource_alloc_id(void) {
    ecs_resource_t id = ecs_world.resource_index.count;
    ecs_assert(id < UINT16_MAX, "resource id overflow\n");
    return id;
}

ecs_resource_t ecs_resource_init(const ecs_resource_desc_t *desc) {
        return ecs_resource_index_register(&ecs_world.resource_index, ecs_resource_alloc_id(), desc);
}

ecs_resource_t ecs_resource_find(const char *name) {
    return ecs_resource_index_find(&ecs_world.resource_index, name);
}

bool ecs_resource_is_registered_rid(ecs_resource_t id) {
    return ecs_resource_index_is_registered(&ecs_world.resource_index, id);
}

ecs_resource_t
ecs_resource_register(ecs_resource_t *id, const ecs_resource_desc_t *desc) {
        ecs_assert_not_null(id);
    if (*id == 0) {
        *id = ecs_resource_alloc_id();
    }
    return ecs_resource_index_register(&ecs_world.resource_index, *id, desc);
}

void ecs_set_resource_rid(ecs_resource_t id, const void *data) {
        ecs_assert_id_valid(id);
    ecs_assert_not_null(data);

    ecs_resource_index_set(&ecs_world.resource_index, id, data);
}

void ecs_move_resource_rid(ecs_resource_t id, void *data) {
        ecs_assert_id_valid(id);
    ecs_assert_not_null(data);

    ecs_resource_index_move(&ecs_world.resource_index, id, data);
}

void *ecs_resource_rid(ecs_resource_t id) {
        ecs_assert_id_valid(id);

    void *resource = ecs_resource_index_get(&ecs_world.resource_index, id);
    ecs_assert(resource != NULL, "resource does not exist: %d\n", id);
    return resource;
}

void *ecs_try_resource_rid(ecs_resource_t id) {
        ecs_assert_id_valid(id);

    return ecs_resource_index_get(&ecs_world.resource_index, id);
}

bool ecs_has_resource_rid(const ecs_resource_t id) {
        ecs_assert_id_valid(id);

    return ecs_resource_index_has(&ecs_world.resource_index, id);
}

void ecs_remove_resource_rid(ecs_resource_t id) {
        ecs_assert_id_valid(id);

    ecs_resource_index_remove(&ecs_world.resource_index, id);
}

#ifndef SIHTTP_H
#endif
#include <time.h>

#define ECS_SYSTEM_NO_QUERY UINT16_MAX

ecs_system_id_t ecs_system_init(const ecs_system_desc_t *desc) {
        ecs_assert_not_null(desc);
    ecs_assert(desc->callback, "system requires callback function\n");
    ecs_assert(desc->phase < EcsPhaseCount, "invalid system phase: %u\n", desc->phase);

    ecs_system_t sys = {
        .name = desc->name,
        .qid = desc->query.terms[0].id ? ecs_query_init(&desc->query) : ECS_SYSTEM_NO_QUERY,
        .callback = desc->callback,
        .user_data = desc->user_data,
        .user_data_dtor = desc->user_data_dtor,
        .phase = desc->phase,
        .enabled = !desc->disabled,
    };

    memcpy(sys.after, desc->after, sizeof(sys.after));

    ecs_system_id_t system = ecs_system_index_create(&ecs_world.system_index, &sys);
    ecs_module_record_system(system);
    return system;
}

void ecs_run_system(ecs_system_id_t system) {
    
    ecs_system_t *sys = ecs_system_index_get(&ecs_world.system_index, system);
    if (!sys->enabled) {
        return;
    }

    ecs_defer_begin();
    if (sys->qid != ECS_SYSTEM_NO_QUERY) {
        ecs_iter_t it = ecs_query_iter(sys->qid);
        it.user_data = sys->user_data;
        it.delta_time = ecs_world.delta_time;
        while (ecs_iter_next(&it)) {
            sys->callback(&it);
        }
    } else {
        ecs_iter_t it = {
                        .count = 1,
            .user_data = sys->user_data,
            .delta_time = ecs_world.delta_time,
        };
        sys->callback(&it);
    }
    ecs_defer_end();
}

void ecs_run_phase(ecs_phase_t phase) {
        ecs_assert(phase < EcsPhaseCount, "invalid system phase: %u\n", phase);

    ecs_system_index_t *index = &ecs_world.system_index;
    if (index->plan_dirty) {
        ecs_system_index_build_plan(index);
    }

    ecs_vec_t *order = &index->phase_order[phase];
    for (uint32_t i = 0; i < order->size; i++) {
        ecs_system_id_t system = *ecs_vec_get(order, i, ecs_system_id_t);
        ecs_run_system(system);
    }
}

static inline double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static inline void sleep_sec(double seconds) {
    if (seconds <= 0.0)
        return;

    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);

    nanosleep(&ts, NULL);
}

bool ecs_progress(void) {
    
    double frame_start = now_sec();

    if (ecs_world.last_time == 0.0) {
        ecs_world.delta_time = 0.0;
    } else {
        ecs_world.delta_time = frame_start - ecs_world.last_time;
    }

    ecs_world.last_time = frame_start;

    if (!ecs_world.did_start) {
        ecs_run_phase(EcsPreStart);
        ecs_run_phase(EcsStart);
        ecs_run_phase(EcsPostStart);
        ecs_world.did_start = true;
    }

    for (ecs_phase_t phase = EcsOnLoad; phase < EcsPhaseCount; phase++) {
        ecs_run_phase(phase);
    }

    if (ecs_world.features.rest) {
        sihttp_server_poll(ecs_world.server);
    }

    if (ecs_world.features.target_fps) {
        double target_dt = 1.0 / (double)ecs_world.features.target_fps;
        double elapsed = now_sec() - frame_start;
        double remaining = target_dt - elapsed;

        sleep_sec(remaining);
    }

    return !ecs_world.exit;
}

void ecs_system_enable(ecs_system_id_t system) {
    
    ecs_system_t *sys = ecs_system_index_get(&ecs_world.system_index, system);
    if (sys->enabled == true) {
        return;
    }

    sys->enabled = true;
    ecs_world.system_index.plan_dirty = true;
}

void ecs_system_disable(ecs_system_id_t system) {
    
    ecs_system_t *sys = ecs_system_index_get(&ecs_world.system_index, system);
    if (sys->enabled == false) {
        return;
    }

    sys->enabled = false;
    ecs_world.system_index.plan_dirty = true;
}

void ecs_table_init(
    ecs_table_t *table,
    ecs_type_t type,
    const ecs_component_index_t *component_index,
    uint16_t table_id
) {
    table->type = type;
    table->entity_capacity = 1;
    table->entity_count = 0;
    table->data_count = 0;
    table->entities = malloc(sizeof(ecs_entity_t) * table->entity_capacity);
    table->cls = type.count == 0 ? NULL : malloc(sizeof(ecs_column_t) * type.count);
    table->data_columns = type.count == 0 ? NULL : malloc(sizeof(uint16_t) * type.count);
    table->bloom = ecs_type_bloom(&type);

    ecs_vec_init(&table->observers_by_event, sizeof(ecs_vec_t));
    ecs_id_map_init(&table->add_edge);

    for (uint16_t i = 0; i < type.count; i++) {
        ecs_component_record_t *rec = ecs_component_index_get_mut(component_index, type.ids[i]);
        ecs_vec_push_u16(&rec->tables, table_id);
        table->cls[i].size = rec->size;
        table->cls[i].data = rec->size != 0 ? calloc(table->entity_capacity, rec->size) : NULL;
        if (rec->size != 0) {
            table->data_columns[table->data_count++] = i;
        }
        ecs_id_map_set(&table->add_edge, type.ids[i], i);
        table->cls[i].remove_edge = UINT16_MAX;
    }

    if (table->data_count == 0) {
        free(table->data_columns);
        table->data_columns = NULL;
    } else if (table->data_count < type.count) {
        table->data_columns = realloc(table->data_columns, sizeof(uint16_t) * table->data_count);
    }
}

static inline void ecs_table_grow(ecs_table_t *table) {
    uint64_t new_capacity = table->entity_capacity * (uint64_t)2;
    table->entities = realloc(table->entities, sizeof(ecs_entity_t) * new_capacity);
    for (uint16_t i = 0; i < table->data_count; i++) {
        uint16_t column_index = table->data_columns[i];
        ecs_column_t *column = &table->cls[column_index];
        const ecs_component_record_t *record =
            ecs_component_index_get(&ecs_world.component_index, table->type.ids[column_index]);

        void *new_data = calloc(new_capacity, column->size);
        ecs_assert_not_null(new_data);
        ecs_component_value_move_ctor(record, new_data, column->data, table->entity_count);
        free(column->data);
        column->data = new_data;
    }
    table->entity_capacity = new_capacity;
}

uint32_t ecs_table_add_entity(ecs_table_t *table, ecs_entity_t entity) {
    if (ECS_UNLIKELY(table->entity_count >= table->entity_capacity)) {
        ecs_table_grow(table);
    }
    uint32_t row = table->entity_count++;
    table->entities[row] = entity;
    return row;
}

// if the entity is not the last one, the last entity will be moved to the removed entity's
// position, and the moved entity will be returned
ecs_entity_t
ecs_table_remove_entity(ecs_table_t *table, uint32_t row, bool row_values_live) {
    ecs_entity_t removed_entity = table->entities[row];
    uint32_t last_row = table->entity_count - 1;
    if (row_values_live) {
        for (uint16_t i = 0; i < table->data_count; i++) {
            uint16_t column_index = table->data_columns[i];
            ecs_column_t *column = &table->cls[column_index];
            const ecs_component_record_t *record =
                ecs_component_index_get(&ecs_world.component_index, table->type.ids[column_index]);
            void *ptr = (char *)column->data + (column->size * row);
            ecs_component_value_dtor(record, ptr, 1);
        }
    }
    if (row != last_row) {
        ecs_entity_t moved_entity = table->entities[last_row];
        table->entities[row] = moved_entity;
        for (uint16_t i = 0; i < table->data_count; i++) {
            uint16_t column_index = table->data_columns[i];
            ecs_column_t *column = &table->cls[column_index];
            const ecs_component_record_t *record =
                ecs_component_index_get(&ecs_world.component_index, table->type.ids[column_index]);
            void *src = (char *)column->data + (column->size * last_row);
            void *dst = (char *)column->data + (column->size * row);
            ecs_component_value_move_ctor(record, dst, src, 1);
        }
        table->entity_count -= 1;
        return moved_entity;
    }
    table->entity_count -= 1;
    return removed_entity;
}

void *ecs_table_get_component(ecs_table_t *table, ecs_component_t component_id, uint32_t row) {
    return ecs_table_component_at_column(
        table,
        ecs_table_get_column_index(table, component_id),
        row
    );
}

void ecs_table_add_observer(ecs_table_t *table, uint16_t event, uint16_t observer_id) {
    ecs_vec_ensure(&table->observers_by_event, event + 1, sizeof(ecs_vec_t));
    ecs_vec_t *list = ecs_vec_get_mut(&table->observers_by_event, event, ecs_vec_t);
    if (list->capacity == 0) {
        ecs_vec_init(list, sizeof(uint16_t));
    }
    ecs_vec_push_u16(list, observer_id);
}

static void ecs_table_fini_component_values(ecs_table_t *table) {
    for (uint16_t c = 0; c < table->type.count; c++) {
        ecs_component_t component = table->type.ids[c];
        const ecs_component_record_t *crec =
            ecs_component_index_get(&ecs_world.component_index, component);

        if (crec->relation_flags & EcsRelationSource) {
            if (!(crec->relation_flags & EcsRelationOneToOne)) {
                for (uint32_t row = 0; row < table->entity_count; row++) {
                    RelationSource *source = ecs_table_component_at_column(table, c, row);
                    ecs_vec_fini(&source->entities);
                }
            }
            continue;
        }

        if (crec->relation_flags & EcsRelationTarget) {
            continue;
        }

        for (uint32_t row = 0; row < table->entity_count; row++) {
            void *ptr = ecs_table_component_at_column(table, c, row);
            if (crec->on_remove) {
                crec->on_remove(table->entities[row], component, ptr);
            }
            ecs_component_value_dtor(crec, ptr, 1);
        }
    }
}

void ecs_table_fini(ecs_table_t *table) {
    ecs_table_fini_component_values(table);

    for (uint16_t i = 0; i < table->type.count; i++) {
        free(table->cls[i].data);
    }
    for (uint32_t e = 0; e < table->observers_by_event.size; e++) {
        ecs_vec_fini(ecs_vec_get_mut(&table->observers_by_event, e, ecs_vec_t));
    }
    ecs_vec_fini(&table->observers_by_event);
    ecs_id_map_fini(&table->add_edge);
    free(table->entities);
    free(table->cls);
    free(table->data_columns);
    ecs_type_fini(&table->type);
}

bool ecs_table_has(
    const ecs_table_t *table,
    ecs_component_t component_id
) {
    if (ecs_table_column_or_invalid(table, component_id) != UINT16_MAX) {
        return true;
    }

    if (component_id == ecs_id(Abstract)) {
        return false;
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);
        if (ecs_table_column_or_invalid(base_table, component_id) != UINT16_MAX) {
            return true;
        }
        base = base_table->type.base;
    }

    return false;
}

bool ecs_table_is_a(const ecs_table_t *table, ecs_entity_t base) {
    if (base == 0) {
        return true;
    }

    ecs_entity_t current = table->type.base;
    while (current != 0) {
        if (current == base) {
            return true;
        }

        const ecs_entity_record_t *record = ecs_get_record(current);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);
        current = base_table->type.base;
    }

    return false;
}

void *ecs_table_field(
        const ecs_table_t *table,
    ecs_component_t component_id,
    bool *is_shared
) {
    uint16_t cidx = ecs_table_column_or_invalid(table, component_id);
    if (cidx != UINT16_MAX) {
        *is_shared = false;
        return &table->cls[cidx].data;
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);

        cidx = ecs_table_column_or_invalid(base_table, component_id);
        if (cidx != UINT16_MAX) {
            *is_shared = true;
            return ecs_table_component_at_column(base_table, cidx, record->table_row);
        }

        base = base_table->type.base;
    }

    *is_shared = false;
    return NULL;
}

static inline void move_column(
        const ecs_table_t *from_table,
    const uint16_t from_col,
    const uint32_t from_row,
    ecs_table_t *to_table,
    const uint16_t to_col,
    const uint32_t to_row
) {
    ecs_component_t component = from_table->type.ids[from_col];
    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component);
    void *src = ecs_table_component_at_column(from_table, from_col, from_row);
    void *dst = ecs_table_component_at_column(to_table, to_col, to_row);
    ecs_component_value_move_ctor(record, dst, src, 1);
}

static inline void ctor_column(
        const ecs_table_t *table,
    const uint16_t col,
    const uint32_t row
) {
    ecs_component_t component = table->type.ids[col];
    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component);
    void *dst = ecs_table_component_at_column(table, col, row);
    ecs_component_value_ctor(record, dst, 1);
}

static inline void dtor_column(
        const ecs_table_t *table,
    const uint16_t col,
    const uint32_t row
) {
    ecs_component_t component = table->type.ids[col];
    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component);
    void *ptr = ecs_table_component_at_column(table, col, row);
    ecs_component_value_dtor(record, ptr, 1);
}

static inline void finish_migration(
        ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint32_t old_row,
    const uint16_t to_table_id,
    const uint32_t new_row
) {
    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row, false);
    if (moved != entity) {
        ecs_get_record(moved)->table_row = old_row;
    }

    record->table_id = to_table_id;
    record->table_row = new_row;
}

void ecs_migrate_to_table(
        ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t fi = 0, ti = 0;
    while (fi < from_table->type.count && ti < to_table->type.count) {
        uint16_t fid = from_table->type.ids[fi];
        uint16_t tid = to_table->type.ids[ti];
        if (fid == tid) {
            move_column(from_table, fi, old_row, to_table, ti, new_row);
            fi++;
            ti++;
        } else if (fid < tid) {
            dtor_column(from_table, fi, old_row);
            fi++;
        } else {
            ctor_column(to_table, ti, new_row);
            ti++;
        }
    }
    for (; fi < from_table->type.count; fi++) {
        dtor_column(from_table, fi, old_row);
    }
    for (; ti < to_table->type.count; ti++) {
        ctor_column(to_table, ti, new_row);
    }

    finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
}

void *ecs_migrate_add(
        ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    const uint16_t to_table_id,
    const ecs_component_t added_id
) {
    const uint32_t old_row = record->table_row;
    const uint32_t new_row = ecs_table_add_entity(to_table, entity);

    const uint16_t k = ecs_table_get_column_index(to_table, added_id);
    ctor_column(to_table, k, new_row);

    uint16_t i = 0;
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= k) {
            break;
        }
        move_column(from_table, from_col, old_row, to_table, from_col, new_row);
    }
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        move_column(from_table, from_col, old_row, to_table, from_col + 1, new_row);
    }

    finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(to_table, k, new_row);
}

void *ecs_migrate_add_many(
        ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    const uint16_t to_table_id,
    const ecs_component_t requested_id
) {
    const uint32_t old_row = record->table_row;
    const uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t from_data = 0;
    for (uint16_t to_data = 0; to_data < to_table->data_count; to_data++) {
        const uint16_t to_col = to_table->data_columns[to_data];
        const ecs_component_t to_id = to_table->type.ids[to_col];

        while (from_data < from_table->data_count) {
            const uint16_t from_col = from_table->data_columns[from_data];
            const ecs_component_t from_id = from_table->type.ids[from_col];
            if (from_id >= to_id) {
                break;
            }
            from_data++;
        }

        if (from_data < from_table->data_count) {
            const uint16_t from_col = from_table->data_columns[from_data];
            if (from_table->type.ids[from_col] == to_id) {
                move_column(from_table, from_col, old_row, to_table, to_col, new_row);
                from_data++;
                continue;
            }
        }

        ctor_column(to_table, to_col, new_row);
    }

    finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(
        to_table,
        ecs_table_get_column_index(to_table, requested_id),
        new_row
    );
}

void ecs_migrate_remove(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id,
    uint16_t col_idx
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t i = 0;
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= col_idx) {
            break;
        }
        move_column(from_table, from_col, old_row, to_table, from_col, new_row);
    }
    if (i < from_table->data_count && from_table->data_columns[i] == col_idx) {
        dtor_column(from_table, col_idx, old_row);
        i++;
    }
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        move_column(from_table, from_col, old_row, to_table, from_col - 1, new_row);
    }

    finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
}

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id) {
    ecs_type_t new_type = {
        .ids = malloc((type->count + 1) * sizeof(uint16_t)),
        .count = type->count + 1,
        .base = type->base,
    };

    uint16_t i = 0;
    while (i < type->count && type->ids[i] < id) {
        new_type.ids[i] = type->ids[i];
        i++;
    }
    new_type.ids[i] = id;
    if (i < type->count) {
        memcpy(&new_type.ids[i + 1], &type->ids[i], (type->count - i) * sizeof(uint16_t));
    }

    return new_type;
}

ecs_type_t ecs_type_with_remove_at(const ecs_type_t *type, uint16_t index) {
    ecs_type_t new_type = {
        .ids = malloc((type->count - 1) * sizeof(uint16_t)),
        .count = type->count - 1,
        .base = type->base,
    };
    if (index > 0) {
        memcpy(new_type.ids, type->ids, index * sizeof(uint16_t));
    }
    if (index + 1 < type->count) {
        memcpy(
            &new_type.ids[index],
            &type->ids[index + 1],
            (type->count - index - 1) * sizeof(uint16_t)
        );
    }
    return new_type;
}

ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base) {
    ecs_type_t new_type = {
        .ids = type->count == 0 ? NULL : malloc(type->count * sizeof(uint16_t)),
        .count = type->count,
        .base = base,
    };
    if (type->count != 0) {
        memcpy(new_type.ids, type->ids, type->count * sizeof(uint16_t));
    }
    return new_type;
}

void ecs_type_fini(ecs_type_t *type) {
    if (type->ids) {
        free(type->ids);
        type->ids = NULL;
    }
}

uint64_t ecs_type_bloom(const ecs_type_t *type) {
    uint64_t filter = 0;

    for (uint16_t i = 0; i < type->count; i++) {
        filter |= (1ull << (type->ids[i] % 64));
    }

    return filter;
}

#ifndef SIHTTP_H
#endif
#ifndef SIREFLECT_H
#endif

ecs_world_t ecs_world;
static bool ecs_world_started;
static bool ecs_world_finished;

void ecs_init_w_features(const ecs_world_feat_desc_t *features) {
    ecs_assert(!ecs_world_started, "ecs_init called while ECS is already running\n");
    if (ecs_world_finished) {
        memset(&ecs_world, 0, sizeof ecs_world);
        ecs_world_finished = false;
    }
    ecs_world_started = true;

    ecs_entity_index_init(&ecs_world.entity_index);
    ecs_component_index_init(&ecs_world.component_index);
    ecs_table_index_init(&ecs_world.table_index);
    ecs_query_index_init(&ecs_world.query_index);
    ecs_observer_index_init(&ecs_world.observer_index);
    ecs_system_index_init(&ecs_world.system_index);
    ecs_module_index_init(&ecs_world.module_index);
    ecs_resource_index_init(&ecs_world.resource_index);
    ecs_arena_init(&ecs_world.arena_allocator);
    ecs_command_buffer_init(&ecs_world.commands);
    ecs_world.active_module = 0;
    ecs_world.features = *features;
    ecs_world.defer_depth = 0;
    ecs_world.flushing_commands = false;
    ecs_world.did_start = false;
    ecs_world.exit = false;
    ecs_world.server = NULL;
    ecs_world.server_state = NULL;
    ecs_world.delta_time = 0;
    ecs_world.last_time = 0;
    ecs_world.sireflect_registry = sireflect_registry_init();

    ecs_bootstrap();
}

void ecs_init(void) { ecs_init_w_features(&(ecs_world_feat_desc_t){}); }

void ecs_fini(void) {
    ecs_assert(ecs_world_started && !ecs_world_finished, "ecs_fini called outside ECS lifetime\n");
    ecs_world_finished = true;

    ecs_resource_index_fini(&ecs_world.resource_index);
    ecs_table_index_fini(&ecs_world.table_index);
    ecs_entity_index_fini(&ecs_world.entity_index);
    ecs_component_index_fini(&ecs_world.component_index);
    ecs_query_index_fini(&ecs_world.query_index);
    ecs_observer_index_fini(&ecs_world.observer_index);
    ecs_system_index_fini(&ecs_world.system_index);
    ecs_module_index_fini(&ecs_world.module_index);
    ecs_command_buffer_fini(&ecs_world.commands);
    ecs_arena_fini(&ecs_world.arena_allocator);
    sireflect_registry_fini(ecs_world.sireflect_registry);

    if (ecs_world.features.rest) {
        sihttp_server_stop(ecs_world.server);
    }
    sihttp_server_fini(ecs_world.server);
    free(ecs_world.server_state);
    ecs_world_started = false;
}

void ecs_quit(void) { ecs_world.exit = true; }

#ifndef SIECS_ADDONS_REST_INTERNAL_H
#define SIECS_ADDONS_REST_INTERNAL_H

#ifndef SIHTTP_H
#endif
#ifndef SIJSON_H
#endif
#ifndef SIREFLECT_H
#endif

sihttp_response_t ecs_rest_json_response(int status, sijson_value_t body);
sihttp_response_t ecs_rest_error_response(int status, const char *message);

sijson_value_t ecs_rest_entity_json(ecs_entity_t entity);
sijson_value_t ecs_rest_entity_children_json(ecs_entity_t entity);
sijson_value_t ecs_rest_entity_detail_json(ecs_entity_t entity);
bool ecs_rest_entity_component_is_reflected(ecs_component_t component);
sijson_value_t
ecs_rest_entity_component_json(ecs_component_t component, const void *ptr);
sihttp_response_t ecs_rest_set_entity_component(
        ecs_entity_t entity,
    ecs_component_t component,
    const char *body
);

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req);
sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req);

#endif

#ifndef SIHTTP_H
#endif

sihttp_response_t health(const sihttp_request_t *) {
    return sihttp_response({ .body = strdup("OK") });
}

void init_rest() {
    sihttp_app_state_t *state = malloc(sizeof(sihttp_app_state_t));

    ecs_world.server_state = state;

    ecs_world.server = sihttp_server(
        {
            .port = 4040,
            .state = state,
        }
    );

    sihttp_get(ecs_world.server, "/schema", ecs_rest_get_schema);
    sihttp_get(ecs_world.server, "/entities", ecs_rest_get_entities);
    sihttp_post(ecs_world.server, "/entities", ecs_rest_post_entities);
    sihttp_get(ecs_world.server, "/entities/:index/children", ecs_rest_get_entity_children);
    sihttp_get(ecs_world.server, "/health", health);
    sihttp_put(
        ecs_world.server,
        "/entities/:index/components/:component",
        ecs_rest_put_entity_component
    );
    sihttp_get(ecs_world.server, "/entities/:index", ecs_rest_get_entity);

    if (ecs_world.features.rest) {
        sihttp_server_start(ecs_world.server);
    }
}

sihttp_response_t ecs_rest_json_response(int status, sijson_value_t body) {
    char *json = sijson_stringify(body);
    if (!json) {
        json = strdup("{\"error\":\"failed to serialize response\"}");
        status = 500;
    }

    return sihttp_response({
        .status = status,
        .body = json,
        .content_type = SIHTTP_CONTENT_JSON,
    });
}

sihttp_response_t ecs_rest_error_response(int status, const char *message) {
    sijson_clean();

    sijson_value_t body = sijson_make_object();
    sijson_object_set(body, "error", sijson_make_string(message));

    return ecs_rest_json_response(status, body);
}

#ifndef SIJSON_H
#endif

static bool entity_from_index(int64_t index, ecs_entity_t *out) {
    if (index <= 0 || (uint64_t)index >= ecs_world.entity_index.entities.size) {
        return false;
    }

    ecs_entity_record_t *record =
        ecs_vec_get_mut(&ecs_world.entity_index.entities, (uint32_t)index, ecs_entity_record_t);
    if (record->table_id == UINT16_MAX) {
        return false;
    }

    *out = ecs_entity((uint32_t)index, record->generation);
    return true;
}

static bool entity_is_alive(ecs_entity_t entity) {
    ecs_entity_t current = 0;
    return entity_from_index(ecs_first(entity), &current) && current == entity;
}

static char *entity_name(ecs_entity_t entity) {
    if (ecs_has(entity, Name)) {
        const char *value = ecs_get(entity, Name)->value;
        return strdup(value ? value : "");
    }
    return siformat("(%d, %d)", ecs_first(entity), ecs_second(entity));
}

sijson_value_t ecs_rest_entity_json(ecs_entity_t entity) {
    sijson_value_t object = sijson_make_object();

    char *name = entity_name(entity);
    sijson_object_set(object, "name", sijson_make_string(name));
    free(name);

    sijson_object_set(object, "index", sijson_make_number(ecs_first(entity)));
    sijson_object_set(object, "generation", sijson_make_number(ecs_second(entity)));
    sijson_object_set(
        object,
        "hasChildren",
        sijson_make_bool(ecs_has_cid(entity, ecs_source(ChildOf)))
    );
    return object;
}

sijson_value_t ecs_rest_entity_children_json(ecs_entity_t entity) {
    sijson_value_t children = sijson_make_array();
    RelationSource *source = ecs_try_get_cid(entity, ecs_source(ChildOf));
    if (!source) {
        return children;
    }

    for (uint32_t i = 0; i < source->entities.size; i++) {
        ecs_entity_t child = *ecs_vec_get(&source->entities, i, ecs_entity_t);
        if (entity_is_alive(child)) {
            sijson_array_push(children, ecs_rest_entity_json(child));
        }
    }
    return children;
}

sijson_value_t ecs_rest_entity_detail_json(ecs_entity_t entity) {
    sijson_value_t detail = sijson_make_object();

    char *name = entity_name(entity);
    sijson_object_set(detail, "name", sijson_make_string(name));
    free(name);

    sijson_object_set(detail, "index", sijson_make_number(ecs_first(entity)));
    sijson_object_set(detail, "generation", sijson_make_number(ecs_second(entity)));

    ChildOf *parent = ecs_try_get(entity, ChildOf);
    if (parent) {
        sijson_object_set(detail, "parent", ecs_rest_entity_json(parent->target));
    }

    sijson_value_t components = sijson_make_array();
    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    for (uint32_t i = 0; i < table->type.count; i++) {
        ecs_component_t cid = table->type.ids[i];
        if (ecs_rest_entity_component_is_reflected(cid)) {
            void *ptr = ecs_table_get_component(table, cid, record->table_row);
            sijson_array_push(components, ecs_rest_entity_component_json(cid, ptr));
        }
    }

    if (table->type.base) {
        sijson_object_set(detail, "isA", ecs_rest_entity_detail_json(table->type.base));
    }

    sijson_object_set(detail, "children", ecs_rest_entity_children_json(entity));
    sijson_object_set(detail, "components", components);
    return detail;
}

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req) {
    sijson_clean();

    sijson_value_t array = sijson_make_array();
    ecs_query_each(it, i, { ecs_source(ChildOf) }, { ecs_id(ChildOf), EcsNot }) {
        sijson_array_push(array, ecs_rest_entity_json(it.entities[i]));
    }
    ecs_query_each(it, i, { ecs_source(ChildOf), EcsNot }, { ecs_id(ChildOf), EcsNot }) {
        sijson_array_push(array, ecs_rest_entity_json(it.entities[i]));
    }
    return ecs_rest_json_response(200, array);
}

sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t entity = 0;
    if (!entity_from_index(sihttp_param(req, "index"), &entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }
    return ecs_rest_json_response(200, ecs_rest_entity_detail_json(entity));
}

sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req) {
    sijson_clean();

    ecs_entity_t entity = 0;
    if (!entity_from_index(sihttp_param(req, "index"), &entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }
    return ecs_rest_json_response(200, ecs_rest_entity_children_json(entity));
}

sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req) {
    int64_t component = sihttp_param(req, "component");

    ecs_entity_t entity = 0;
    if (!entity_from_index(sihttp_param(req, "index"), &entity)) {
        sijson_clean();
        return ecs_rest_error_response(404, "entity not found");
    }
    if (component <= 0 || component > UINT16_MAX) {
        sijson_clean();
        return ecs_rest_error_response(404, "component not found");
    }

    return ecs_rest_set_entity_component(entity, (ecs_component_t)component, req->body);
}

sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req) {
    ecs_entity_t entity = ecs_new();
    return sihttp_response(
        {
            .body = sijson_stringify(ecs_rest_entity_json(entity)),
        }
    );
}

#ifndef SIJSON_H
#endif

static void ensure_sijson_entity_type(void) {
    sireflect_register_struct(
        sijson_default_registry(),
        &(sireflect_struct_desc_t){
            .name = "ecs_entity_t",
            .fields = "{ uint32_t id; uint32_t generation; }",
            .size = sizeof(ecs_entity_t),
            .align = _Alignof(ecs_entity_t),
        }
    );
}

bool ecs_rest_entity_component_is_reflected(ecs_component_t component) {
    if (component >= ecs_world.component_index.components.size) {
        return false;
    }

    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component);
    return record->reflection != SIREFLECT_INVALID_HANDLE &&
           record->reflection_desc != NULL;
}

static bool validate_component_shape(
        const ecs_component_record_t *record,
    sijson_value_t value
) {
    const sireflect_fields_t *fields =
        sireflect_type_fields(ecs_world.sireflect_registry, record->reflection);
    if (sijson_type(value) != SIJSON_OBJECT || sijson_object_len(value) != fields->field_count) {
        return false;
    }

    for (size_t i = 0; i < fields->field_count; i++) {
        if (!sijson_object_get(value, fields->fields[i].name)) {
            return false;
        }
    }

    for (size_t i = 0; i < sijson_object_len(value); i++) {
        const char *key = sijson_object_key(value, i);
        bool found = false;
        for (size_t f = 0; f < fields->field_count; f++) {
            found = found || strcmp(key, fields->fields[f].name) == 0;
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

static sijson_value_t component_value_json(const ecs_component_record_t *record, const void *ptr) {
    ensure_sijson_entity_type();

    sireflect_handle_t ref = SIREFLECT_INVALID_HANDLE;
    char *json = sijson_to_json_impl(&ref, record->reflection_desc, ptr);
    if (!json) {
        return sijson_make_null();
    }

    sijson_value_t value = sijson_parse(json);
    free(json);
    return value ? value : sijson_make_null();
}

sijson_value_t
ecs_rest_entity_component_json(ecs_component_t component_id, const void *ptr) {
    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component_id);
    const sireflect_type_info_t *type =
        sireflect_type_info(ecs_world.sireflect_registry, record->reflection);

    sijson_value_t component = sijson_make_object();
    sijson_object_set(component, "id", sijson_make_number(component_id));
    sijson_object_set(component, "name", sijson_make_string(type && type->name ? type->name : ""));
    sijson_object_set(component, "value", component_value_json(record, ptr));
    return component;
}

sihttp_response_t ecs_rest_set_entity_component(
        ecs_entity_t entity,
    ecs_component_t component,
    const char *body_text
) {
    sijson_clean();

    if (!ecs_is_alive(entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }
    if (!ecs_rest_entity_component_is_reflected(component)) {
        return ecs_rest_error_response(404, "component not found");
    }
    if (!ecs_has_cid(entity, component)) {
        return ecs_rest_error_response(404, "entity component not found");
    }

    sijson_value_t body = sijson_parse(body_text);
    sijson_value_t value = body ? sijson_object_get(body, "value") : NULL;
    if (!body || sijson_type(body) != SIJSON_OBJECT || sijson_object_len(body) != 1 || !value) {
        return ecs_rest_error_response(400, "invalid json body");
    }

    const ecs_component_record_t *record =
        ecs_component_index_get(&ecs_world.component_index, component);
    if (!validate_component_shape(record, value)) {
        return ecs_rest_error_response(400, "invalid component value");
    }

    char *json = sijson_stringify(value);
    sireflect_handle_t ref = SIREFLECT_INVALID_HANDLE;
    void *decoded = json ? sijson_from_json_impl(&ref, record->reflection_desc, json) : NULL;
    free(json);
    if (!decoded || sijson_error()) {
        return ecs_rest_error_response(400, "invalid component value");
    }

    ecs_set_cid(entity, component, decoded);
    return ecs_rest_json_response(
        200,
        ecs_rest_entity_component_json(component, ecs_get_cid(entity, component))
    );
}

typedef struct {
    bool *items;
    size_t count;
} ecs_rest_type_set_t;

static bool ecs_rest_component_is_reflected(ecs_component_t id) {
    if (id >= ecs_world.component_index.components.size) {
        return false;
    }

    const ecs_component_record_t *record = ecs_component_index_get(&ecs_world.component_index, id);
    return record->reflection != SIREFLECT_INVALID_HANDLE;
}

static sijson_value_t ecs_rest_field_json(const sireflect_field_info_t *field) {
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "name", sijson_make_string(field->name));
    sijson_object_set(object, "type", sijson_make_number(field->type));

    return object;
}

static sijson_value_t ecs_rest_component_json(
        ecs_component_t id,
    const ecs_component_record_t *record
) {
    sijson_value_t fields_json = sijson_make_array();
    const sireflect_type_info_t *type =
        sireflect_type_info(ecs_world.sireflect_registry, record->reflection);
    const sireflect_fields_t *fields =
        sireflect_type_fields(ecs_world.sireflect_registry, record->reflection);
    for (size_t i = 0; i < fields->field_count; i++) {
        sijson_array_push(fields_json, ecs_rest_field_json(&fields->fields[i]));
    }

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(type && type->name ? type->name : ""));
    sijson_object_set(object, "isRelation", sijson_make_bool(record->relation_flags != 0));
    sijson_object_set(object, "type", sijson_make_number(record->reflection));
    sijson_object_set(object, "fields", fields_json);

    return object;
}

static void ecs_rest_type_set_add(ecs_rest_type_set_t *set, sireflect_handle_t id) {
    if (id == SIREFLECT_INVALID_HANDLE) {
        return;
    }

    if (id >= set->count) {
        size_t count = set->count == 0 ? 64 : set->count;
        while (id >= count) {
            count *= 2;
        }

        bool *items = realloc(set->items, count * sizeof(bool));
        if (!items) {
            abort();
        }

        memset(items + set->count, 0, (count - set->count) * sizeof(bool));
        set->items = items;
        set->count = count;
    }

    set->items[id] = true;
}

static void ecs_rest_collect_component_types(
        ecs_rest_type_set_t *set,
    const ecs_component_record_t *record
) {
    ecs_rest_type_set_add(set, record->reflection);

    const sireflect_fields_t *fields =
        sireflect_type_fields(ecs_world.sireflect_registry, record->reflection);
    for (size_t i = 0; i < fields->field_count; i++) {
        ecs_rest_type_set_add(set, fields->fields[i].type);
    }
}

static bool ecs_rest_type_name_is(const sireflect_type_info_t *type, const char *name) {
    return type->name && strcmp(type->name, name) == 0;
}

static const char *
ecs_rest_editor_type(sireflect_handle_t id, const sireflect_type_info_t *type) {
    if (ecs_rest_type_name_is(type, "ecs_entity_t")) {
        return "entity";
    }

    if (type->kind == sireflect_kind_bool) {
        return "boolean";
    }

    if (sireflect_is_numeric(type->kind)) {
        return "number";
    }

    if (type->kind == sireflect_kind_struct) {
        return "object";
    }

    if (type->kind == sireflect_kind_pointer) {
        const sireflect_type_info_t *element =
            sireflect_type_info(ecs_world.sireflect_registry, type->element_type);
        if (element && element->kind == sireflect_kind_char) {
            return "string";
        }
    }

    (void)id;
    return "unsupported";
}

static sijson_value_t ecs_rest_type_json(sireflect_handle_t id) {
    const sireflect_type_info_t *type = sireflect_type_info(ecs_world.sireflect_registry, id);

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(type->name ? type->name : ""));
    sijson_object_set(object, "editor", sijson_make_string(ecs_rest_editor_type(id, type)));

    return object;
}

sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req) {
    ecs_rest_type_set_t types = { 0 };

    sijson_clean();

    sijson_value_t components = sijson_make_array();
    for (uint32_t i = 2; i < ecs_world.component_index.components.size; i++) {
        if (!ecs_rest_component_is_reflected((ecs_component_t)i)) {
            continue;
        }

        const ecs_component_record_t *record =
            ecs_component_index_get(&ecs_world.component_index, (ecs_component_t)i);
        ecs_rest_collect_component_types(&types, record);
        sijson_array_push(components, ecs_rest_component_json((ecs_component_t)i, record));
    }

    sijson_value_t type_values = sijson_make_array();
    for (size_t i = 1; i < types.count; i++) {
        if (types.items[i]) {
            sijson_array_push(type_values, ecs_rest_type_json((sireflect_handle_t)i));
        }
    }

    free(types.items);

    sijson_value_t schema = sijson_make_object();
    sijson_object_set(schema, "components", components);
    sijson_object_set(schema, "types", type_values);

    return ecs_rest_json_response(200, schema);
}

#define ECS_ARENA_INITIAL_CAPACITY 4096u

static ecs_arena_block_t *ecs_arena_block_new(uint32_t capacity) {
    ecs_arena_block_t *block = malloc(sizeof(ecs_arena_block_t) + capacity);
    *block = (ecs_arena_block_t){ .capacity = capacity };
    return block;
}

void ecs_arena_init(ecs_arena_t *allocator) {
    ecs_arena_block_t *block = ecs_arena_block_new(ECS_ARENA_INITIAL_CAPACITY);
    *allocator = (ecs_arena_t){
        .first = block,
        .current = block,
        .last = block,
    };
}

void *ecs_arena_alloc_slow(ecs_arena_t *allocator, uint32_t size) {
    for (ecs_arena_block_t *block = allocator->current->next; block; block = block->next) {
        if (size <= block->capacity) {
            allocator->current = block;
            block->cursor = size;
            return block->data;
        }
    }

    uint32_t capacity = allocator->last->capacity;
    if (capacity <= UINT32_MAX / 2u) {
        capacity *= 2u;
    }
    while (capacity < size) {
        if (capacity > UINT32_MAX / 2u) {
            capacity = size;
            break;
        }
        capacity *= 2u;
    }

    ecs_arena_block_t *block = ecs_arena_block_new(capacity);
    allocator->last->next = block;
    allocator->last = block;
    allocator->current = block;
    block->cursor = size;
    return block->data;
}

void ecs_arena_fini(ecs_arena_t *allocator) {
    ecs_arena_block_t *block = allocator->first;
    while (block) {
        ecs_arena_block_t *next = block->next;
        free(block);
        block = next;
    }
}

void ecs_id_map_init(ecs_id_map_t *map) {
    map->capacity = 1;
    map->ids = malloc(sizeof(uint16_t));
    *map->ids = UINT16_MAX;
}

void ecs_id_map_fini(ecs_id_map_t *map) { free(map->ids); }

void ecs_id_map_ensure(ecs_id_map_t *map, uint16_t id) {
    if (ECS_UNLIKELY(id >= map->capacity)) {
        uint16_t new_cap = map->capacity;
        while (new_cap <= id)
            new_cap *= 2;
        map->ids = realloc(map->ids, sizeof(uint16_t) * new_cap);
        memset(map->ids + map->capacity, 0xFF, sizeof(uint16_t) * (new_cap - map->capacity));
        map->capacity = new_cap;
    }
}

void ecs_vec_init(ecs_vec_t *vec, uint32_t element_size) {
    vec->data = malloc(element_size); // Start with 1 elements
    vec->size = 0;
    vec->capacity = 1;
}

void ecs_vec_init_w_size(ecs_vec_t *vec, uint32_t element_size, uint32_t size) {
    vec->data = malloc(element_size * size);
    vec->size = 0;
    vec->capacity = size;
}

void ecs_vec_fini(ecs_vec_t *vec) { free(vec->data); }

void ecs_vec_grow(ecs_vec_t *vec, uint32_t element_size) {
    vec->capacity *= 2;
    vec->data = realloc(vec->data, element_size * vec->capacity);
}

void ecs_vec_push(ecs_vec_t *vec, const void *element, const uint32_t element_size) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, element_size);
    }
    memcpy((uint8_t *)vec->data + (vec->size * element_size), element, element_size);
    vec->size++;
}

void ecs_vec_ensure(ecs_vec_t *vec, uint32_t count, const uint32_t element_size) {
    if (count <= vec->size)
        return;
    while (vec->capacity < count)
        ecs_vec_grow(vec, element_size);
    memset((uint8_t *)vec->data + vec->size * element_size, 0, (count - vec->size) * element_size);
    vec->size = count;
}

void ecs_vec_remove_fast(ecs_vec_t *vec, uint32_t index, const uint32_t element_size) {
    if (index < vec->size - 1) {
        void *dst = (uint8_t *)vec->data + (index * element_size);
        const void *src = (uint8_t *)vec->data + ((vec->size - 1) * element_size);
        memcpy(dst, src, element_size);
    }
    vec->size--;
}

bool ecs_vec_contains_u16(const ecs_vec_t *vec, const uint16_t value) {
    ecs_vec_iter(vec, uint16_t, current, {
        if (*current == value) {
            return true;
        }
    });
    return false;
}

static inline void ecs_vec_remove_fast_u16(ecs_vec_t *vec, uint32_t index) {
    if (index < vec->size - 1) {
        uint16_t *data = vec->data;
        data[index] = data[vec->size - 1];
    }
    vec->size--;
}

void ecs_vec_remove_u16(ecs_vec_t *vec, const uint16_t value) {
    ecs_vec_iter(vec, uint16_t, current, {
        if (*current == value) {
            ecs_vec_remove_fast_u16(vec, i);
            return;
        }
    });
}

static inline void ecs_vec_remove_fast_u64(ecs_vec_t *vec, uint32_t index) {
    if (index < vec->size - 1) {
        uint64_t *data = vec->data;
        data[index] = data[vec->size - 1];
    }
    vec->size--;
}

void ecs_vec_remove_u64(ecs_vec_t *vec, uint64_t value) {
    ecs_vec_iter(vec, uint64_t, current, {
        if (*current == value) {
            ecs_vec_remove_fast_u64(vec, i);
            return;
        }
    });
}

#ifndef SIREFLECT_H
#endif

void ecs_component_index_register(
    ecs_component_index_t *index,
    ecs_component_t id,
    uint64_t size,
    ecs_type_ops_t ops,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    uint32_t relation_flags,
    sireflect_handle_t reflection,
    const sireflect_struct_desc_t *reflection_desc
) {
    ecs_vec_ensure(&index->components, (uint32_t)id + 1, sizeof(ecs_component_record_t));

    ecs_component_record_t *existing =
        ecs_vec_get_mut(&index->components, id, ecs_component_record_t);
    if (existing->tables.data) {
        return;
    }

    ecs_component_record_t record = {
        .required = NULL,
        .required_count = 0,
        .size = size,
        .ops = ops,
        .on_set = on_set,
        .on_remove = on_remove,
        .on_add = on_add,
        .relation_flags = relation_flags,
        .tables = { 0 },
        .reflection = reflection,
        .reflection_desc = reflection_desc,
    };
    ecs_vec_init(&record.tables, sizeof(uint16_t));

    *existing = record;
}

void ecs_component_index_init(ecs_component_index_t *index) {
    ecs_vec_init_w_size(&index->components, sizeof(ecs_component_record_t), 256);
}

void ecs_component_index_fini(ecs_component_index_t *index) {
    ecs_component_record_t *records = index->components.data;

    for (uint32_t i = 0; i < index->components.size; i++) {
        free(records[i].required);
        ecs_vec_fini(&records[i].tables);
    }
    ecs_vec_fini(&index->components);
}

void ecs_component_value_ctor(const ecs_component_record_t *record, void *dst, uint32_t count) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.ctor) {
        record->ops.ctor(dst, count);
        return;
    }

    memset(dst, 0, (size_t)record->size * count);
}

void ecs_component_value_dtor(const ecs_component_record_t *record, void *ptr, uint32_t count) {
    if (record->size == 0 || count == 0 || !record->ops.dtor) {
        return;
    }

    record->ops.dtor(ptr, count);
}

void ecs_component_value_copy_ctor(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

void ecs_component_value_copy(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.copy) {
        record->ops.copy(dst, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

void ecs_component_value_move_ctor(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.move_ctor) {
        record->ops.move_ctor(dst, src, count);
        return;
    }
    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, count);
        ecs_component_value_dtor(record, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

void ecs_component_value_move(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.move) {
        record->ops.move(dst, src, count);
        return;
    }
    if (record->ops.copy) {
        record->ops.copy(dst, src, count);
        ecs_component_value_dtor(record, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

void ecs_entity_index_init(ecs_entity_index_t *index) {
    ecs_vec_init_w_size(&index->entities, sizeof(ecs_entity_record_t), 256);
    index->first_available = UINT32_MAX;
}

void ecs_entity_index_fini(ecs_entity_index_t *index) {
    ecs_vec_fini(&index->entities);
}

static bool ecs_module_id_valid(const ecs_module_index_t *index, ecs_module_id_t module) {
    return module != 0 && module < index->modules.size;
}

static void ecs_module_record_init(ecs_module_t *module, ecs_module_id_t *id, const char *name) {
    module->id = id;
    module->name = name;
    module->enabled = true;
    ecs_vec_init(&module->observers, sizeof(ecs_observer_id_t));
    ecs_vec_init(&module->systems, sizeof(ecs_system_id_t));
}

static void ecs_module_record_fini(ecs_module_t *module) {
    if (module->id) {
        *module->id = 0;
    }

    ecs_vec_fini(&module->observers);
    ecs_vec_fini(&module->systems);
}

void ecs_module_index_init(ecs_module_index_t *index) {
    ecs_vec_init(&index->modules, sizeof(ecs_module_t));
    ecs_vec_ensure(&index->modules, 1, sizeof(ecs_module_t));
}

void ecs_module_index_fini(ecs_module_index_t *index) {
    for (uint32_t i = 1; i < index->modules.size; i++) {
        ecs_module_t *module = ecs_vec_get_mut(&index->modules, i, ecs_module_t);
        ecs_module_record_fini(module);
    }
    ecs_vec_fini(&index->modules);
}

ecs_module_id_t ecs_module_index_create(
    ecs_module_index_t *index,
    ecs_module_id_t *id,
    const char *name
) {
    ecs_module_t module;
    ecs_module_record_init(&module, id, name);
    ecs_vec_push(&index->modules, &module, sizeof(ecs_module_t));
    return index->modules.size - 1;
}

ecs_module_t *ecs_module_index_get(ecs_module_index_t *index, ecs_module_id_t module) {
    ecs_assert(ecs_module_id_valid(index, module), "invalid module id: %u\n", module);
    return ecs_vec_get_mut(&index->modules, module, ecs_module_t);
}

const ecs_module_t *ecs_module_index_get_const(
    const ecs_module_index_t *index,
    ecs_module_id_t module
) {
    ecs_assert(ecs_module_id_valid(index, module), "invalid module id: %u\n", module);
    return ecs_vec_get(&index->modules, module, ecs_module_t);
}

ecs_module_id_t ecs_module_index_find(const ecs_module_index_t *index, const ecs_module_id_t *id) {
    if (!id || !*id) {
        return 0;
    }

    ecs_module_id_t module = *id;
    if (ecs_module_id_valid(index, module)) {
        return module;
    }

    return 0;
}

#define ECS_BUILTIN_EVENT_COUNT 3 // EcsOnAdd, EcsOnRemove, EcsOnSet

void ecs_observer_index_init(ecs_observer_index_t *index) {
    ecs_vec_init(&index->observers, sizeof(ecs_observer_t));
    index->event_count = ECS_BUILTIN_EVENT_COUNT;
}

void ecs_observer_index_fini(ecs_observer_index_t *index) {
    for (uint32_t i = 0; i < index->observers.size; i++) {
        ecs_observer_t *obs = ecs_vec_get_mut(&index->observers, i, ecs_observer_t);
        ecs_query_index_destroy(&obs->query);
    }
    ecs_vec_fini(&index->observers);
}

uint16_t ecs_observer_index_create(ecs_observer_index_t *index, const ecs_observer_desc_t *desc) {
    ecs_observer_t *obs = ecs_vec_push_empty(&index->observers, sizeof(ecs_observer_t));
    obs->event = desc->on;
    obs->callback = desc->callback;
    obs->user_data = desc->user_data;
    obs->enabled = true;
    ecs_query_from_desc(&desc->query, &obs->query);
    return index->observers.size - 1;
}

void ecs_observer_index_match_tables(
        ecs_table_t *tables,
    uint16_t table_count,
    uint16_t observer_id
) {
    ecs_observer_t *obs =
        ecs_vec_get_mut(&ecs_world.observer_index.observers, observer_id, ecs_observer_t);
    for (uint16_t i = 0; i < table_count; i++) {
        if (ecs_query_match_table(&obs->query, &tables[i])) {
            ecs_table_add_observer(&tables[i], obs->event, observer_id);
        }
    }
}

void ecs_observer_index_add_table(ecs_table_t *table) {
    for (uint32_t i = 0; i < ecs_world.observer_index.observers.size; i++) {
        ecs_observer_t *obs = ecs_vec_get_mut(&ecs_world.observer_index.observers, i, ecs_observer_t);
        if (ecs_query_match_table(&obs->query, table)) {
            ecs_table_add_observer(table, obs->event, i);
        }
    }
}

void ecs_query_index_init(ecs_query_index_t *index) {
    ecs_vec_init(&index->queries, sizeof(ecs_query_cache_t));
    ecs_vec_init(&index->active_ids, sizeof(ecs_query_id_t));
    index->first_free = UINT16_MAX;
}

void ecs_query_index_fini(ecs_query_index_t *index) {
    const ecs_query_id_t *active_ids = index->active_ids.data;
    for (uint32_t i = 0; i < index->active_ids.size; i++) {
        ecs_query_cache_t *cache =
            ecs_vec_get_mut(&index->queries, active_ids[i], ecs_query_cache_t);
        ecs_vec_fini(&cache->table_ids);
        free(cache->fields_ptr);
        free(cache->fields_kind);
        ecs_query_index_destroy(&cache->query);
    }
    ecs_vec_fini(&index->active_ids);
    ecs_vec_fini(&index->queries);
}

void ecs_query_index_destroy(ecs_query_t *query) {
    free(query->terms);
    free(query->fields);
}

static uint16_t ecs_query_count_terms(const ecs_query_term_t *terms) {
    uint16_t i = 0;
    while (terms[i].id) {
        i++;
    }
    return i;
}

static ecs_query_term_t *ecs_query_copy_terms(const ecs_query_term_t *terms, uint16_t count) {
    if (count == 0) {
        return NULL;
    }
    ecs_query_term_t *copy = malloc(sizeof(ecs_query_term_t) * count);
    memcpy(copy, terms, sizeof(ecs_query_term_t) * count);
    return copy;
}

static bool
ecs_query_has_term(const ecs_query_term_t *terms, uint16_t term_count, ecs_component_t id) {
    for (uint16_t i = 0; i < term_count; i++) {
        if (terms[i].id == id) {
            return true;
        }
    }
    return false;
}

static ecs_query_term_t *
ecs_query_copy_terms_with_implicit_excludes(const ecs_query_term_t *terms, uint16_t *count) {
    const ecs_component_t excludes[] = {
        ecs_id(Disabled),
        ecs_id(Abstract),
    };
    const uint16_t explicit_count = *count;
    uint16_t exclude_count = 0;

    for (uint32_t i = 0; i < sizeof(excludes) / sizeof(excludes[0]); i++) {
        if (excludes[i] && !ecs_query_has_term(terms, explicit_count, excludes[i])) {
            exclude_count++;
        }
    }

    if (exclude_count == 0) {
        return ecs_query_copy_terms(terms, *count);
    }

    ecs_assert(*count + exclude_count < 16, "query has no room for implicit exclude terms\n");

    ecs_query_term_t *copy = malloc(sizeof(ecs_query_term_t) * (*count + exclude_count));
    if (*count != 0) {
        memcpy(copy, terms, sizeof(ecs_query_term_t) * *count);
    }

    for (uint32_t i = 0; i < sizeof(excludes) / sizeof(excludes[0]); i++) {
        if (excludes[i] && !ecs_query_has_term(terms, explicit_count, excludes[i])) {
            copy[*count] = (ecs_query_term_t){ .id = excludes[i], .access = EcsNot };
            *count += 1;
        }
    }

    return copy;
}

static bool ecs_query_term_is_field(ecs_query_term_t term) {
    return term.access == EcsIn || term.access == EcsOut || term.access == EcsInOut ||
           term.access == EcsInOptional || term.access == EcsInOutOptional;
}

static bool ecs_query_term_is_positive(ecs_query_term_t term) {
    return term.access == EcsIn || term.access == EcsOut || term.access == EcsInOut ||
           term.access == EcsFilter;
}

static void ecs_query_validate_terms(const ecs_query_term_t *terms, uint16_t term_count) {
    for (uint16_t i = 0; i < term_count; i++) {
        ecs_assert_id_valid(terms[i].id);
        ecs_assert(
            terms[i].access == EcsIn || terms[i].access == EcsOut || terms[i].access == EcsInOut ||
                terms[i].access == EcsInOptional || terms[i].access == EcsInOutOptional ||
                terms[i].access == EcsFilter || terms[i].access == EcsNot,
            "invalid query term access: %d\n",
            terms[i].access
        );

        for (uint16_t j = i + 1; j < term_count; j++) {
            ecs_assert(
                terms[i].id != terms[j].id,
                "duplicate query term component: %d\n",
                terms[i].id
            );
        }
    }
}

void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query) {
    query->term_count = ecs_query_count_terms(desc->terms);

    query->terms = ecs_query_copy_terms_with_implicit_excludes(desc->terms, &query->term_count);
    ecs_query_validate_terms(query->terms, query->term_count);

    query->is_a = desc->is_a;

    query->field_count = 0;
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_field(query->terms[i])) {
            query->field_count++;
        }
    }

    query->fields = NULL;
    if (query->field_count != 0) {
        query->fields = malloc(sizeof(ecs_query_term_t) * query->field_count);

        uint16_t field = 0;
        for (uint16_t i = 0; i < query->term_count; i++) {
            if (ecs_query_term_is_field(query->terms[i])) {
                query->fields[field++] = query->terms[i];
            }
        }
    }

    query->bloom = 0;
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_positive(query->terms[i])) {
            query->bloom |= 1ull << (query->terms[i].id % 64);
        }
    }
}

static void ecs_query_cache_add_table(
        ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_id
) {
    ecs_vec_push_u16(&cache->table_ids, table_id);
    const uint16_t table_count = cache->table_ids.size;
    const uint16_t field_count = cache->query.field_count;

    if (table_count > cache->field_table_capacity) {
        uint16_t capacity = cache->field_table_capacity ? cache->field_table_capacity : 4;
        while (capacity < table_count) {
            capacity *= 2;
        }

        const uint32_t slot_count = (uint32_t)capacity * field_count;
        cache->fields_ptr = realloc(cache->fields_ptr, sizeof(void *) * slot_count);
        cache->fields_kind = realloc(cache->fields_kind, sizeof(ecs_field_kind_t) * slot_count);
        cache->field_table_capacity = capacity;
    }

    const uint32_t base = (uint32_t)(table_count - 1) * field_count;
    for (uint16_t i = 0; i < cache->query.field_count; i++) {
        const ecs_query_term_t term = cache->query.fields[i];
        void *field = NULL;
        ecs_field_kind_t field_kind = EcsFieldNone;

        if (ecs_query_term_requires_owned(term)) {
            uint16_t column = ecs_table_column_or_invalid(table, term.id);
            if (column != UINT16_MAX) {
                field = &table->cls[column].data;
                field_kind = EcsFieldOwned;
            }
        } else {
            bool is_shared = false;
            field = ecs_table_field(table, term.id, &is_shared);
            if (field || is_shared) {
                field_kind = is_shared ? EcsFieldShared : EcsFieldOwned;
            }
        }

        ecs_assert(
            field_kind != EcsFieldNone || term.access == EcsInOptional ||
                term.access == EcsInOutOptional,
            "query cache matched table without field component: %d\n",
            term.id
        );

        cache->fields_ptr[base + i] = field;
        cache->fields_kind[base + i] = field_kind;
    }
}

ecs_query_id_t ecs_query_index_create(ecs_query_index_t *index, const ecs_query_desc_t *desc) {
    ecs_query_id_t id;
    ecs_query_cache_t *query_cache;

    if (index->first_free != UINT16_MAX) {
        id = index->first_free;
        query_cache = ecs_vec_get_mut(&index->queries, id, ecs_query_cache_t);
        index->first_free = query_cache->next_free;
    } else {
        query_cache = ecs_vec_push_empty(&index->queries, sizeof(ecs_query_cache_t));
        id = index->queries.size - 1;
    }

    ecs_query_from_desc(desc, &query_cache->query);
    ecs_vec_init(&query_cache->table_ids, sizeof(uint16_t));
    query_cache->fields_ptr = NULL;
    query_cache->fields_kind = NULL;
    query_cache->field_table_capacity = 0;
    query_cache->active_index = index->active_ids.size;
    query_cache->next_free = UINT16_MAX;
    query_cache->alive = true;
    ecs_vec_push_u16(&index->active_ids, id);

    return id;
}

static ecs_component_t ecs_query_first_positive_term(const ecs_query_t *query) {
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_positive(query->terms[i])) {
            return query->terms[i].id;
        }
    }
    return 0;
}

void ecs_query_index_update_matches(ecs_query_cache_t *query_cache) {
    uint16_t component = ecs_query_first_positive_term(&query_cache->query);

    if (ECS_LIKELY(component)) {
        const ecs_vec_t *tables_vec =
            &ecs_component_index_get(&ecs_world.component_index, component)->tables;

        ecs_vec_iter(tables_vec, uint16_t, table_index, {
            const ecs_table_t *table = &ecs_world.table_index.tables[*table_index];

            if (ecs_query_match_table(&query_cache->query, table)) {
                ecs_query_cache_add_table(query_cache, table, *table_index);
            }
        });
    } else {
        const uint16_t table_count = ecs_world.table_index.table_count;
        const ecs_table_t *tables = ecs_world.table_index.tables;

        for (uint16_t i = 0; i < table_count; i++) {
            if (ecs_query_match_table(&query_cache->query, &tables[i])) {
                ecs_query_cache_add_table(query_cache, &tables[i], i);
            }
        }
    }
}

void ecs_query_index_add_table(const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = ecs_world.query_index.active_ids.data;
    for (uint32_t i = 0; i < ecs_world.query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            ecs_vec_get_mut(&ecs_world.query_index.queries, active_ids[i], ecs_query_cache_t);
        if (ecs_query_match_table(&cache->query, table)) {
            ecs_query_cache_add_table(cache, table, table_id);
        }
    }
}

static uint64_t ecs_resource_storage_size(const ecs_resource_desc_t *record) {
    return record->size ? record->size : 1;
}

static void ecs_resource_value_copy_ctor(
    const ecs_resource_desc_t *record,
    void *dst,
    const void *src
) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, 1);
        return;
    }
    memcpy(dst, src, record->size);
}

static void ecs_resource_value_copy(const ecs_resource_desc_t *record, void *dst, const void *src) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.copy) {
        record->ops.copy(dst, src, 1);
        return;
    }
    memcpy(dst, src, record->size);
}

static void ecs_resource_value_move_ctor(ecs_resource_desc_t *record, void *dst, void *src) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.move_ctor) {
        record->ops.move_ctor(dst, src, 1);
        return;
    }
    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, 1);
        if (record->ops.dtor) {
            record->ops.dtor(src, 1);
        }
        return;
    }
    memcpy(dst, src, record->size);
}

static void ecs_resource_value_move(ecs_resource_desc_t *record, void *dst, void *src) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.move) {
        record->ops.move(dst, src, 1);
        return;
    }
    if (record->ops.copy) {
        record->ops.copy(dst, src, 1);
        if (record->ops.dtor) {
            record->ops.dtor(src, 1);
        }
        return;
    }
    memcpy(dst, src, record->size);
}

static void ecs_resource_value_dtor(const ecs_resource_desc_t *record, void *ptr) {
    if (record->size != 0 && record->ops.dtor) {
        record->ops.dtor(ptr, 1);
    }
}

static void
ecs_resource_index_assert_registered(const ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_assert(
        id != 0 && id < index->count && id < index->capacity && index->records[id].name != NULL,
        "invalid resource id: %d\n",
        id
    );
}

static void ecs_resource_index_ensure(ecs_resource_index_t *index, ecs_resource_t id) {
    if (id < index->capacity) {
        return;
    }

    uint64_t capacity = index->capacity ? index->capacity : 16;
    while (id >= capacity) {
        capacity *= 2;
    }

    ecs_resource_desc_t *records = realloc(index->records, sizeof(ecs_resource_desc_t) * capacity);
    ecs_assert_not_null(records);
    void **data = realloc(index->data, sizeof(void *) * capacity);
    ecs_assert_not_null(data);
    bool *present = realloc(index->present, sizeof(bool) * capacity);
    ecs_assert_not_null(present);

    for (uint64_t i = index->capacity; i < capacity; i++) {
        records[i] = (ecs_resource_desc_t){ 0 };
        data[i] = NULL;
        present[i] = false;
    }

    index->records = records;
    index->data = data;
    index->present = present;
    index->capacity = capacity;
}

void ecs_resource_index_init(ecs_resource_index_t *index) {
    index->records = NULL;
    index->data = NULL;
    index->present = NULL;
    index->capacity = 0;
    index->count = 1;
}

void ecs_resource_index_fini(ecs_resource_index_t *index) {
    for (uint64_t id = 1; id < index->capacity; id++) {
        if (!index->present[id]) {
            continue;
        }

        const ecs_resource_desc_t *record = &index->records[id];
        if (record->on_remove) {
            record->on_remove(index->data[id]);
        }
        ecs_resource_value_dtor(record, index->data[id]);
        free(index->data[id]);
    }

    free(index->records);
    free(index->data);
    free(index->present);
}

ecs_resource_t
ecs_resource_index_register(ecs_resource_index_t *index, ecs_resource_t id, const ecs_resource_desc_t *desc) {
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);
    ecs_assert_id_valid(id);

    ecs_resource_index_ensure(index, id);
    if (index->records[id].name != NULL) {
        return id;
    }
    index->records[id] = *desc;
    if (id >= index->count) {
        index->count = (uint64_t)id + 1;
    }
    return id;
}

ecs_resource_t ecs_resource_index_find(const ecs_resource_index_t *index, const char *name) {
    ecs_assert_not_null(name);
    for (uint32_t id = 1; id < index->count; id++) {
        if (index->records[id].name && strcmp(index->records[id].name, name) == 0) return id;
    }
    return 0;
}

bool ecs_resource_index_is_registered(const ecs_resource_index_t *index, ecs_resource_t id) {
    return id != 0 && id < index->count && id < index->capacity && index->records[id].name != NULL;
}

void ecs_resource_index_set(
    ecs_resource_index_t *index,
        ecs_resource_t id,
    const void *data
) {
    ecs_resource_index_assert_registered(index, id);

    const ecs_resource_desc_t *record = &index->records[id];
    bool was_present = index->present[id];
    if (!index->present[id]) {
        index->data[id] = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(index->data[id]);
        index->present[id] = true;
    }

    if (record->on_set) {
        record->on_set(data);
    }

    if (record->size != 0) {
        if (was_present) {
            ecs_resource_value_copy(record, index->data[id], data);
        } else {
            ecs_resource_value_copy_ctor(record, index->data[id], data);
        }
    }
}

void ecs_resource_index_move(
    ecs_resource_index_t *index,
        ecs_resource_t id,
    void *data
) {
    ecs_resource_index_assert_registered(index, id);

    ecs_resource_desc_t *record = &index->records[id];
    bool was_present = index->present[id];
    if (!index->present[id]) {
        index->data[id] = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(index->data[id]);
        index->present[id] = true;
    }

    if (record->on_set) {
        record->on_set(data);
    }

    if (record->size != 0) {
        if (was_present) {
            ecs_resource_value_move(record, index->data[id], data);
        } else {
            ecs_resource_value_move_ctor(record, index->data[id], data);
        }
    }
}

void *ecs_resource_index_get(ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    if (!index->present[id]) {
        return NULL;
    }

    return index->data[id];
}

bool ecs_resource_index_has(const ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    return index->present[id];
}

void ecs_resource_index_remove(ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    if (!index->present[id]) {
        return;
    }

    const ecs_resource_desc_t *record = &index->records[id];
    if (record->on_remove) {
        record->on_remove(index->data[id]);
    }
    ecs_resource_value_dtor(record, index->data[id]);

    free(index->data[id]);
    index->data[id] = NULL;
    index->present[id] = false;
}

static bool ecs_system_id_valid(const ecs_system_index_t *index, ecs_system_id_t system) {
    return system != 0 && system < index->systems.size;
}

static void ecs_system_index_plan_one(
    ecs_system_index_t *index,
    ecs_system_id_t system,
    uint8_t *state,
    ecs_vec_t *order
) {
    if (!ecs_system_id_valid(index, system)) {
        ecs_assert(false, "invalid system dependency: %u\n", system);
        return;
    }

    if (state[system] == 2) {
        return;
    }

    if (state[system] == 1) {
        ecs_assert(false, "system dependency cycle detected at system %u\n", system);
        return;
    }

    state[system] = 1;

    ecs_system_t *sys = ecs_system_index_get(index, system);
    for (uint32_t i = 0; i < ECS_SYSTEM_AFTER_CAPACITY; i++) {
        ecs_system_id_t after = sys->after[i];
        if (after == 0) {
            continue;
        }

        if (!ecs_system_id_valid(index, after)) {
            ecs_assert(false, "invalid system dependency: %u\n", after);
            continue;
        }

        ecs_system_t *dep = ecs_system_index_get(index, after);
        if (dep->phase != sys->phase) {
            ecs_assert(false, "system dependency must be in the same phase\n");
            continue;
        }

        ecs_system_index_plan_one(index, after, state, order);
    }

    state[system] = 2;

    if (sys->enabled) {
        ecs_vec_push_u16(order, system);
    }
}

void ecs_system_index_init(ecs_system_index_t *index) {
    ecs_vec_init(&index->systems, sizeof(ecs_system_t));
    ecs_vec_ensure(&index->systems, 1, sizeof(ecs_system_t));

    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        ecs_vec_init(&index->phase_order[i], sizeof(ecs_system_id_t));
    }

    index->plan_dirty = true;
}

ecs_system_id_t ecs_system_index_create(ecs_system_index_t *index, const ecs_system_t *system) {
    ecs_vec_push(&index->systems, system, sizeof(ecs_system_t));
    index->plan_dirty = true;
    return index->systems.size - 1;
}

ecs_system_t *ecs_system_index_get(ecs_system_index_t *index, ecs_system_id_t system) {
    ecs_assert(ecs_system_id_valid(index, system), "invalid system id: %u\n", system);
    return ecs_vec_get_mut(&index->systems, system, ecs_system_t);
}

void ecs_system_index_build_plan(ecs_system_index_t *index) {
    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        ecs_vec_clear(&index->phase_order[i]);
    }

    uint8_t *state = calloc(index->systems.size, sizeof(uint8_t));
    ecs_assert_not_null(state);

    for (uint32_t system = 1; system < index->systems.size; system++) {
        ecs_system_t *sys = ecs_system_index_get(index, system);
        ecs_assert(sys->phase < EcsPhaseCount, "invalid system phase: %u\n", sys->phase);

        if (sys->phase >= EcsPhaseCount) {
            continue;
        }

        ecs_system_index_plan_one(index, system, state, &index->phase_order[sys->phase]);
    }

    free(state);
    index->plan_dirty = false;
}

void ecs_system_index_fini(ecs_system_index_t *index) {
    ecs_system_t *systems = ecs_vec_data(&index->systems, ecs_system_t);
    for (uint32_t i = 1; i < index->systems.size; i++) {
        if (systems[i].user_data_dtor) {
            systems[i].user_data_dtor(systems[i].user_data);
        }
    }

    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        ecs_vec_fini(&index->phase_order[i]);
    }

    ecs_vec_fini(&index->systems);
}

#define INITIAL_SLOT_SHIFT 12
#define LOAD_FACTOR 0.75
#define ECS_TABLE_SLOT_EMPTY UINT16_MAX

static inline uint32_t ecs_type_hash(ecs_type_t type) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < type.count; ++i) {
        h ^= (uint32_t)type.ids[i];
        h *= 16777619u;
    }
    h ^= (uint32_t)type.base;
    h *= 16777619u;
    h ^= (uint32_t)(type.base >> 32);
    h *= 16777619u;

    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

static inline uint16_t ecs_type_hash_fingerprint(uint32_t hash) {
    return (uint16_t)(hash ^ (hash >> 16));
}

static inline uint32_t ecs_table_index_slot_count(const ecs_table_index_t *map) {
    return 1u << map->slot_shift;
}

static inline void ecs_table_index_init_slots(ecs_table_index_t *map) {
    uint32_t slot_count = ecs_table_index_slot_count(map);
    map->slots = malloc(sizeof(ecs_type_slot_t) * slot_count);
    memset(map->slots, 0xFF, sizeof(ecs_type_slot_t) * slot_count);
}

static inline void
ecs_table_index_insert_slot(ecs_table_index_t *map, uint32_t hash, uint16_t table_index) {
    uint32_t slot_mask = ecs_table_index_slot_count(map) - 1;
    uint32_t slot_idx = hash & slot_mask;
    while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
        slot_idx = (slot_idx + 1) & slot_mask;
    }
    map->slots[slot_idx].hash = ecs_type_hash_fingerprint(hash);
    map->slots[slot_idx].table_index = table_index;
}

void ecs_table_index_init(ecs_table_index_t *map) {
    map->table_count = 0;
    map->table_capacity = 1;
    map->tables = malloc(sizeof(ecs_table_t) * map->table_capacity);
    map->slot_shift = INITIAL_SLOT_SHIFT;
    ecs_table_index_init_slots(map);
}

void ecs_table_index_fini(ecs_table_index_t *map) {
    for (uint16_t i = 0; i < map->table_count; i++) {
        ecs_table_fini(&map->tables[i]);
    }
    free(map->tables);
    free(map->slots);
}

static void ecs_table_index_resize(ecs_table_index_t *map) {
    ecs_type_slot_t *old_slots = map->slots;

    map->slot_shift += 1;
    ecs_table_index_init_slots(map);
    for (uint16_t i = 0; i < map->table_count; ++i) {
        ecs_table_index_insert_slot(map, ecs_type_hash(map->tables[i].type), i);
    }
    free(old_slots);
}

static void ecs_table_index_grow_tables(ecs_table_index_t *map) {
    map->table_capacity *= 2;
    map->tables = realloc(map->tables, sizeof(ecs_table_t) * map->table_capacity);
}

static bool ecs_table_index_inherits_component_before(
    const ecs_table_t *table,
    ecs_entity_t stop_base,
    ecs_component_t component
) {
    ecs_entity_t base = table->type.base;
    while (base != 0 && base != stop_base) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);
        if (ecs_table_column_or_invalid(base_table, component) != UINT16_MAX) {
            return true;
        }
        base = base_table->type.base;
    }
    return false;
}

static void ecs_table_index_register_inherited_components(
        ecs_table_t *table,
    uint16_t table_id
) {
    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);

        for (uint16_t i = 0; i < base_table->type.count; i++) {
            ecs_component_t component = base_table->type.ids[i];
            if (ecs_table_column_or_invalid(table, component) != UINT16_MAX ||
                ecs_table_index_inherits_component_before(table, base, component)) {
                continue;
            }

            table->bloom |= 1ull << (component % 64);
            ecs_component_record_t *record =
                ecs_component_index_get_mut(&ecs_world.component_index, component);
            ecs_vec_push_u16(&record->tables, table_id);
        }

        base = base_table->type.base;
    }
}

uint16_t ecs_table_index_get_or_create(ecs_type_t type) {
    const ecs_component_index_t *component_index = &ecs_world.component_index;
    ecs_table_index_t *map = &ecs_world.table_index;
    uint32_t hash = ecs_type_hash(type);
    uint16_t hash_fingerprint = ecs_type_hash_fingerprint(hash);
    uint32_t slot_mask = ecs_table_index_slot_count(map) - 1;
    uint32_t slot_idx = hash & slot_mask;

    // Fast path: lookup
    while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
        if (ECS_LIKELY(map->slots[slot_idx].hash == hash_fingerprint)) {
            const ecs_table_t *table = ecs_table_index_at(map, map->slots[slot_idx].table_index);
            if (ECS_LIKELY(ecs_type_equals(&table->type, &type))) {
                ecs_type_fini(&type);
                return (uint16_t)map->slots[slot_idx].table_index;
            }
        }
        slot_idx = (slot_idx + 1) & slot_mask;
    }

    // Slow path: creation
    if (ECS_UNLIKELY(map->table_count >= ecs_table_index_slot_count(map) * LOAD_FACTOR)) {
        ecs_table_index_resize(map);
        // re-calculate slot_idx after resize
        slot_mask = ecs_table_index_slot_count(map) - 1;
        slot_idx = hash & slot_mask;
    }
    if (ECS_UNLIKELY(map->table_count >= map->table_capacity)) {
        ecs_table_index_grow_tables(map);
    }

    uint16_t table_idx = map->table_count++;
    ecs_table_t new_table;
    ecs_table_init(&new_table, type, component_index, table_idx);
    map->tables[table_idx] = new_table;
    ecs_table_index_register_inherited_components(&map->tables[table_idx], table_idx);

    map->slots[slot_idx].hash = hash_fingerprint;
    map->slots[slot_idx].table_index = table_idx;

    ecs_query_index_add_table(ecs_table_index_at(map, table_idx), table_idx);
    ecs_observer_index_add_table(ecs_table_index_at(map, table_idx));
    return (uint16_t)table_idx;
}

