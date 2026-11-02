#include "eval.h"
#include <math.h>
#include <inttypes.h>

int script_eval_error(script_eval_t *ctx, const char *pos, const char *format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof message, format, args);
    va_end(args);
    if (!pos) pos = ctx->position;
    ecs_parser_error(ctx->script->name, ctx->script->code,
        pos ? pos - ctx->script->code : 0, "%s", message);
    return -1;
}

static const char *literal_types[] = {"bool", "char", "int64_t", "uint64_t", "double", "char *"};

static bool value_is_unsigned(sireflect_kind_t kind) {
    switch (kind) {
    case sireflect_kind_u8: case sireflect_kind_u16: case sireflect_kind_u32:
    case sireflect_kind_u64: case sireflect_kind_unsigned_char:
    case sireflect_kind_unsigned_short: case sireflect_kind_unsigned_int:
    case sireflect_kind_unsigned_long: case sireflect_kind_unsigned_long_long:
        return true;
    default: return false;
    }
}

static int value_number(const script_value_t *value, long double *number) {
    const sireflect_type_info_t *type = sireflect_type_info(value->type);
    if (!type) return -1;
    const void *data = script_value_data(value);
    switch (type->kind) {
    case sireflect_kind_f64: *number = *(const double*)data; return 0;
    case sireflect_kind_f32: {
        float number_f; memcpy(&number_f, data, sizeof number_f); *number = number_f; return 0;
    }
    case sireflect_kind_bool: *number = *(const bool*)data; return 0;
    case sireflect_kind_char: *number = *(const char*)data; return 0;
    default: break;
    }
    if (!sireflect_is_numeric(type->kind) && type->kind != sireflect_kind_enum) return -1;
    if (value_is_unsigned(type->kind)) {
        uint64_t number_u = 0;
        memcpy(&number_u, data, type->size);
        *number = number_u;
    } else {
        switch (type->size) {
        case 1: { int8_t x; memcpy(&x, data, 1); *number = x; break; }
        case 2: { int16_t x; memcpy(&x, data, 2); *number = x; break; }
        case 4: { int32_t x; memcpy(&x, data, 4); *number = x; break; }
        case 8: { int64_t x; memcpy(&x, data, 8); *number = x; break; }
        default: return -1;
        }
    }
    return 0;
}

static int value_store_number(script_eval_t *ctx, const char *pos,
    sireflect_handle_t handle, long double number, void *out) {
    const sireflect_type_info_t *type = sireflect_type_info(handle);
    if (!type) return script_eval_error(ctx, pos, "unknown value type");
    if (type->kind == sireflect_kind_f32) {
        float result = (float)number; memcpy(out, &result, sizeof result); return 0;
    }
    if (type->kind == sireflect_kind_f64) {
        double result = (double)number; memcpy(out, &result, sizeof result); return 0;
    }
    if (type->kind == sireflect_kind_bool) {
        bool result = number != 0; memcpy(out, &result, sizeof result); return 0;
    }
    if (!isfinite(number)) return script_eval_error(ctx, pos, "non-finite integer conversion");
    if (!sireflect_is_numeric(type->kind) && type->kind != sireflect_kind_char &&
        type->kind != sireflect_kind_enum) {
        return script_eval_error(ctx, pos, "expected numeric type '%s'", type->name);
    }
    unsigned bits = (unsigned)type->size * CHAR_BIT;
    if (!bits || bits > 64) return script_eval_error(ctx, pos, "invalid numeric size");
    if (value_is_unsigned(type->kind)) {
        if (number < 0 || number >= ldexpl(1, (int)bits))
            return script_eval_error(ctx, pos, "integer conversion out of range");
        uint64_t result = (uint64_t)number;
        switch (type->size) {
        case 1: { uint8_t x = (uint8_t)result; memcpy(out, &x, 1); break; }
        case 2: { uint16_t x = (uint16_t)result; memcpy(out, &x, 2); break; }
        case 4: { uint32_t x = (uint32_t)result; memcpy(out, &x, 4); break; }
        case 8: memcpy(out, &result, 8); break;
        }
    } else {
        long double limit = ldexpl(1, (int)bits - 1);
        if (number < -limit || number >= limit)
            return script_eval_error(ctx, pos, "integer conversion out of range");
        int64_t result = (int64_t)number;
        switch (type->size) {
        case 1: { int8_t x = (int8_t)result; memcpy(out, &x, 1); break; }
        case 2: { int16_t x = (int16_t)result; memcpy(out, &x, 2); break; }
        case 4: { int32_t x = (int32_t)result; memcpy(out, &x, 4); break; }
        case 8: memcpy(out, &result, 8); break;
        }
    }
    return 0;
}

const script_variable_t *script_find_variable(script_eval_t *ctx, const char *name) {
    script_variable_t *variables = ctx->variables.data;
    for (uint32_t i = ctx->variables.size; i > ctx->variable_base; i--) {
        if (!strcmp(variables[i - 1].name, name)) return &variables[i - 1];
    }
    for (uint32_t i = ctx->capture_count; i; i--)
        if (!strcmp(ctx->captures[i - 1].name, name)) return &ctx->captures[i - 1];
    return NULL;
}

int script_eval_expr(script_eval_t *ctx, const ecs_expr_node_t *expr, script_value_t *out) {
    if (!expr) return script_eval_error(ctx, NULL, "missing expression");
    *out = (script_value_t){0};
    switch (expr->kind) {
    case EcsExprValue: {
        const ecs_expr_value_node_t *value = (const ecs_expr_value_node_t*)expr;
        out->type = script_backend_type_lookup(literal_types[value->literal_kind]);
        out->storage = value->storage;
        return out->type ? 0 : script_eval_error(ctx, expr->pos, "literal type unavailable");
    }
    case EcsExprInterpolatedString: {
        ecs_expr_interpolated_string_t *string = (ecs_expr_interpolated_string_t*)expr;
        if (script_prepare_interpolation((ecs_script_t*)ctx->script, string)) return -1;
        ecs_strbuf_t buffer = ECS_STRBUF_INIT;
        ecs_expr_fragment_t *fragments = string->fragments.data;
        for (uint32_t i = 0; i < string->fragments.size; i++) {
            if (fragments[i].text) ecs_strbuf_appendstr(&buffer, fragments[i].text);
            if (!fragments[i].expr) continue;
            script_value_t value;
            if (script_eval_expr(ctx, fragments[i].expr, &value)) {
                free(ecs_strbuf_get(&buffer)); return -1;
            }
            char formatted[96];
            if (sireflect_type_is_cstring(value.type)) {
                ecs_strbuf_appendstr(&buffer, value.storage.string ? value.storage.string : "");
                continue;
            }
            long double number;
            if (value_number(&value, &number)) {
                free(ecs_strbuf_get(&buffer));
                return script_eval_error(ctx, expr->pos, "unsupported interpolated value");
            }
            const sireflect_type_info_t *info = sireflect_type_info(value.type);
            if (info->kind == sireflect_kind_bool) snprintf(formatted, sizeof formatted, "%s", number ? "true" : "false");
            else if (sireflect_type_is_floating(value.type)) snprintf(formatted, sizeof formatted, "%g", (double)number);
            else if (value_is_unsigned(info->kind)) snprintf(formatted, sizeof formatted, "%" PRIu64, (uint64_t)number);
            else snprintf(formatted, sizeof formatted, "%" PRId64, (int64_t)number);
            ecs_strbuf_appendstr(&buffer, formatted);
        }
        char *text = ecs_strbuf_get(&buffer);
        out->storage.string = siecs_strdup(&ctx->values, text);
        out->type = script_backend_type_lookup("string");
        free(text);
        return 0;
    }
    case EcsExprIdentifier:
    case EcsExprVariable: {
        const char *name = expr->kind == EcsExprVariable ?
            ((const ecs_expr_variable_t*)expr)->name : ((const ecs_expr_identifier_t*)expr)->value;
        const script_variable_t *variable = script_find_variable(ctx, name);
        if (variable) { *out = variable->value; return 0; }
        ecs_entity_t entity = script_backend_entity_lookup(name, ctx->parent);
        if (entity) {
            out->type = script_backend_type_lookup("entity");
            out->storage.entity = entity;
            return 0;
        }
        return script_eval_error(ctx, expr->pos, "unresolved identifier '%s'", name);
    }
    case EcsExprElement: {
        const ecs_expr_element_t *element = (const ecs_expr_element_t*)expr;
        if (!element->index || element->index->kind != EcsExprIdentifier)
            return script_eval_error(ctx, expr->pos, "dynamic component references not implemented");
        const char *name = ((const ecs_expr_identifier_t*)element->index)->value;
        ecs_component_t component = script_backend_component_lookup(name);
        if (!component) return script_eval_error(ctx, expr->pos, "unresolved component '%s'", name);
        script_value_t entity;
        if (script_eval_expr(ctx, element->left, &entity)) return -1;
        if (entity.type != script_backend_type_lookup("entity"))
            return script_eval_error(ctx, expr->pos, "expected entity reference");
        ecs_entity_t handle;
        memcpy(&handle, script_value_data(&entity), sizeof handle);
        const void *source = script_backend_component_get(handle, component);
        if (!source) return script_eval_error(ctx, expr->pos, "entity has no component '%s'", name);
        const ecs_component_info_t *info = script_backend_component_info(component);
        if (!info->type) return script_eval_error(ctx, expr->pos, "component '%s' has no reflection", name);
        out->type = info->type;
        void *copy = script_alloc(&ctx->values, (size_t)info->size);
        script_backend_value_init(component, copy, source);
        script_owned_value_t owned = {.component = component, .ptr = copy};
        sicore_vec_push(&ctx->owned_values, &owned, sizeof owned);
        out->ptr = copy;
        return 0;
    }
    case EcsExprMember: {
        const ecs_expr_member_t *member = (const ecs_expr_member_t*)expr;
        script_value_t left;
        if (script_eval_expr(ctx, member->left, &left)) return -1;
        const sireflect_fields_t *fields = sireflect_type_fields(left.type);
        for (size_t i = 0; i < fields->field_count; i++) {
            const sireflect_field_info_t *field = &fields->fields[i];
            if (strcmp(field->name, member->member_name)) continue;
            out->type = field->type;
            if (field->size <= sizeof out->storage) {
                memcpy(&out->storage, (const char*)script_value_data(&left) + field->offset, field->size);
            } else out->ptr = (const char*)script_value_data(&left) + field->offset;
            return 0;
        }
        return script_eval_error(ctx, expr->pos, "unknown member '%s'", member->member_name);
    }
    case EcsExprUnary: {
        const ecs_expr_unary_t *unary = (const ecs_expr_unary_t*)expr;
        script_value_t input;
        long double number;
        if (script_eval_expr(ctx, unary->expr, &input)) return -1;
        if (value_number(&input, &number)) return script_eval_error(ctx, expr->pos, "expected numeric operand");
        if (unary->operator == EcsTokNot) {
            out->type = script_backend_type_lookup("bool"); out->storage.bool_ = !number; return 0;
        }
        out->type = input.type;
        return value_store_number(ctx, expr->pos, out->type, -number, &out->storage);
    }
    case EcsExprBinary: {
        const ecs_expr_binary_t *binary = (const ecs_expr_binary_t*)expr;
        script_value_t left, right;
        long double a, b;
        if (script_eval_expr(ctx, binary->left, &left)) return -1;
        bool left_string = sireflect_type_is_cstring(left.type);
        if (!left_string && value_number(&left, &a))
            return script_eval_error(ctx, expr->pos, "expected numeric left operand");
        if (!left_string && ((binary->operator == EcsTokAnd && !a) ||
            (binary->operator == EcsTokOr && a))) {
            out->type = script_backend_type_lookup("bool"); out->storage.bool_ = a != 0; return 0;
        }
        if (script_eval_expr(ctx, binary->right, &right)) return -1;
        if (left_string) {
            if (!sireflect_type_is_cstring(right.type) ||
                (binary->operator != EcsTokEq && binary->operator != EcsTokNeq))
                return script_eval_error(ctx, expr->pos, "invalid string operator");
            char *l = left.storage.string, *r = right.storage.string;
            out->type = script_backend_type_lookup("bool");
            out->storage.bool_ = l && r ? (binary->operator == EcsTokEq ? !strcmp(l, r) : strcmp(l, r) != 0) : l == r;
            return 0;
        }
        if (value_number(&right, &b))
            return script_eval_error(ctx, expr->pos, "expected numeric right operand");
        const sireflect_type_info_t *lt = sireflect_type_info(left.type);
        const sireflect_type_info_t *rt = sireflect_type_info(right.type);
        bool real = sireflect_type_is_floating(left.type) || sireflect_type_is_floating(right.type);
        bool is_unsigned = value_is_unsigned(lt->kind) && value_is_unsigned(rt->kind) && binary->operator != EcsTokSub;
        bool logical = binary->operator == EcsTokAnd || binary->operator == EcsTokOr;
        bool comparison = logical || binary->operator == EcsTokEq || binary->operator == EcsTokNeq ||
            binary->operator == EcsTokLt || binary->operator == EcsTokLtEq ||
            binary->operator == EcsTokGt || binary->operator == EcsTokGtEq;
        if ((binary->operator == EcsTokDiv || binary->operator == EcsTokMod) && !b)
            return script_eval_error(ctx, expr->pos, "division by zero");
        if (logical) {
            out->type = script_backend_type_lookup("bool");
            out->storage.bool_ = binary->operator == EcsTokAnd ? a && b : a || b;
            return 0;
        }
        sireflect_handle_t operand_type = script_backend_type_lookup(real ? "double" : is_unsigned ? "uint64_t" : "int64_t");
        out->type = comparison ? script_backend_type_lookup("bool") : operand_type;
        script_value_t l = {.type = operand_type}, r = {.type = operand_type};
        if (value_store_number(ctx, expr->pos, operand_type, a, &l.storage) ||
            value_store_number(ctx, expr->pos, operand_type, b, &r.storage)) return -1;
        int status;
        if (real) status = script_operator_double(&l.storage.f64, &r.storage.f64, &out->storage, binary->operator);
        else {
            bool shift = binary->operator == EcsTokShiftLeft || binary->operator == EcsTokShiftRight;
            if (shift && (b < 0 || b >= 64))
                return script_eval_error(ctx, expr->pos, "invalid shift count");
            if (!is_unsigned) {
                int64_t x = l.storage.i64, y = r.storage.i64;
                if ((binary->operator == EcsTokDiv || binary->operator == EcsTokMod) && x == INT64_MIN && y == -1)
                    return script_eval_error(ctx, expr->pos, "integer overflow");
                if (shift && x < 0)
                    return script_eval_error(ctx, expr->pos, "negative signed shift operand");
                long double bound = 0;
                bool arithmetic = true;
                switch (binary->operator) {
                case EcsTokAdd: bound = a + b; break;
                case EcsTokSub: bound = a - b; break;
                case EcsTokMul: bound = a * b; break;
                case EcsTokShiftLeft: bound = ldexpl(a, (int)b); break;
                default: arithmetic = false; break;
                }
                if (arithmetic && (bound < -ldexpl(1, 63) || bound >= ldexpl(1, 63)))
                    return script_eval_error(ctx, expr->pos, "integer overflow");
                status = script_operator_int64_t(&l.storage.i64, &r.storage.i64, &out->storage, binary->operator);
            } else status = script_operator_uint64_t(&l.storage.u64, &r.storage.u64, &out->storage, binary->operator);
        }
        if (status) return script_eval_error(ctx, expr->pos, "invalid binary operator for operand type");
        return 0;
    }
    default: return script_eval_error(ctx, expr->pos, "expression kind %d not implemented", expr->kind);
    }
}

int script_value_write(script_eval_t *ctx, const ecs_expr_node_t *expr,
    sireflect_handle_t type, void *destination) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info) return script_eval_error(ctx, expr->pos, "unknown reflected type");
    if (expr->kind == EcsExprInitializer || expr->kind == EcsExprEmptyInitializer) {
        const ecs_expr_initializer_t *init = (const ecs_expr_initializer_t*)expr;
        const ecs_expr_initializer_element_t *elements = init->elements.data;
        const sireflect_fields_t *fields = sireflect_type_fields(type);
        if (!sireflect_type_is_struct(info) || type == script_backend_type_lookup("entity")) {
            if (init->elements.size == 1 && !elements[0].member)
                return script_value_write(ctx, elements[0].value, type, destination);
            return script_eval_error(ctx, expr->pos, "expected scalar initializer for '%s'", info->name);
        }
        for (uint32_t i = 0; i < init->elements.size; i++) {
            const sireflect_field_info_t *field = NULL;
            if (elements[i].member) {
                for (size_t j = 0; j < fields->field_count; j++) {
                    if (!strcmp(fields->fields[j].name, elements[i].member)) { field = &fields->fields[j]; break; }
                }
            } else if (i < fields->field_count) field = &fields->fields[i];
            if (!field) {
                if (ctx->skip_unknown && elements[i].member) continue;
                return script_eval_error(ctx, elements[i].value->pos, "unknown member '%s' in '%s'",
                    elements[i].member ? elements[i].member : "<positional>", info->name);
            }
            if (script_value_write(ctx, elements[i].value, field->type,
                (char*)destination + field->offset)) return -1;
        }
        return 0;
    }
    script_value_t value;
    if (script_eval_expr(ctx, expr, &value)) return -1;
    return script_value_convert(ctx, expr->pos, &value, type, destination);
}

int script_value_convert(script_eval_t *ctx, const char *position,
    const script_value_t *value, sireflect_handle_t type, void *destination) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (type == script_backend_type_lookup("entity") && sireflect_type_is_cstring(value->type)) {
        const char *name = *(const char *const*)script_value_data(value);
        ecs_entity_t entity = script_backend_entity_lookup(name, ctx->parent);
        if (!entity) return script_eval_error(ctx, position, "unresolved entity '%s'", name);
        memcpy(destination, &entity, sizeof entity);
        return 0;
    }
    if (sireflect_type_is_cstring(type))
        return script_eval_error(ctx, position, "string component lifecycle not implemented");
    if (value->type == type) {
        memcpy(destination, script_value_data(value), info->size);
        return 0;
    }
    long double number;
    if (value_number(value, &number)) return script_eval_error(ctx, position, "incompatible value for '%s'", info->name);
    return value_store_number(ctx, position, type, number, destination);
}
