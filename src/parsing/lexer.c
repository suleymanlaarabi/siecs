#include "lexer.h"
#include "scanner.h"

#include <string.h>

static inline void ecs_lexer_push(ecs_vec_t *tokens, ecs_token_type_t type) {
    ecs_token_t *token = ecs_vec_push_empty(tokens, sizeof(ecs_token_t));
    token->type = type;
}

static inline void ecs_lexer_push_char(ecs_vec_t *tokens, ecs_token_type_t type, char value) {
    ecs_token_t *token = ecs_vec_push_empty(tokens, sizeof(ecs_token_t));
    token->type = type;
    token->data.character = value;
}

static inline void
ecs_lexer_push_slice(ecs_vec_t *tokens, ecs_token_type_t type, const char *data, uint32_t len) {
    ecs_token_t *token = ecs_vec_push_empty(tokens, sizeof(ecs_token_t));
    token->type = type;
    token->data.str = (ecs_token_slice_t){ data, len };
}

static inline void ecs_lexer_push_number(ecs_vec_t *tokens, double value) {
    ecs_token_t *token = ecs_vec_push_empty(tokens, sizeof(ecs_token_t));
    token->type = EcsTokNumber;
    token->data.number = value;
}

static inline bool ecs_lexer_slice_eq(const char *data, uint32_t len, const char *keyword) {
    uint32_t keyword_len = (uint32_t)strlen(keyword);
    return len == keyword_len && memcmp(data, keyword, len) == 0;
}

static ecs_token_type_t ecs_lexer_keyword_type(const char *data, uint32_t len) {
    switch (len) {
    case 2:
        if (ecs_lexer_slice_eq(data, len, "if"))
            return EcsTokKeywordIf;
        if (ecs_lexer_slice_eq(data, len, "in"))
            return EcsTokKeywordIn;
        if (ecs_lexer_slice_eq(data, len, "fn"))
            return EcsTokKeywordFn;
        break;
    case 3:
        if (ecs_lexer_slice_eq(data, len, "for"))
            return EcsTokKeywordFor;
        if (ecs_lexer_slice_eq(data, len, "new"))
            return EcsTokKeywordNew;
        break;
    case 4:
        if (ecs_lexer_slice_eq(data, len, "with"))
            return EcsTokKeywordWith;
        if (ecs_lexer_slice_eq(data, len, "else"))
            return EcsTokKeywordElse;
        if (ecs_lexer_slice_eq(data, len, "prop"))
            return EcsTokKeywordProp;
        break;
    case 5:
        if (ecs_lexer_slice_eq(data, len, "using"))
            return EcsTokKeywordUsing;
        if (ecs_lexer_slice_eq(data, len, "const"))
            return EcsTokKeywordConst;
        if (ecs_lexer_slice_eq(data, len, "match"))
            return EcsTokKeywordMatch;
        break;
    case 6:
        if (ecs_lexer_slice_eq(data, len, "module"))
            return EcsTokKeywordModule;
        if (ecs_lexer_slice_eq(data, len, "export"))
            return EcsTokKeywordExport;
        break;
    case 7:
        if (ecs_lexer_slice_eq(data, len, "include"))
            return EcsTokKeywordInclude;
        break;
    case 8:
        if (ecs_lexer_slice_eq(data, len, "function"))
            return EcsTokFunction;
        if (ecs_lexer_slice_eq(data, len, "template"))
            return EcsTokKeywordTemplate;
        break;
    }
    return EcsTokIdentifier;
}

static void ecs_lexer_lex_identifier(ecs_scanner_t *scanner, ecs_vec_t *tokens) {
    const char *start = ecs_scanner_current_ptr(scanner);
    uint32_t start_pos = scanner->pos;

    ecs_scanner_advance(scanner);
    while (!ecs_scanner_is_done(scanner) && ecs_is_identifier_part(ecs_scanner_peek(scanner))) {
        ecs_scanner_advance(scanner);
    }

    uint32_t len = scanner->pos - start_pos;
    ecs_lexer_push_slice(tokens, ecs_lexer_keyword_type(start, len), start, len);
}

static void ecs_lexer_lex_number(ecs_scanner_t *scanner, ecs_vec_t *tokens) {
    double value = 0.0;

    while (!ecs_scanner_is_done(scanner) && isdigit(ecs_scanner_peek(scanner))) {
        value = value * 10.0 + (double)(ecs_scanner_peek(scanner) - '0');
        ecs_scanner_advance(scanner);
    }

    if (!ecs_scanner_is_done(scanner) && ecs_scanner_peek(scanner) == '.' &&
        isdigit(ecs_scanner_peek_next(scanner))) {
        double place = 0.1;
        ecs_scanner_advance(scanner);
        while (!ecs_scanner_is_done(scanner) && isdigit(ecs_scanner_peek(scanner))) {
            value += (double)(ecs_scanner_peek(scanner) - '0') * place;
            place *= 0.1;
            ecs_scanner_advance(scanner);
        }
    }

    if (!ecs_scanner_is_done(scanner) &&
        (ecs_scanner_peek(scanner) == 'e' || ecs_scanner_peek(scanner) == 'E')) {
        uint32_t pos = scanner->pos + 1;
        bool negative = false;

        if (pos < scanner->len && (scanner->str[pos] == '+' || scanner->str[pos] == '-')) {
            negative = scanner->str[pos] == '-';
            pos++;
        }

        if (pos < scanner->len && isdigit(scanner->str[pos])) {
            uint32_t exponent = 0;
            double scale = 1.0;

            scanner->pos = pos;
            while (!ecs_scanner_is_done(scanner) && isdigit(ecs_scanner_peek(scanner))) {
                exponent = exponent * 10 + (uint32_t)(ecs_scanner_peek(scanner) - '0');
                ecs_scanner_advance(scanner);
            }

            while (exponent-- > 0) {
                scale *= 10.0;
            }

            value = negative ? value / scale : value * scale;
        }
    }

    ecs_lexer_push_number(tokens, value);
}

static void ecs_lexer_lex_string(ecs_scanner_t *scanner, ecs_vec_t *tokens) {
    ecs_scanner_advance(scanner);

    const char *start = ecs_scanner_current_ptr(scanner);
    uint32_t start_pos = scanner->pos;

    while (!ecs_scanner_is_done(scanner) && ecs_scanner_peek(scanner) != '"') {
        if (ecs_scanner_peek(scanner) == '\\' && ecs_scanner_peek_next(scanner) != '\0') {
            ecs_scanner_advance_n(scanner, 2);
        } else {
            ecs_scanner_advance(scanner);
        }
    }

    uint32_t len = scanner->pos - start_pos;
    if (!ecs_scanner_is_done(scanner)) {
        ecs_scanner_advance(scanner);
    }

    ecs_lexer_push_slice(tokens, EcsTokString, start, len);
}

static bool ecs_lexer_try_two_char(
    ecs_scanner_t *scanner,
    ecs_vec_t *tokens,
    char next,
    ecs_token_type_t type
) {
    if (ecs_scanner_peek_next(scanner) != next) {
        return false;
    }
    ecs_lexer_push(tokens, type);
    ecs_scanner_advance_n(scanner, 2);
    return true;
}

void ecs_lexer_lex(const char *str, ecs_vec_t *tokens) {
    ecs_scanner_t scanner;
    ecs_scanner_init(&scanner, str);

    while (!ecs_scanner_is_done(&scanner)) {
        ecs_scanner_skip_while(&scanner, isspace);
        if (ecs_scanner_is_done(&scanner)) {
            break;
        }

        char c = ecs_scanner_peek(&scanner);

        if (ecs_is_identifier_start(c)) {
            ecs_lexer_lex_identifier(&scanner, tokens);
            continue;
        }

        if (isdigit(c)) {
            ecs_lexer_lex_number(&scanner, tokens);
            continue;
        }

        if (c == '"') {
            ecs_lexer_lex_string(&scanner, tokens);
            continue;
        }

        switch (c) {
        case '{':
        case '}':
        case '(':
        case ')':
        case '[':
        case ']':
        case ',':
        case ';':
        case ':':
        case '/':
        case '%':
        case '?':
            ecs_lexer_push(tokens, (ecs_token_type_t)c);
            ecs_scanner_advance(&scanner);
            break;
        case '.':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '.', EcsTokRange)) {
                ecs_lexer_push(tokens, EcsTokMember);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '=':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokEq) &&
                !ecs_lexer_try_two_char(&scanner, tokens, '>', EcsTokArrow)) {
                ecs_lexer_push(tokens, EcsTokAssign);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '+':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokAddAssign)) {
                ecs_lexer_push(tokens, EcsTokAdd);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '-':
            ecs_lexer_push(tokens, EcsTokSub);
            ecs_scanner_advance(&scanner);
            break;
        case '*':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokMulAssign)) {
                ecs_lexer_push(tokens, EcsTokMul);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '|':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '|', EcsTokOr)) {
                ecs_lexer_push(tokens, EcsTokBitwiseOr);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '&':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '&', EcsTokAnd)) {
                ecs_lexer_push(tokens, EcsTokBitwiseAnd);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '!':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokNeq)) {
                ecs_lexer_push(tokens, EcsTokNot);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '~':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokMatch)) {
                ecs_lexer_push_char(tokens, EcsTokUnknown, c);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '<':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokLtEq) &&
                !ecs_lexer_try_two_char(&scanner, tokens, '<', EcsTokShiftLeft)) {
                ecs_lexer_push(tokens, EcsTokLt);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '>':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokGtEq) &&
                !ecs_lexer_try_two_char(&scanner, tokens, '>', EcsTokShiftRight)) {
                ecs_lexer_push(tokens, EcsTokGt);
                ecs_scanner_advance(&scanner);
            }
            break;
        default:
            ecs_lexer_push_char(tokens, EcsTokUnknown, c);
            ecs_scanner_advance(&scanner);
            break;
        }
    }

    ecs_lexer_push(tokens, EcsTokEnd);
}
