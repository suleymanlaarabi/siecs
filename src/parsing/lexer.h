#ifndef ECS_LEXER_H
#define ECS_LEXER_H

#include "datastructure/vec.h"

typedef enum {
    EcsTokEnd = '\0',
    EcsTokUnknown,
    EcsTokScopeOpen = '{',
    EcsTokScopeClose = '}',
    EcsTokParenOpen = '(',
    EcsTokParenClose = ')',
    EcsTokBracketOpen = '[',
    EcsTokBracketClose = ']',
    EcsTokMember = '.',
    EcsTokComma = ',',
    EcsTokSemiColon = ';',
    EcsTokColon = ':',
    EcsTokAssign = '=',
    EcsTokAdd = '+',
    EcsTokSub = '-',
    EcsTokMul = '*',
    EcsTokDiv = '/',
    EcsTokMod = '%',
    EcsTokBitwiseOr = '|',
    EcsTokBitwiseAnd = '&',
    EcsTokNot = '!',
    EcsTokOptional = '?',
    EcsTokEq = 100,              // ==
    EcsTokNeq = 101,             // !=
    EcsTokGt = 102,              // >
    EcsTokGtEq = 103,            // >=
    EcsTokLt = 104,              // <
    EcsTokLtEq = 105,            // <=
    EcsTokAnd = 106,             // &&
    EcsTokOr = 107,              // ||
    EcsTokMatch = 108,           // ~=
    EcsTokRange = 109,           // ..
    EcsTokShiftLeft = 110,       // <<
    EcsTokShiftRight = 111,      // >>
    EcsTokIdentifier = 112,      // identifier
    EcsTokFunction = 113,        // function
    EcsTokString = 114,          // string literal
    EcsTokNumber = 115,          // number literal
    EcsTokKeywordModule = 116,   // module
    EcsTokKeywordUsing = 117,    // using
    EcsTokKeywordWith = 118,     // with
    EcsTokKeywordIf = 119,       // if
    EcsTokKeywordFor = 120,      // for
    EcsTokKeywordIn = 121,       // in
    EcsTokKeywordElse = 122,     // else
    EcsTokKeywordTemplate = 130, // template
    EcsTokKeywordProp = 131,     // prop
    EcsTokKeywordConst = 132,    // const
    EcsTokKeywordMatch = 133,    // match
    EcsTokKeywordNew = 134,      // new
    EcsTokKeywordExport = 135,   // export
    EcsTokKeywordInclude = 138,  // include
    EcsTokKeywordFn = 139,       // fn
    EcsTokArrow = 140,           // =>
    EcsTokAddAssign = 136,       // +=
    EcsTokMulAssign = 137,       // *=
} ecs_token_type_t;

typedef struct {
    const char *data;
    uint32_t len;
} ecs_token_slice_t;

typedef struct {
    ecs_token_type_t type;
    union {
        ecs_token_slice_t str;
        double number;
        char character;
    } data;
} ecs_token_t;

void ecs_lexer_lex(const char *str, ecs_vec_t *tokens); // tokens = ecs_token_t

#endif
