/* Derived from Flecs. Copyright (c) 2025 Sander Mertens.
 * Portions Copyright (c) Meta Platforms, Inc. and affiliates.
 * Distributed under the MIT license in addons/script/LICENSE.flecs. */
/* Typed operator kernels ported from Flecs expr/util.c (MIT). */
#include "../script.h"

#define SIECS_VALUE_COMPARE(T)\
    case EcsTokEq: *(bool*)out = l == r; return 0;\
    case EcsTokNeq: *(bool*)out = l != r; return 0;\
    case EcsTokGt: *(bool*)out = l > r; return 0;\
    case EcsTokGtEq: *(bool*)out = l >= r; return 0;\
    case EcsTokLt: *(bool*)out = l < r; return 0;\
    case EcsTokLtEq: *(bool*)out = l <= r; return 0;

#define SIECS_VALUE_INTEGER(T)\
    case EcsTokMod: *(T*)out = (T)(l % r); return 0;\
    case EcsTokBitwiseAnd: *(T*)out = (T)(l & r); return 0;\
    case EcsTokBitwiseOr: *(T*)out = (T)(l | r); return 0;\
    case EcsTokShiftLeft: *(T*)out = (T)(l << r); return 0;\
    case EcsTokShiftRight: *(T*)out = (T)(l >> r); return 0;

#define SIECS_VALUE_FLOAT(T)

#define SIECS_VALUE_ARITHMETIC(T)\
    case EcsTokAdd: *(T*)out = (T)(l + r); return 0;\
    case EcsTokSub: *(T*)out = (T)(l - r); return 0;\
    case EcsTokMul: *(T*)out = (T)(l * r); return 0;\
    case EcsTokDiv: *(T*)out = (T)(l / r); return 0;

#define SIECS_VALUE_BINARY(T, ARITHMETIC, INTEGER)\
int script_operator_##T(\
    const void *left, const void *right, void *out, ecs_token_kind_t operator)\
{\
    T l = *(const T*)left, r = *(const T*)right;\
    switch (operator) {\
    SIECS_VALUE_COMPARE(T)\
    ARITHMETIC(T)\
    INTEGER(T)\
    default: return -1;\
    }\
    return -1;\
}

SIECS_VALUE_BINARY(int64_t, SIECS_VALUE_ARITHMETIC, SIECS_VALUE_INTEGER)
SIECS_VALUE_BINARY(uint64_t, SIECS_VALUE_ARITHMETIC, SIECS_VALUE_INTEGER)
SIECS_VALUE_BINARY(double, SIECS_VALUE_ARITHMETIC, SIECS_VALUE_FLOAT)
SIECS_VALUE_BINARY(bool, SIECS_VALUE_FLOAT, SIECS_VALUE_FLOAT)

#undef SIECS_VALUE_BINARY
#undef SIECS_VALUE_ARITHMETIC
#undef SIECS_VALUE_INTEGER
#undef SIECS_VALUE_FLOAT
#undef SIECS_VALUE_COMPARE
