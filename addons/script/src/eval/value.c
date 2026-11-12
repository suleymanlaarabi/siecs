#include "../backend/transaction.h"
#include "async.h"
#include "eval.h"
#include <inttypes.h>
#include <math.h>

int script_eval_error(script_eval_t *ctx, const char *pos, const char *format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof message, format, args);
    va_end(args);
    if (!pos)
        pos = ctx->position;
    uintptr_t source = (uintptr_t)ctx->script->code;
    uintptr_t location = (uintptr_t)pos;
    if (location < source || location > source + strlen(ctx->script->code))
        pos = ctx->position;
    location = (uintptr_t)pos;
    int64_t offset = location >= source && location <= source + strlen(ctx->script->code)
                         ? (int64_t)(location - source)
                         : 0;
    ecs_parser_error(ctx->script->name, ctx->script->code, offset, "%s", message);
    return -1;
}

void script_custom_value_fini(const ecs_type_ops_t *ops, void *value) {
    if (ops->dtor)
        ops->dtor(value, 1);
    else if (ops->reflected_type)
        ecs_reflected_value_fini(ops->reflected_type, value);
}

void *script_value_storage(script_eval_t *ctx, sireflect_handle_t type, script_value_t *value) {
    value->type = type;
    size_t size = sireflect_type_size(type);
    ecs_type_ops_t ops = ecs_reflected_ops(type);
    const sireflect_type_info_t *info = sireflect_type_info(type);
    ecs_component_t component = script_backend_component_lookup(info->name);
    if (component && script_backend_component_info(component)->type == type) {
        /* Reflected component hooks remain authoritative for owning C values. */
        ops = *script_backend_value_ops(component);
    }
    bool owning = ops.ctor || ops.dtor || ops.copy_ctor || ops.copy || ops.move_ctor || ops.move ||
                  ops.reflected_type;
    if (!owning && size <= sizeof value->storage)
        return &value->storage;
    void *storage = script_alloc(&ctx->values, size);
    value->ptr = storage;
    if (owning) {
        ecs_type_ops_t *saved_ops = script_alloc(&ctx->values, sizeof(ops));
        *saved_ops = ops;
        value->ops = saved_ops;
        if (ops.ctor)
            ops.ctor(storage, 1);
        script_owned_value_t owned = { .ptr = storage, .ops = ops, .custom = true };
        sicore_vec_push(&ctx->owned_values, &owned, sizeof owned);
    }
    return storage;
}

static int
component_identifier(script_eval_t *ctx, const ecs_expr_node_t *expression, const char **name) {
    if (!expression)
        return script_eval_error(ctx, NULL, "expected component identifier");
    if (expression->kind == EcsExprIdentifier) {
        *name = ((const ecs_expr_identifier_t *)expression)->value;
        return 0;
    }
    script_value_t value = { 0 };
    if (script_eval_expr(ctx, expression, &value))
        return -1;
    if (expression->kind == EcsExprVariable && value.type == script_backend_type_lookup("entity")) {
        const char *variable = ((const ecs_expr_variable_t *)expression)->name;
        char *reference = script_alloc(&ctx->values, strlen(variable) + 2);
        sprintf(reference, "$%s", variable);
        *name = reference;
        return 0;
    }
    if (!sireflect_type_is_cstring(value.type))
        return script_eval_error(ctx, expression->pos, "expected component name string");
    *name = *(const char *const *)script_value_data(&value);
    return 0;
}

static script_key_t
component_key(script_eval_t *ctx, const ecs_expr_node_t *first, const ecs_expr_node_t *second) {
    const char *name = NULL, *target = NULL;
    if (component_identifier(ctx, first, &name))
        return 0;
    if (!second)
        return script_resolve_component(ctx, name);
    if (component_identifier(ctx, second, &target))
        return 0;
    return script_resolve_pair(ctx, name, target);
}

static const char *literal_types[] = { "bool", "char", "int64_t", "uint64_t", "double", "char *" };
static bool integer_value(const script_value_t *value, bool *is_unsigned, uint64_t *bits);

static bool value_is_unsigned(sireflect_kind_t kind) {
    switch (kind) {
    case sireflect_kind_u8:
    case sireflect_kind_u16:
    case sireflect_kind_u32:
    case sireflect_kind_u64:
    case sireflect_kind_unsigned_char:
    case sireflect_kind_unsigned_short:
    case sireflect_kind_unsigned_int:
    case sireflect_kind_unsigned_long:
    case sireflect_kind_unsigned_long_long:
        return true;
    default:
        return false;
    }
}

static int value_number(const script_value_t *value, long double *number) {
    if (value->collection || !value->type)
        return -1;
    const sireflect_type_info_t *type = sireflect_type_info(value->type);
    if (!type)
        return -1;
    const void *data = script_value_data(value);
    switch (type->kind) {
    case sireflect_kind_f64:
        *number = *(const double *)data;
        return 0;
    case sireflect_kind_f32: {
        float number_f;
        memcpy(&number_f, data, sizeof number_f);
        *number = number_f;
        return 0;
    }
    case sireflect_kind_bool:
        *number = *(const bool *)data;
        return 0;
    case sireflect_kind_char:
        *number = *(const char *)data;
        return 0;
    default:
        break;
    }
    if (!sireflect_is_numeric(type->kind) && type->kind != sireflect_kind_enum)
        return -1;
    if (value_is_unsigned(type->kind)) {
        uint64_t number_u = 0;
        memcpy(&number_u, data, type->size);
        *number = number_u;
    } else {
        switch (type->size) {
        case 1: {
            int8_t x;
            memcpy(&x, data, 1);
            *number = x;
            break;
        }
        case 2: {
            int16_t x;
            memcpy(&x, data, 2);
            *number = x;
            break;
        }
        case 4: {
            int32_t x;
            memcpy(&x, data, 4);
            *number = x;
            break;
        }
        case 8: {
            int64_t x;
            memcpy(&x, data, 8);
            *number = x;
            break;
        }
        default:
            return -1;
        }
    }
    return 0;
}

static void append_formatted(ecs_strbuf_t *buffer, const char *format, ...) {
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length >= 0) {
        char local[128];
        char *text = (size_t)length < sizeof local ? local : malloc((size_t)length + 1);
        assert(text != NULL);
        vsnprintf(text, (size_t)length + 1, format, args);
        ecs_strbuf_appendstrn(buffer, text, length);
        if (text != local)
            free(text);
    }
    va_end(args);
}

static int interpolate_value(
    script_eval_t *ctx,
    const char *position,
    const script_value_t *value,
    const char *specification,
    ecs_strbuf_t *buffer
) {
    const sireflect_type_info_t *info = value->type ? sireflect_type_info(value->type) : NULL;
    if (!info || value->collection || value->future)
        return script_eval_error(ctx, position, "unsupported interpolated value");
    bool string = sireflect_type_is_cstring(value->type);
    bool boolean = info->kind == sireflect_kind_bool;
    bool floating = sireflect_type_is_floating(value->type);
    bool is_unsigned = false;
    uint64_t bits = 0;
    bool integer = integer_value(value, &is_unsigned, &bits);
    size_t length = specification ? strlen(specification) : 0;
    char kind = length              ? specification[length - 1]
                : string || boolean ? 's'
                : floating          ? 'g'
                : is_unsigned       ? 'u'
                                    : 'd';
    char format[32] = "%";
    if (length)
        memcpy(format + 1, specification, length - 1);
    size_t prefix = length ? length : 1;
    format[prefix] = kind;
    format[prefix + 1] = '\0';
    if (kind == 's' && (string || boolean)) {
        const char *text = string ? *(const char *const *)script_value_data(value)
                           : bits ? "true"
                                  : "false";
        append_formatted(buffer, format, text ? text : "");
        return 0;
    }
    if (strchr("fFeEgG", kind)) {
        long double number;
        if (!value_number(value, &number)) {
            append_formatted(buffer, format, (double)number);
            return 0;
        }
    } else if (integer && strchr("duxXo", kind)) {
        if (kind == 'd') {
            if (is_unsigned && bits > INT64_MAX)
                return script_eval_error(ctx, position, "signed interpolation value out of range");
            strcpy(format + prefix, PRId64);
            append_formatted(buffer, format, (int64_t)bits);
            return 0;
        }
        if (!is_unsigned && (int64_t)bits < 0)
            return script_eval_error(ctx, position, "unsigned interpolation value out of range");
        const char *conversion = kind == 'u'   ? PRIu64
                                 : kind == 'x' ? PRIx64
                                 : kind == 'X' ? PRIX64
                                               : PRIo64;
        strcpy(format + prefix, conversion);
        append_formatted(buffer, format, bits);
        return 0;
    }
    return script_eval_error(ctx, position, "interpolation format is incompatible with value type");
}

static int value_store_number(
    script_eval_t *ctx,
    const char *pos,
    sireflect_handle_t handle,
    long double number,
    void *out
) {
    const sireflect_type_info_t *type = sireflect_type_info(handle);
    if (!type)
        return script_eval_error(ctx, pos, "unknown value type");
    if (type->kind == sireflect_kind_f32) {
        float result = (float)number;
        memcpy(out, &result, sizeof result);
        return 0;
    }
    if (type->kind == sireflect_kind_f64) {
        double result = (double)number;
        memcpy(out, &result, sizeof result);
        return 0;
    }
    if (type->kind == sireflect_kind_bool) {
        bool result = number != 0;
        memcpy(out, &result, sizeof result);
        return 0;
    }
    if (!isfinite(number))
        return script_eval_error(ctx, pos, "non-finite integer conversion");
    if (!sireflect_is_numeric(type->kind) && type->kind != sireflect_kind_char &&
        type->kind != sireflect_kind_enum) {
        return script_eval_error(ctx, pos, "expected numeric type '%s'", type->name);
    }
    unsigned bits = (unsigned)type->size * CHAR_BIT;
    if (!bits || bits > 64)
        return script_eval_error(ctx, pos, "invalid numeric size");
    if (value_is_unsigned(type->kind)) {
        if (number < 0 || number >= ldexpl(1, (int)bits))
            return script_eval_error(ctx, pos, "integer conversion out of range");
        uint64_t result = (uint64_t)number;
        switch (type->size) {
        case 1: {
            uint8_t x = (uint8_t)result;
            memcpy(out, &x, 1);
            break;
        }
        case 2: {
            uint16_t x = (uint16_t)result;
            memcpy(out, &x, 2);
            break;
        }
        case 4: {
            uint32_t x = (uint32_t)result;
            memcpy(out, &x, 4);
            break;
        }
        case 8:
            memcpy(out, &result, 8);
            break;
        }
    } else {
        long double limit = ldexpl(1, (int)bits - 1);
        if (number < -limit || number >= limit)
            return script_eval_error(ctx, pos, "integer conversion out of range");
        int64_t result = (int64_t)number;
        switch (type->size) {
        case 1: {
            int8_t x = (int8_t)result;
            memcpy(out, &x, 1);
            break;
        }
        case 2: {
            int16_t x = (int16_t)result;
            memcpy(out, &x, 2);
            break;
        }
        case 4: {
            int32_t x = (int32_t)result;
            memcpy(out, &x, 4);
            break;
        }
        case 8:
            memcpy(out, &result, 8);
            break;
        }
    }
    return 0;
}

const script_variable_t *script_find_variable(script_eval_t *ctx, const char *name) {
    script_variable_t *variables = ctx->variables.data;
    for (uint32_t i = ctx->variables.size; i > ctx->variable_base; i--) {
        if (!strcmp(variables[i - 1].name, name))
            return &variables[i - 1];
    }
    for (uint32_t i = ctx->capture_count; i; i--)
        if (!strcmp(ctx->captures[i - 1].name, name))
            return &ctx->captures[i - 1];
    return NULL;
}

static int eval_expression(script_eval_t *ctx, const ecs_expr_node_t *expr, script_value_t *out) {
    if (!expr)
        return script_eval_error(ctx, NULL, "missing expression");
    *out = (script_value_t){ 0 };
    switch (expr->kind) {
    case EcsExprValue: {
        const ecs_expr_value_node_t *value = (const ecs_expr_value_node_t *)expr;
        out->type = script_backend_type_lookup(literal_types[value->literal_kind]);
        out->storage = value->storage;
        return out->type ? 0 : script_eval_error(ctx, expr->pos, "literal type unavailable");
    }
    case EcsExprInterpolatedString: {
        ecs_expr_interpolated_string_t *string = (ecs_expr_interpolated_string_t *)expr;
        if (script_prepare_interpolation((ecs_script_t *)ctx->script, string))
            return -1;
        ecs_strbuf_t buffer = ECS_STRBUF_INIT;
        ecs_expr_fragment_t *fragments = string->fragments.data;
        for (uint32_t i = 0; i < string->fragments.size; i++) {
            if (fragments[i].text)
                ecs_strbuf_appendstr(&buffer, fragments[i].text);
            if (!fragments[i].expr)
                continue;
            script_value_t value;
            if (script_eval_expr(ctx, fragments[i].expr, &value)) {
                free(ecs_strbuf_get(&buffer));
                return -1;
            }
            if (interpolate_value(ctx, expr->pos, &value, fragments[i].format, &buffer)) {
                free(ecs_strbuf_get(&buffer));
                return -1;
            }
        }
        char *text = ecs_strbuf_get(&buffer);
        out->storage.string = siecs_strdup(&ctx->values, text);
        out->type = script_backend_type_lookup("string");
        free(text);
        return 0;
    }
    case EcsExprIdentifier:
    case EcsExprVariable: {
        const char *name = expr->kind == EcsExprVariable
                               ? ((const ecs_expr_variable_t *)expr)->name
                               : ((const ecs_expr_identifier_t *)expr)->value;
        const script_variable_t *variable = script_find_variable(ctx, name);
        if (variable) {
            *out = variable->value;
            return 0;
        }
        ecs_entity_t entity = script_resolve_entity(ctx, name);
        if (entity) {
            out->type = script_backend_type_lookup("entity");
            out->storage.entity = entity;
            return 0;
        }
        return script_eval_error(ctx, expr->pos, "unresolved identifier '%s'", name);
    }
    case EcsExprFunction:
        return script_call(ctx, (const ecs_expr_function_t *)expr, out);
    case EcsExprScript:
        out->type = script_backend_type_lookup("string");
        out->storage.string = (char *)((const ecs_expr_script_t *)expr)->script->code;
        return 0;
    case EcsExprInitializer: {
        const ecs_expr_initializer_t *initializer = (const ecs_expr_initializer_t *)expr;
        out->collection = true;
        out->count = initializer->elements.size;
        out->items = script_alloc(&ctx->values, out->count * sizeof(script_value_t));
        const ecs_expr_initializer_element_t *elements = initializer->elements.data;
        for (uint32_t i = 0; i < out->count; i++) {
            if (script_eval_expr(ctx, elements[i].value, &out->items[i]))
                return -1;
        }
        return 0;
    }
    case EcsExprHas: {
        const ecs_expr_has_t *has = (const ecs_expr_has_t *)expr;
        script_value_t entity;
        if (script_eval_expr(ctx, has->left, &entity))
            return -1;
        if (entity.type != script_backend_type_lookup("entity"))
            return script_eval_error(ctx, expr->pos, "invalid presence test");
        script_key_t component = component_key(ctx, has->first, has->second);
        if (!component && has->second)
            return -1;
        ecs_entity_t handle;
        memcpy(&handle, script_value_data(&entity), sizeof handle);
        out->type = script_backend_type_lookup("bool");
        out->storage.bool_ = component && script_backend_component_has(handle, component);
        return 0;
    }
    case EcsExprElement: {
        const ecs_expr_element_t *element = (const ecs_expr_element_t *)expr;
        script_value_t container;
        if (script_eval_expr(ctx, element->left, &container))
            return -1;
        if (element->second && container.type != script_backend_type_lookup("entity"))
            return script_eval_error(ctx, expr->pos, "pair access requires an entity");
        if (container.collection) {
            script_value_t index;
            long double number;
            if (script_eval_expr(ctx, element->index, &index) || value_number(&index, &number))
                return script_eval_error(ctx, expr->pos, "expected collection index");
            if (number < 0 || number >= container.count || number != floorl(number))
                return script_eval_error(ctx, expr->pos, "collection index out of range");
            *out = script_collection_at(&container, (uint32_t)number);
            return 0;
        }
        const sireflect_type_info_t *container_type =
            container.type ? sireflect_type_info(container.type) : NULL;
        if (container_type && container_type->kind == sireflect_kind_array) {
            script_value_t index;
            long double number;
            if (script_eval_expr(ctx, element->index, &index) || value_number(&index, &number))
                return script_eval_error(ctx, expr->pos, "expected array index");
            if (number < 0 || number >= container_type->element_count || number != floorl(number))
                return script_eval_error(ctx, expr->pos, "array index out of range");
            out->type = container_type->element_type;
            size_t size = sireflect_type_size(out->type);
            size_t offset = (size_t)number * size;
            const void *element_data = (const char *)script_value_data(&container) + offset;
            if (size <= sizeof out->storage)
                memcpy(&out->storage, element_data, size);
            else
                out->ptr = element_data;
            out->source_entity = container.source_entity;
            out->source_component = container.source_component;
            out->source_offset = container.source_offset + offset;
            return 0;
        }
        script_key_t component = component_key(ctx, element->index, element->second);
        if (!component)
            return script_eval_error(ctx, expr->pos, "unresolved component or pair");
        script_value_t entity = container;
        if (entity.type != script_backend_type_lookup("entity"))
            return script_eval_error(ctx, expr->pos, "expected entity reference");
        ecs_entity_t handle;
        memcpy(&handle, script_value_data(&entity), sizeof handle);
        const void *source = script_transaction_get(script_transaction_active, handle, component);
        if (!source && script_transaction_active)
            script_transaction_read(script_transaction_active, handle, component);
        if (!source)
            return script_eval_error(ctx, expr->pos, "entity has no requested component or pair");
        const ecs_component_info_t *info = script_backend_component_info(component);
        if (!info->type)
            return script_eval_error(
                ctx,
                expr->pos,
                "requested component or pair has no reflection"
            );
        out->type = info->type;
        out->source_entity = handle;
        out->source_component = component;
        void *copy = script_alloc(&ctx->values, (size_t)info->size);
        script_backend_value_init(component, copy, source);
        script_owned_value_t owned = { .component = component, .ptr = copy };
        sicore_vec_push(&ctx->owned_values, &owned, sizeof owned);
        out->ptr = copy;
        return 0;
    }
    case EcsExprMember: {
        const ecs_expr_member_t *member = (const ecs_expr_member_t *)expr;
        script_value_t left;
        bool deferred = ctx->defer_read;
        ctx->defer_read = true;
        int status = script_eval_expr(ctx, member->left, &left);
        ctx->defer_read = deferred;
        if (status)
            return -1;
        if (left.collection) {
            if (!strcmp(member->member_name, "length") || !strcmp(member->member_name, "count")) {
                *out = (script_value_t){ .type = script_backend_type_lookup("uint64_t"),
                                         .storage.u64 = left.count };
                return 0;
            }
            size_t length = strlen(member->member_name);
            if (!length || length > 16)
                return script_eval_error(ctx, expr->pos, "invalid swizzle");
            out->collection = length != 1;
            out->count = (uint32_t)length;
            out->items = script_alloc(&ctx->values, length * sizeof(script_value_t));
            for (uint32_t i = 0; i < length; i++) {
                const char *alphabet = "xyzw";
                const char *lane = strchr(alphabet, member->member_name[i]);
                if (!lane) {
                    alphabet = "rgba";
                    lane = strchr(alphabet, member->member_name[i]);
                }
                if (!lane)
                    return script_eval_error(ctx, expr->pos, "invalid swizzle lane");
                uint32_t index = (uint32_t)(lane - alphabet);
                if (index >= left.count)
                    return script_eval_error(ctx, expr->pos, "swizzle index out of range");
                out->items[i] = script_collection_at(&left, index);
            }
            if (length == 1)
                *out = out->items[0];
            return 0;
        }
        if (left.type == script_backend_type_lookup("entity")) {
            ecs_entity_t scope;
            memcpy(&scope, script_value_data(&left), sizeof(scope));
            ecs_entity_t child;
            if (script_transaction_active) {
                script_transaction_lookup(script_transaction_active, member->member_name, scope);
                child = script_transaction_entity(
                    script_transaction_active,
                    member->member_name,
                    scope,
                    false
                );
            } else
                child = ecs_lookup_from(scope, member->member_name);
            if (!child)
                return script_eval_error(
                    ctx,
                    expr->pos,
                    "unresolved entity member '%s'",
                    member->member_name
                );
            *out = (script_value_t){ .type = left.type, .storage.entity = child };
            return 0;
        }
        const sireflect_fields_t *fields = sireflect_type_fields(left.type);
        for (size_t i = 0; i < fields->field_count; i++) {
            const sireflect_field_info_t *field = &fields->fields[i];
            if (strcmp(field->name, member->member_name))
                continue;
            out->type = field->type;
            out->source_entity = left.source_entity;
            out->source_component = left.source_component;
            out->source_offset = left.source_offset + field->offset;
            if (field->size <= sizeof out->storage) {
                memcpy(
                    &out->storage,
                    (const char *)script_value_data(&left) + field->offset,
                    field->size
                );
            } else
                out->ptr = (const char *)script_value_data(&left) + field->offset;
            return 0;
        }
        return script_eval_error(ctx, expr->pos, "unknown member '%s'", member->member_name);
    }
    case EcsExprUnary: {
        const ecs_expr_unary_t *unary = (const ecs_expr_unary_t *)expr;
        script_value_t input;
        long double number;
        if (script_eval_expr(ctx, unary->expr, &input))
            return -1;
        if (unary->operator == EcsTokAdd) {
            if (!sireflect_type_is_numeric_handle(input.type))
                return script_eval_error(ctx, expr->pos, "expected numeric operand");
            *out = input;
            return 0;
        }
        bool unsigned_value;
        uint64_t bits;
        if (unary->operator == EcsTokSub && integer_value(&input, &unsigned_value, &bits)) {
            if ((unsigned_value && bits > (UINT64_C(1) << 63)) ||
                (!unsigned_value && bits == (UINT64_C(1) << 63)))
                return script_eval_error(ctx, expr->pos, "integer overflow");
            out->type = script_backend_type_lookup("int64_t");
            out->storage.i64 = bits == (UINT64_C(1) << 63) ? INT64_MIN : -(int64_t)bits;
            return 0;
        }
        if (value_number(&input, &number))
            return script_eval_error(ctx, expr->pos, "expected numeric operand");
        if (unary->operator == EcsTokNot) {
            out->type = script_backend_type_lookup("bool");
            out->storage.bool_ = !number;
            return 0;
        }
        out->type = input.type;
        return value_store_number(
            ctx,
            expr->pos,
            out->type,
            unary->operator == EcsTokAdd ? number : -number,
            &out->storage
        );
    }
    case EcsExprBinary: {
        const ecs_expr_binary_t *binary = (const ecs_expr_binary_t *)expr;
        script_value_t left, right;
        if (script_eval_expr(ctx, binary->left, &left))
            return -1;
        if (binary->operator == EcsTokAnd || binary->operator == EcsTokOr) {
            long double number;
            if (value_number(&left, &number))
                return script_eval_error(ctx, expr->pos, "expected logical operand");
            if ((binary->operator == EcsTokAnd && !number) ||
                (binary->operator == EcsTokOr && number)) {
                out->type = script_backend_type_lookup("bool");
                out->storage.bool_ = number != 0;
                return 0;
            }
        }
        if (script_eval_expr(ctx, binary->right, &right))
            return -1;
        return script_binary_values(ctx, expr->pos, binary->operator, &left, &right, out);
    }
    case EcsExprMatch: {
        const ecs_expr_match_t *match = (const ecs_expr_match_t *)expr;
        script_value_t input;
        if (script_eval_expr(ctx, match->expr, &input))
            return -1;
        const ecs_expr_match_element_t *arms = match->elements.data;
        for (uint32_t i = 0; i < match->elements.size; i++) {
            script_value_t compared, equal;
            if (script_eval_expr(ctx, arms[i].compare, &compared) ||
                script_binary_values(ctx, expr->pos, EcsTokEq, &input, &compared, &equal))
                return -1;
            if (equal.storage.bool_)
                return script_eval_expr(ctx, arms[i].expr, out);
        }
        if (match->any.expr)
            return script_eval_expr(ctx, match->any.expr, out);
        return script_eval_error(ctx, expr->pos, "match has no matching arm");
    }
    case EcsExprRange: {
        const ecs_expr_range_t *range = (const ecs_expr_range_t *)expr;
        int64_t from = 0, to = 0;
        sireflect_handle_t integer = script_backend_type_lookup("int64_t");
        if (script_value_write(ctx, range->from, integer, &from) ||
            script_value_write(ctx, range->to, integer, &to))
            return -1;
        uint64_t count = to > from ? (uint64_t)to - (uint64_t)from : 0;
        if (count > UINT32_MAX)
            return script_eval_error(ctx, expr->pos, "range exceeds collection limit");
        *out = (script_value_t){ .collection = true,
                                 .range = true,
                                 .range_start = from,
                                 .count = (uint32_t)count };
        return 0;
    }
    case EcsExprNew: {
        const ecs_script_entity_t *entity = ((const ecs_expr_new_t *)expr)->entity;
        ecs_entity_t previous_entity = ctx->entity, previous_parent = ctx->parent;
        int status = script_entity_scope_begin(ctx, entity);
        ecs_entity_t created = ctx->entity;
        if (!status)
            status = script_eval_scope(ctx, entity->scope);
        if (!status && entity->kind && !strcmp(entity->kind, "prefab"))
            script_backend_prefab(created);
        ctx->entity = previous_entity;
        ctx->parent = previous_parent;
        out->type = script_backend_type_lookup("entity");
        out->storage.entity = created;
        return status;
    }

    default:
        return script_eval_error(ctx, expr->pos, "expression kind %d not implemented", expr->kind);
    }
}

static bool collection_view(script_eval_t *ctx, const script_value_t *value, script_value_t *view);

int script_eval_expr(script_eval_t *ctx, const ecs_expr_node_t *expr, script_value_t *out) {
    int status = eval_expression(ctx, expr, out);
    bool vector =
        !status && out->type && sireflect_type_info(out->type)->kind == sireflect_kind_vector;
    if (!status && (!ctx->defer_read || vector) && out->source_component &&
        script_transaction_active)
        script_transaction_read_member(
            script_transaction_active,
            out->source_entity,
            out->source_component,
            out->type,
            out->source_offset
        );
    if (vector) {
        script_value_t view;
        if (!collection_view(ctx, out, &view))
            return script_eval_error(ctx, expr->pos, "vector exceeds collection limit");
        *out = view;
    }
    return status;
}

static int write_vector(
    script_eval_t *ctx,
    const char *position,
    sireflect_handle_t type,
    void *destination,
    const script_value_t *source,
    const ecs_expr_initializer_t *initializer
) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    size_t stride = sireflect_type_size(info->element_type);
    size_t count = initializer ? initializer->elements.size : source->count;
    if (count > SIZE_MAX / stride)
        return script_eval_error(ctx, position, "vector size overflow");
    sireflect_vector_t vector = { .count = count, .data = count ? calloc(count, stride) : NULL };
    if (count && !vector.data)
        return script_eval_error(ctx, position, "out of memory");
    const ecs_expr_initializer_element_t *elements =
        initializer ? initializer->elements.data : NULL;
    for (uint32_t i = 0; i < count; i++) {
        void *item = (char *)vector.data + i * stride;
        int status;
        if (initializer) {
            status = elements[i].member
                         ? script_eval_error(ctx, position, "vector elements cannot be named")
                         : script_value_write(ctx, elements[i].value, info->element_type, item);
        } else {
            script_value_t element = script_collection_at(source, i);
            status = script_value_convert(ctx, position, &element, info->element_type, item);
        }
        if (status) {
            ecs_reflected_value_fini(type, &vector);
            return status;
        }
    }
    ecs_reflected_value_fini(type, destination);
    *(sireflect_vector_t *)destination = vector;
    return 0;
}

int script_value_write(
    script_eval_t *ctx,
    const ecs_expr_node_t *expr,
    sireflect_handle_t type,
    void *destination
) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!info)
        return script_eval_error(ctx, expr->pos, "unknown reflected type");
    if (expr->kind == EcsExprInitializer) {
        const ecs_expr_initializer_t *init = (const ecs_expr_initializer_t *)expr;
        const ecs_expr_initializer_element_t *elements = init->elements.data;
        if (info->kind == sireflect_kind_vector)
            return write_vector(ctx, expr->pos, type, destination, NULL, init);
        const sireflect_fields_t *fields = sireflect_type_fields(type);
        if (info->kind == sireflect_kind_array) {
            if (init->elements.size > info->element_count)
                return script_eval_error(ctx, expr->pos, "too many array elements");
            size_t stride = sireflect_type_size(info->element_type);
            for (uint32_t i = 0; i < init->elements.size; i++)
                if (script_value_write(
                        ctx,
                        elements[i].value,
                        info->element_type,
                        (char *)destination + i * stride
                    ))
                    return -1;
            return 0;
        }
        if (!sireflect_type_is_struct(info) || type == script_backend_type_lookup("entity")) {
            if (init->elements.size == 1 && !elements[0].member)
                return script_value_write(ctx, elements[0].value, type, destination);
            return script_eval_error(
                ctx,
                expr->pos,
                "expected scalar initializer for '%s'",
                info->name
            );
        }
        for (uint32_t i = 0; i < init->elements.size; i++) {
            const sireflect_field_info_t *field = NULL;
            if (elements[i].member) {
                for (size_t j = 0; j < fields->field_count; j++) {
                    if (!strcmp(fields->fields[j].name, elements[i].member)) {
                        field = &fields->fields[j];
                        break;
                    }
                }
            } else if (i < fields->field_count)
                field = &fields->fields[i];
            if (!field) {
                if (ctx->skip_unknown && elements[i].member)
                    continue;
                return script_eval_error(
                    ctx,
                    elements[i].value->pos,
                    "unknown member '%s' in '%s'",
                    elements[i].member ? elements[i].member : "<positional>",
                    info->name
                );
            }
            if (script_value_write(
                    ctx,
                    elements[i].value,
                    field->type,
                    (char *)destination + field->offset
                ))
                return -1;
        }
        return 0;
    }
    script_value_t value;
    if (script_eval_expr(ctx, expr, &value))
        return -1;
    return script_value_convert(ctx, expr->pos, &value, type, destination);
}

int script_value_convert(
    script_eval_t *ctx,
    const char *position,
    const script_value_t *value,
    sireflect_handle_t type,
    void *destination
) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (value->collection) {
        if (info->kind == sireflect_kind_vector)
            return write_vector(ctx, position, type, destination, value, NULL);
        size_t count =
            info->kind == sireflect_kind_array ? info->element_count : info->fields.field_count;
        if ((info->kind != sireflect_kind_array && !sireflect_type_is_struct(info)) ||
            count != value->count)
            return script_eval_error(ctx, position, "collection does not match destination layout");
        for (uint32_t i = 0; i < value->count; i++) {
            script_value_t element = script_collection_at(value, i);
            sireflect_handle_t element_type = info->kind == sireflect_kind_array
                                                  ? info->element_type
                                                  : info->fields.fields[i].type;
            size_t offset = info->kind == sireflect_kind_array
                                ? i * sireflect_type_size(element_type)
                                : info->fields.fields[i].offset;
            if (script_value_convert(
                    ctx,
                    position,
                    &element,
                    element_type,
                    (char *)destination + offset
                ))
                return -1;
        }
        return 0;
    }
    if (type == script_backend_type_lookup("entity") && sireflect_type_is_cstring(value->type)) {
        const char *name = *(const char *const *)script_value_data(value);
        ecs_entity_t entity = script_resolve_entity(ctx, name);
        if (!entity)
            return script_eval_error(ctx, position, "unresolved entity '%s'", name);
        memcpy(destination, &entity, sizeof entity);
        return 0;
    }
    if (sireflect_type_is_cstring(type)) {
        if (!sireflect_type_is_cstring(value->type))
            return script_eval_error(ctx, position, "expected string value");
        ecs_reflected_value_copy(type, destination, script_value_data(value), false);
        return 0;
    }
    if (value->type == type) {
        ecs_component_t component = script_backend_component_lookup(info->name);
        if (component && script_backend_component_info(component)->type == type)
            script_backend_value_copy(component, destination, script_value_data(value));
        else if (ecs_reflected_ops(type).reflected_type)
            ecs_reflected_value_copy(type, destination, script_value_data(value), false);
        else
            memcpy(destination, script_value_data(value), info->size);
        return 0;
    }
    long double number;
    if (value_number(value, &number))
        return script_eval_error(ctx, position, "incompatible value for '%s'", info->name);
    return value_store_number(ctx, position, type, number, destination);
}

int script_call(script_eval_t *ctx, const ecs_expr_function_t *call, script_value_t *out) {
    if (!call->function_name && call->left && call->left->kind == EcsExprMember) {
        const ecs_expr_member_t *member = (const ecs_expr_member_t *)call->left;
        ecs_expr_function_t method = *call;
        ecs_expr_initializer_t arguments = *call->args;
        uint32_t count = arguments.elements.size;
        ecs_expr_initializer_element_t *items =
            script_alloc(&ctx->values, (count + 1) * sizeof(*items));
        items[0].value = member->left;
        if (count)
            memcpy(items + 1, arguments.elements.data, count * sizeof(*items));
        arguments.elements.data = items;
        arguments.elements.size = count + 1;
        method.args = &arguments;
        method.function_name = member->member_name;
        return script_call(ctx, &method, out);
    }
    if (!call->function_name)
        return script_eval_error(ctx, call->node.pos, "expected named function");
    const ecs_expr_initializer_element_t *args = call->args->elements.data;
    uint32_t count = call->args->elements.size;
    sireflect_handle_t conversion = script_resolve_type(ctx, call->function_name);
    if (conversion) {
        if (count != 1)
            return script_eval_error(ctx, call->node.pos, "conversion requires one argument");
        void *storage = script_value_storage(ctx, conversion, out);
        if (script_value_write(ctx, args[0].value, conversion, storage))
            return -1;
        return 0;
    }
    script_function_t *function = ctx->functions;
    while (function && strcmp(function->node->name, call->function_name))
        function = function->next;
    if (!function)
        return script_async_host_call(ctx, call, out);
    if (count != function->node->params.size)
        return script_eval_error(ctx, call->node.pos, "function argument count mismatch");
    if (ctx->call_depth >= 64)
        return script_eval_error(ctx, call->node.pos, "function recursion limit exceeded");
    script_variable_t *parameters = script_alloc(&ctx->values, count * sizeof(script_variable_t));
    const ecs_script_fn_param_t *declarations = function->node->params.data;
    for (uint32_t i = 0; i < count; i++) {
        parameters[i].name = declarations[i].name;
        script_eval_t lexical = *ctx;
        lexical.imports = function->imports;
        parameters[i].value.type = script_resolve_type(&lexical, declarations[i].type);
        if (!parameters[i].value.type)
            return script_eval_error(ctx, call->node.pos, "unknown parameter type");
        void *storage = script_value_storage(ctx, parameters[i].value.type, &parameters[i].value);
        if (script_value_write(ctx, args[i].value, parameters[i].value.type, storage))
            return -1;
    }
    uint32_t previous_size = ctx->variables.size, previous_base = ctx->variable_base;
    const script_variable_t *previous_captures = ctx->captures;
    uint32_t previous_capture_count = ctx->capture_count;
    script_function_t *previous_functions = ctx->functions;
    script_import_t *previous_imports = ctx->imports;
    int32_t previous_loop_depth = ctx->loop_depth;
    ctx->variable_base = previous_size;
    ctx->captures = function->captures;
    ctx->capture_count = function->capture_count;
    ctx->functions = function;
    ctx->imports = function->imports;
    ctx->loop_depth = 0;
    ctx->call_depth++;
    for (uint32_t i = 0; i < count; i++)
        sicore_vec_push(&ctx->variables, &parameters[i], sizeof(*parameters));
    const ecs_script_t *previous_program = ctx->script;
    ctx->script = function->program;
    int status = 0;
    const ecs_script_node_t *const *statements = function->node->body->stmts.data;
    for (uint32_t i = 0; i < function->node->body->stmts.size && !status; i++)
        status = script_eval_node(ctx, statements[i]);
    if (!status) {
        out->type = script_resolve_type(ctx, function->node->return_type);
        if (!out->type || !function->node->return_expr)
            status = script_eval_error(ctx, call->node.pos, "function requires a typed result");
        else {
            void *storage = script_value_storage(ctx, out->type, out);
            status = script_value_write(ctx, function->node->return_expr, out->type, storage);
        }
    }
    ctx->variables.size = previous_size;
    ctx->variable_base = previous_base;
    ctx->captures = previous_captures;
    ctx->capture_count = previous_capture_count;
    ctx->functions = previous_functions;
    ctx->imports = previous_imports;
    ctx->loop_depth = previous_loop_depth;
    ctx->call_depth--;
    ctx->script = previous_program;
    return status;
}

int script_collection_convert(
    script_eval_t *ctx,
    const char *position,
    const script_value_t *source,
    sireflect_handle_t type,
    script_value_t *destination
) {
    if (!source->collection)
        return script_eval_error(ctx, position, "expected collection value");
    *destination =
        (script_value_t){ .collection = true, .item_type = type, .count = source->count };
    destination->items = script_alloc(&ctx->values, source->count * sizeof(script_value_t));
    for (uint32_t i = 0; i < source->count; i++) {
        script_value_t item = script_collection_at(source, i);
        void *storage = script_value_storage(ctx, type, &destination->items[i]);
        if (script_value_convert(ctx, position, &item, type, storage))
            return -1;
    }
    return 0;
}

script_value_t script_collection_at(const script_value_t *collection, uint32_t index) {
    if (collection->range)
        return (script_value_t){ .type = script_backend_type_lookup("int64_t"),
                                 .storage.i64 = collection->range_start + (int64_t)index };
    return collection->items[index];
}

static bool integer_value(const script_value_t *value, bool *is_unsigned, uint64_t *bits) {
    if (!value->type || value->collection || value->future)
        return false;
    const sireflect_type_info_t *info = sireflect_type_info(value->type);
    if (sireflect_type_is_floating(value->type) ||
        (!sireflect_is_numeric(info->kind) && info->kind != sireflect_kind_enum &&
         info->kind != sireflect_kind_bool && info->kind != sireflect_kind_char))
        return false;
    *is_unsigned = value_is_unsigned(info->kind) || info->kind == sireflect_kind_bool;
    *bits = 0;
    const void *data = script_value_data(value);
    if (*is_unsigned)
        memcpy(bits, data, info->size);
    else {
        int64_t signed_value = 0;
        switch (info->size) {
        case 1: {
            int8_t number;
            memcpy(&number, data, 1);
            signed_value = number;
            break;
        }
        case 2: {
            int16_t number;
            memcpy(&number, data, 2);
            signed_value = number;
            break;
        }
        case 4: {
            int32_t number;
            memcpy(&number, data, 4);
            signed_value = number;
            break;
        }
        case 8:
            memcpy(&signed_value, data, 8);
            break;
        default:
            return false;
        }
        memcpy(bits, &signed_value, sizeof(*bits));
    }
    return true;
}

static int compare_order(int ordering, int operation, bool *result) {
    switch (operation) {
    case EcsTokEq:
        *result = ordering == 0;
        break;
    case EcsTokNeq:
        *result = ordering != 0;
        break;
    case EcsTokGt:
        *result = ordering > 0;
        break;
    case EcsTokGtEq:
        *result = ordering >= 0;
        break;
    case EcsTokLt:
        *result = ordering < 0;
        break;
    case EcsTokLtEq:
        *result = ordering <= 0;
        break;
    default:
        return -1;
    }
    return 0;
}

static bool collection_view(script_eval_t *ctx, const script_value_t *value, script_value_t *view) {
    *view = *value;
    if (value->collection)
        return true;
    if (!value->type || value->type == script_backend_type_lookup("entity"))
        return false;
    const sireflect_type_info_t *info = sireflect_type_info(value->type);
    bool vector = info->kind == sireflect_kind_vector;
    bool sequence = vector || info->kind == sireflect_kind_array;
    if (!sequence && !sireflect_type_is_struct(info))
        return false;
    const void *data = script_value_data(value);
    size_t count = sequence ? info->element_count : info->fields.field_count;
    if (vector) {
        const sireflect_vector_t *storage = data;
        count = storage->count;
        data = storage->data;
    }
    if (count > UINT32_MAX || count > ctx->instruction_budget)
        return false;
    if (sireflect_type_is_struct(info)) {
        for (size_t i = 0; i < count; i++) {
            const sireflect_type_info_t *field = sireflect_type_info(info->fields.fields[i].type);
            if (!sireflect_is_numeric(field->kind) && field->kind != sireflect_kind_bool)
                return false;
        }
    }
    *view = (script_value_t){ .collection = true,
                              .count = (uint32_t)count,
                              .item_type = sequence ? info->element_type : 0 };
    view->items = script_alloc(&ctx->values, count * sizeof(script_value_t));
    for (uint32_t i = 0; i < count; i++) {
        sireflect_handle_t type = sequence ? info->element_type : info->fields.fields[i].type;
        size_t offset = sequence ? i * sireflect_type_size(type) : info->fields.fields[i].offset;
        view->items[i] = (script_value_t){ .type = type, .ptr = (const char *)data + offset };
    }
    return true;
}

int script_binary_values(
    script_eval_t *ctx,
    const char *position,
    int operation,
    const script_value_t *left,
    const script_value_t *right,
    script_value_t *out
) {
    bool equality = operation == EcsTokEq || operation == EcsTokNeq;
    if (left->future || right->future) {
        if (!equality)
            return script_eval_error(ctx, position, "task values require await");
        out->type = script_backend_type_lookup("bool");
        out->storage.bool_ = (left->future == right->future) == (operation == EcsTokEq);
        return 0;
    }
    if (equality && left->type && left->type == right->type &&
        (sireflect_type_is_struct(sireflect_type_info(left->type)) ||
         sireflect_type_info(left->type)->kind == sireflect_kind_array)) {
        bool equal = ecs_reflected_value_equal(
            left->type,
            script_value_data(left),
            script_value_data(right)
        );
        *out = (script_value_t){ .type = script_backend_type_lookup("bool"),
                                 .storage.bool_ = equal == (operation == EcsTokEq) };
        return 0;
    }
    script_value_t a_view, b_view;
    bool a_collection = collection_view(ctx, left, &a_view);
    bool b_collection = collection_view(ctx, right, &b_view);
    if (a_collection || b_collection) {
        uint32_t count = a_collection ? a_view.count : b_view.count;
        if (a_collection && b_collection && a_view.count != b_view.count) {
            if (equality) {
                *out = (script_value_t){ .type = script_backend_type_lookup("bool"),
                                         .storage.bool_ = operation == EcsTokNeq };
                return 0;
            }
            return script_eval_error(ctx, position, "collection lengths differ");
        }
        if (count > ctx->instruction_budget)
            return script_eval_error(ctx, position, "collection instruction budget exhausted");
        ctx->instruction_budget -= count;
        if (!equality) {
            out->collection = true;
            out->count = count;
            out->items = script_alloc(&ctx->values, count * sizeof(script_value_t));
        }
        bool all_equal = true;
        for (uint32_t i = 0; i < count; i++) {
            script_value_t a = a_collection ? script_collection_at(&a_view, i) : *left;
            script_value_t b = b_collection ? script_collection_at(&b_view, i) : *right;
            script_value_t result = { 0 };
            if (script_binary_values(
                    ctx,
                    position,
                    equality ? EcsTokEq : operation,
                    &a,
                    &b,
                    &result
                ))
                return -1;
            if (equality)
                all_equal &= result.storage.bool_;
            else
                out->items[i] = result;
        }
        if (equality)
            *out = (script_value_t){ .type = script_backend_type_lookup("bool"),
                                     .storage.bool_ = all_equal == (operation == EcsTokEq) };
        return 0;
    }
    bool a_string = left->type && sireflect_type_is_cstring(left->type);
    bool b_string = right->type && sireflect_type_is_cstring(right->type);
    if (a_string || b_string) {
        if (!a_string || !b_string)
            return script_eval_error(ctx, position, "incompatible string operand");
        const char *a = *(const char *const *)script_value_data(left);
        const char *b = *(const char *const *)script_value_data(right);
        if (operation == EcsTokAdd) {
            size_t first = a ? strlen(a) : 0, second = b ? strlen(b) : 0;
            if (second >= SIZE_MAX - first)
                return script_eval_error(ctx, position, "string exceeds size limit");
            char *text = script_alloc(&ctx->values, first + second + 1);
            if (first)
                memcpy(text, a, first);
            if (second)
                memcpy(text + first, b, second);
            *out = (script_value_t){ .type = script_backend_type_lookup("string"),
                                     .storage.string = text };
            return 0;
        }
        out->type = script_backend_type_lookup("bool");
        int order = a && b ? strcmp(a, b) : (a != NULL) - (b != NULL);
        if (!compare_order(order, operation, &out->storage.bool_))
            return 0;
        return script_eval_error(ctx, position, "invalid string operator");
    }
    uint64_t a_bits, b_bits;
    bool a_unsigned, b_unsigned;
    bool a_integer = integer_value(left, &a_unsigned, &a_bits);
    bool b_integer = integer_value(right, &b_unsigned, &b_bits);
    bool comparison = equality || operation == EcsTokLt || operation == EcsTokLtEq ||
                      operation == EcsTokGt || operation == EcsTokGtEq;
    bool logical = operation == EcsTokAnd || operation == EcsTokOr;
    if (a_integer && b_integer) {
        int64_t a_signed, b_signed;
        memcpy(&a_signed, &a_bits, sizeof a_signed);
        memcpy(&b_signed, &b_bits, sizeof b_signed);
        bool a_negative = !a_unsigned && a_signed < 0, b_negative = !b_unsigned && b_signed < 0;
        if (comparison || logical) {
            out->type = script_backend_type_lookup("bool");
            if (logical)
                out->storage.bool_ = operation == EcsTokAnd ? a_bits && b_bits : a_bits || b_bits;
            else {
                int ordering;
                if (a_negative != b_negative)
                    ordering = a_negative ? -1 : 1;
                else if (a_negative)
                    ordering = (a_signed > b_signed) - (a_signed < b_signed);
                else
                    ordering = (a_bits > b_bits) - (a_bits < b_bits);
                compare_order(ordering, operation, &out->storage.bool_);
            }
            return 0;
        }
        bool use_unsigned =
            (a_unsigned && b_unsigned) ||
            (!a_negative && !b_negative && (a_bits > INT64_MAX || b_bits > INT64_MAX));
        if (use_unsigned) {
            bool overflow = (operation == EcsTokAdd && a_bits > UINT64_MAX - b_bits) ||
                            (operation == EcsTokSub && a_bits < b_bits) ||
                            (operation == EcsTokMul && b_bits && a_bits > UINT64_MAX / b_bits);
            if (overflow)
                return script_eval_error(ctx, position, "unsigned integer overflow");
            if ((operation == EcsTokDiv || operation == EcsTokMod) && !b_bits)
                return script_eval_error(ctx, position, "division by zero");
            if (operation == EcsTokShiftLeft || operation == EcsTokShiftRight) {
                if (b_bits >= 64)
                    return script_eval_error(ctx, position, "invalid shift count");
                if (operation == EcsTokShiftLeft && a_bits > (UINT64_MAX >> b_bits))
                    return script_eval_error(ctx, position, "unsigned integer overflow");
            }
            out->type = script_backend_type_lookup("uint64_t");
            if (!script_operator_uint64_t(
                    &a_bits,
                    &b_bits,
                    &out->storage,
                    (ecs_token_kind_t)operation
                ))
                return 0;
        } else {
            if ((a_unsigned && a_bits > INT64_MAX) || (b_unsigned && b_bits > INT64_MAX))
                return script_eval_error(
                    ctx,
                    position,
                    "mixed integer operands require an explicit conversion"
                );
            int64_t a = a_signed, b = b_signed;
            bool overflow = false;
            if (operation == EcsTokAdd)
                overflow = b > 0 ? a > INT64_MAX - b : b < 0 && a < INT64_MIN - b;
            if (operation == EcsTokSub)
                overflow = b < 0 ? a > INT64_MAX + b : b > 0 && a < INT64_MIN + b;
            if (operation == EcsTokMul && a && b) {
                if (a > 0)
                    overflow = b > 0 ? a > INT64_MAX / b : b < INT64_MIN / a;
                else
                    overflow = b > 0 ? a < INT64_MIN / b : b < INT64_MAX / a;
            }
            if ((operation == EcsTokDiv || operation == EcsTokMod) && !b)
                return script_eval_error(ctx, position, "division by zero");
            if ((operation == EcsTokDiv || operation == EcsTokMod) && a == INT64_MIN && b == -1)
                overflow = true;
            if (operation == EcsTokShiftLeft || operation == EcsTokShiftRight) {
                if (b < 0 || b >= 64 || a < 0)
                    return script_eval_error(ctx, position, "invalid signed shift");
                if (operation == EcsTokShiftLeft && (uint64_t)a > ((uint64_t)INT64_MAX >> b))
                    overflow = true;
            }
            if (overflow)
                return script_eval_error(ctx, position, "integer overflow");
            out->type = script_backend_type_lookup("int64_t");
            if (!script_operator_int64_t(&a, &b, &out->storage, (ecs_token_kind_t)operation))
                return 0;
        }
        return script_eval_error(ctx, position, "invalid integer operator");
    }
    long double a, b;
    if (value_number(left, &a) || value_number(right, &b))
        return script_eval_error(ctx, position, "expected numeric operands");
    if (logical) {
        *out = (script_value_t){ .type = script_backend_type_lookup("bool"),
                                 .storage.bool_ = operation == EcsTokAnd ? a && b : a || b };
        return 0;
    }
    if ((operation == EcsTokDiv || operation == EcsTokMod) && !b)
        return script_eval_error(ctx, position, "division by zero");
    double first = (double)a, second = (double)b;
    out->type = script_backend_type_lookup(comparison ? "bool" : "double");
    if (!script_operator_double(&first, &second, &out->storage, (ecs_token_kind_t)operation))
        return 0;
    return script_eval_error(ctx, position, "invalid floating-point operator");
}
