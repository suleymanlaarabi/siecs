#include "../script.h"

static int compare_numbers(long double left, long double right, bool *result, int operation) {
    switch (operation) {
    case EcsTokEq:
        *result = left == right;
        break;
    case EcsTokNeq:
        *result = left != right;
        break;
    case EcsTokGt:
        *result = left > right;
        break;
    case EcsTokGtEq:
        *result = left >= right;
        break;
    case EcsTokLt:
        *result = left < right;
        break;
    case EcsTokLtEq:
        *result = left <= right;
        break;
    default:
        return -1;
    }
    return 0;
}

int script_operator_double(
    const void *left,
    const void *right,
    void *out,
    ecs_token_kind_t operation
) {
    double a = *(const double *)left, b = *(const double *)right, value;
    switch (operation) {
    case '+':
        value = a + b;
        break;
    case '-':
        value = a - b;
        break;
    case '*':
        value = a * b;
        break;
    case '/':
        value = a / b;
        break;
    default:
        return compare_numbers(a, b, out, operation);
    }
    memcpy(out, &value, sizeof value);
    return 0;
}

int script_operator_uint64_t(
    const void *left,
    const void *right,
    void *out,
    ecs_token_kind_t operation
) {
    uint64_t a = *(const uint64_t *)left, b = *(const uint64_t *)right, value;
    switch (operation) {
    case '+':
        value = a + b;
        break;
    case '-':
        value = a - b;
        break;
    case '*':
        value = a * b;
        break;
    case '/':
        value = a / b;
        break;
    case '%':
        value = a % b;
        break;
    case '|':
        value = a | b;
        break;
    case '&':
        value = a & b;
        break;
    case EcsTokShiftLeft:
        value = a << b;
        break;
    case EcsTokShiftRight:
        value = a >> b;
        break;
    default:
        return compare_numbers(a, b, out, operation);
    }
    memcpy(out, &value, sizeof value);
    return 0;
}

int script_operator_int64_t(
    const void *left,
    const void *right,
    void *out,
    ecs_token_kind_t operation
) {
    int64_t a = *(const int64_t *)left, b = *(const int64_t *)right, value;
    switch (operation) {
    case '+':
        value = a + b;
        break;
    case '-':
        value = a - b;
        break;
    case '*':
        value = a * b;
        break;
    case '/':
        value = a / b;
        break;
    case '%':
        value = a % b;
        break;
    case '|':
        value = a | b;
        break;
    case '&':
        value = a & b;
        break;
    case EcsTokShiftLeft:
        value = a << b;
        break;
    case EcsTokShiftRight:
        value = a >> b;
        break;
    default:
        return compare_numbers(a, b, out, operation);
    }
    memcpy(out, &value, sizeof value);
    return 0;
}
