#include "../../src/parsing/lexer.h"
#include <siecs_test.h>
#include <string.h>

static ecs_vec_t lex(const char *source) {
    ecs_vec_t tokens;
    ecs_vec_init(&tokens, sizeof(ecs_token_t));
    ecs_lexer_lex(source, &tokens);
    return tokens;
}

static const ecs_token_t *tok(const ecs_vec_t *tokens, uint32_t index) {
    return ecs_vec_get(tokens, index, ecs_token_t);
}

static void expect_type(const ecs_vec_t *tokens, uint32_t index, ecs_token_type_t type) {
    test_assert(tok(tokens, index)->type == type);
}

static void expect_slice(const ecs_vec_t *tokens, uint32_t index, const char *value) {
    const ecs_token_slice_t slice = tok(tokens, index)->data.str;
    test_assert(slice.len == strlen(value));
    test_assert(memcmp(slice.data, value, slice.len) == 0);
}

void lexer_single_char_tokens(void) {
    ecs_vec_t tokens = lex("{}()[] . , ; : = + - * / % | & ! ?");

    ecs_token_type_t expected[] = {
        EcsTokScopeOpen,    EcsTokScopeClose, EcsTokParenOpen,  EcsTokParenClose, EcsTokBracketOpen,
        EcsTokBracketClose, EcsTokMember,     EcsTokComma,      EcsTokSemiColon,  EcsTokColon,
        EcsTokAssign,       EcsTokAdd,        EcsTokSub,        EcsTokMul,        EcsTokDiv,
        EcsTokMod,          EcsTokBitwiseOr,  EcsTokBitwiseAnd, EcsTokNot,        EcsTokOptional,
        EcsTokEnd,
    };

    test_assert(tokens.size == sizeof(expected) / sizeof(expected[0]));
    for (uint32_t i = 0; i < tokens.size; i++) {
        expect_type(&tokens, i, expected[i]);
    }

    ecs_vec_fini(&tokens);
}

void lexer_two_char_tokens(void) {
    ecs_vec_t tokens = lex("== != >= <= && || ~= .. << >> => += *=");

    ecs_token_type_t expected[] = {
        EcsTokEq,    EcsTokNeq,       EcsTokGtEq,      EcsTokLtEq,      EcsTokAnd,
        EcsTokOr,    EcsTokMatch,     EcsTokRange,     EcsTokShiftLeft, EcsTokShiftRight,
        EcsTokArrow, EcsTokAddAssign, EcsTokMulAssign, EcsTokEnd,
    };

    test_assert(tokens.size == sizeof(expected) / sizeof(expected[0]));
    for (uint32_t i = 0; i < tokens.size; i++) {
        expect_type(&tokens, i, expected[i]);
    }

    ecs_vec_fini(&tokens);
}

void lexer_keywords_and_identifiers(void) {
    ecs_vec_t tokens =
        lex("module using with if for in else function template prop const match new export "
            "include fn name "
            "_x x42");

    ecs_token_type_t expected[] = {
        EcsTokKeywordModule,   EcsTokKeywordUsing,  EcsTokKeywordWith,    EcsTokKeywordIf,
        EcsTokKeywordFor,      EcsTokKeywordIn,     EcsTokKeywordElse,    EcsTokFunction,
        EcsTokKeywordTemplate, EcsTokKeywordProp,   EcsTokKeywordConst,   EcsTokKeywordMatch,
        EcsTokKeywordNew,      EcsTokKeywordExport, EcsTokKeywordInclude, EcsTokKeywordFn,
        EcsTokIdentifier,      EcsTokIdentifier,    EcsTokIdentifier,     EcsTokEnd,
    };

    test_assert(tokens.size == sizeof(expected) / sizeof(expected[0]));
    for (uint32_t i = 0; i < tokens.size; i++) {
        expect_type(&tokens, i, expected[i]);
    }
    expect_slice(&tokens, 16, "name");
    expect_slice(&tokens, 17, "_x");
    expect_slice(&tokens, 18, "x42");

    ecs_vec_fini(&tokens);
}

void lexer_numbers(void) {
    ecs_vec_t tokens = lex("0 42 3.14 2e3 4.5e-2 1..10");

    expect_type(&tokens, 0, EcsTokNumber);
    test_assert(tok(&tokens, 0)->data.number == 0.0);
    expect_type(&tokens, 1, EcsTokNumber);
    test_assert(tok(&tokens, 1)->data.number == 42.0);
    expect_type(&tokens, 2, EcsTokNumber);
    test_assert(tok(&tokens, 2)->data.number == 3.14);
    expect_type(&tokens, 3, EcsTokNumber);
    test_assert(tok(&tokens, 3)->data.number == 2000.0);
    expect_type(&tokens, 4, EcsTokNumber);
    test_assert(tok(&tokens, 4)->data.number == 0.045);

    expect_type(&tokens, 5, EcsTokNumber);
    test_assert(tok(&tokens, 5)->data.number == 1.0);
    expect_type(&tokens, 6, EcsTokRange);
    expect_type(&tokens, 7, EcsTokNumber);
    test_assert(tok(&tokens, 7)->data.number == 10.0);
    expect_type(&tokens, 8, EcsTokEnd);

    ecs_vec_fini(&tokens);
}

void lexer_strings(void) {
    ecs_vec_t tokens = lex("\"hello\" \"a\\\"b\" \"unterminated");

    expect_type(&tokens, 0, EcsTokString);
    expect_slice(&tokens, 0, "hello");
    expect_type(&tokens, 1, EcsTokString);
    expect_slice(&tokens, 1, "a\\\"b");
    expect_type(&tokens, 2, EcsTokString);
    expect_slice(&tokens, 2, "unterminated");
    expect_type(&tokens, 3, EcsTokEnd);

    ecs_vec_fini(&tokens);
}

void lexer_unknown(void) {
    ecs_vec_t tokens = lex("@");

    expect_type(&tokens, 0, EcsTokUnknown);
    test_assert(tok(&tokens, 0)->data.character == '@');
    expect_type(&tokens, 1, EcsTokEnd);

    ecs_vec_fini(&tokens);
}
