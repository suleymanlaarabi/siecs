#ifndef ECS_PARSING_SCANNER_H
#define ECS_PARSING_SCANNER_H

#include "../datastructure/string.h"
#include "../datastructure/vec.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    const char *str;
    uint32_t pos;
    uint32_t len;
} ecs_scanner_t;

void ecs_scanner_init(ecs_scanner_t *scanner, const char *str);

static inline bool ecs_scanner_is_done(const ecs_scanner_t *scanner) {
    return scanner->pos >= scanner->len;
}

static inline void ecs_scanner_advance(ecs_scanner_t *scanner) { scanner->pos += 1; }

static inline char ecs_scanner_peek(const ecs_scanner_t *scanner) {
    return scanner->str[scanner->pos];
}

static inline char ecs_scanner_pop(ecs_scanner_t *scanner) {
    char letter = ecs_scanner_peek(scanner);
    ecs_scanner_advance(scanner);
    return letter;
}

typedef int (*ecs_scanner_cmp_t)(int);

static inline void ecs_scanner_skip_while(ecs_scanner_t *scanner, const ecs_scanner_cmp_t cmp) {
    while (!ecs_scanner_is_done(scanner) && cmp(ecs_scanner_peek(scanner))) {
        ecs_scanner_advance(scanner);
    }
}

static inline void ecs_scanner_skip_whitespace(ecs_scanner_t *scanner) {
    ecs_scanner_skip_while(scanner, isblank);
}

static inline ecs_str_t
ecs_scanner_take_while(ecs_scanner_t *scanner, const ecs_scanner_cmp_t cmp) {
    ecs_str_t str = ecs_str_new();

    while (!ecs_scanner_is_done(scanner) && cmp(ecs_scanner_peek(scanner))) {
        ecs_str_char_append(&str, ecs_scanner_pop(scanner));
    }

    return str;
}

static inline bool ecs_is_identifier_start(int c) { return isalpha(c) || c == '_'; }

static inline bool ecs_is_identifier_part(int c) { return isalnum(c) || c == '_'; }

static inline ecs_str_t ecs_scanner_take_identifier(ecs_scanner_t *scanner) {
    ecs_str_t str = ecs_str_new();

    if (ecs_scanner_is_done(scanner) || !ecs_is_identifier_start(ecs_scanner_peek(scanner))) {
        return str;
    }

    ecs_str_char_append(&str, ecs_scanner_pop(scanner));

    while (!ecs_scanner_is_done(scanner) && ecs_is_identifier_part(ecs_scanner_peek(scanner))) {
        ecs_str_char_append(&str, ecs_scanner_pop(scanner));
    }

    return str;
}

static inline char ecs_scanner_peek_next(const ecs_scanner_t *scanner) {
    if (scanner->pos + 1 >= scanner->len) {
        return '\0';
    }
    return scanner->str[scanner->pos + 1];
}

static inline bool ecs_scanner_match(ecs_scanner_t *scanner, char expected) {
    if (ecs_scanner_is_done(scanner)) {
        return false;
    }
    return scanner->str[scanner->pos] == expected;
}

static inline const char *ecs_scanner_current_ptr(const ecs_scanner_t *scanner) {
    return scanner->str + scanner->pos;
}

static inline void ecs_scanner_advance_n(ecs_scanner_t *scanner, uint64_t count) {
    scanner->pos += count;
}

#endif
