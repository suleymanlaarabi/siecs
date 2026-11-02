/* Derived from Flecs. Copyright (c) 2025 Sander Mertens.
 * Portions Copyright (c) Meta Platforms, Inc. and affiliates.
 * Distributed under the MIT license in addons/script/LICENSE.flecs. */
/**
 * @file addons/script/expr/ast.c
 * @brief Script expression AST implementation.
 */

#include "../script.h"

#ifdef SIECS_SCRIPT
#include "../script.h"


#define siecs_expr_ast_new(parser, T, kind)\
    (T*)siecs_expr_ast_new_(parser, ECS_SIZEOF(T), kind)

static void* siecs_expr_ast_new_(
    ecs_parser_t *parser,
    ecs_size_t size,
    ecs_expr_node_kind_t kind)
{
    ecs_assert(parser->script != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_allocator_t *a = &parser->script->allocator;
    ecs_expr_node_t *result = siecs_calloc_w_dbg_info(a, size,
        "ecs_expr_node_t");
    result->kind = kind;
    result->alloc_size = size;
    result->pos = parser->pos;
    return result;
}

ecs_expr_value_node_t* siecs_expr_value_from(
    ecs_script_t *script,
    ecs_expr_node_t *node,
    sireflect_handle_t type)
{
    ecs_expr_value_node_t *result = siecs_calloc_t(
        &((ecs_script_impl_t*)script)->allocator, ecs_expr_value_node_t);
    result->ptr = &result->storage.u64;
    result->node.kind = EcsExprValue;
    result->node.alloc_size = ECS_SIZEOF(ecs_expr_value_node_t);
    result->node.pos = node ? node->pos : NULL;
    result->node.end = node ? node->end : NULL;
    result->node.type = type;
    result->node.type_info = NULL /* resolved through sireflect during type checking */;
    return result;
}

ecs_expr_variable_t* siecs_expr_variable_from(
    ecs_script_t *script,
    ecs_expr_node_t *node,
    const char *name)
{
    ecs_expr_variable_t *result = siecs_calloc_t(
        &((ecs_script_impl_t*)script)->allocator, ecs_expr_variable_t);
    result->name = name;
    result->sp = -1;
    result->node.kind = EcsExprVariable;
    result->node.alloc_size = ECS_SIZEOF(ecs_expr_variable_t);
    result->node.pos = node ? node->pos : NULL;
    result->node.end = node ? node->end : NULL;
    return result;
}

ecs_expr_member_t* siecs_expr_member_from(
    ecs_script_t *script,
    ecs_expr_node_t *node,
    const char *name)
{
    ecs_expr_member_t *result = siecs_calloc_t(
        &siecs_script_impl(script)->allocator, ecs_expr_member_t);
    result->node.kind = EcsExprMember;
    result->node.alloc_size = ECS_SIZEOF(ecs_expr_member_t);
    result->node.pos = node->pos;
    result->node.end = node->end;
    result->left = node;
    result->member_name =name;
    return result;
}

ecs_expr_swizzle_t* siecs_expr_swizzle_from(
    ecs_script_t *script,
    ecs_expr_node_t *node,
    ecs_expr_node_t *left,
    const char *name)
{
    ecs_expr_swizzle_t *result = siecs_calloc_t(
        &siecs_script_impl(script)->allocator, ecs_expr_swizzle_t);
    result->node.kind = EcsExprSwizzle;
    result->node.alloc_size = ECS_SIZEOF(ecs_expr_swizzle_t);
    result->node.pos = node->pos;
    result->node.end = node->end;
    result->left = left;
    result->name = name;
    return result;
}

ecs_expr_value_node_t* siecs_expr_bool(
    ecs_parser_t *parser,
    bool value)
{
    ecs_expr_value_node_t *result = siecs_expr_ast_new(
        parser, ecs_expr_value_node_t, EcsExprValue);
    result->storage.bool_ = value;
    result->ptr = &result->storage.bool_;
    result->literal_kind = ScriptLiteralBool;
    return result;
}

ecs_expr_value_node_t* siecs_expr_char(
    ecs_parser_t *parser,
    const char *value)
{
    ecs_expr_value_node_t *result = siecs_expr_ast_new(
        parser, ecs_expr_value_node_t, EcsExprValue);
    if(value[0] == '\\' && value[1] == '\'') {
        result->storage.char_ = '\'';
    } else {
        char ch = 0;
        const char *ptr = siecs_chrparse(value, &ch);
        if(!ptr) {
            siecs_free_t(
                &parser->script->allocator, ecs_expr_value_node_t, result);
            return NULL;
        }

        result->storage.char_ = ch;
    }

    result->ptr = &result->storage.char_;
    result->literal_kind = ScriptLiteralChar;
    return result;
}

ecs_expr_value_node_t* siecs_expr_int(
    ecs_parser_t *parser,
    int64_t value)
{
    ecs_expr_value_node_t *result = siecs_expr_ast_new(
        parser, ecs_expr_value_node_t, EcsExprValue);
    result->storage.i64 = value;
    result->ptr = &result->storage.i64;
    result->literal_kind = ScriptLiteralI64;
    return result;
}

ecs_expr_value_node_t* siecs_expr_uint(
    ecs_parser_t *parser,
    uint64_t value)
{
    ecs_expr_value_node_t *result = siecs_expr_ast_new(
        parser, ecs_expr_value_node_t, EcsExprValue);
    result->storage.u64 = value;
    result->ptr = &result->storage.u64;
    if (value > INT64_MAX) {
        result->literal_kind = ScriptLiteralU64;
    } else {
        result->literal_kind = ScriptLiteralI64;
    }

    return result;
}

ecs_expr_value_node_t* siecs_expr_float(
    ecs_parser_t *parser,
    double value)
{
    ecs_expr_value_node_t *result = siecs_expr_ast_new(
        parser, ecs_expr_value_node_t, EcsExprValue);
    result->storage.f64 = value;
    result->ptr = &result->storage.f64;
    result->literal_kind = ScriptLiteralF64;
    return result;
}

ecs_expr_value_node_t* siecs_expr_string(
    ecs_parser_t *parser,
    const char *value)
{
    char *str = ECS_CONST_CAST(char*, value);
    ecs_expr_value_node_t *result = siecs_expr_ast_new(
        parser, ecs_expr_value_node_t, EcsExprValue);
    result->storage.string = str;
    result->ptr = &result->storage.string;
    result->literal_kind = ScriptLiteralString;

    if (!siecs_string_escape(str)) {
        siecs_free_t(&parser->script->allocator, ecs_expr_value_node_t, result);
        return NULL;
    }

    return result;
}

ecs_expr_interpolated_string_t* siecs_expr_interpolated_string(
    ecs_parser_t *parser,
    const char *value)
{
    ecs_expr_interpolated_string_t *result = siecs_expr_ast_new(
        parser, ecs_expr_interpolated_string_t, EcsExprInterpolatedString);
    result->value = siecs_strdup(&parser->script->allocator, value);
    result->buffer = siecs_strdup(&parser->script->allocator, value);
    result->buffer_size = ecs_os_strlen(result->buffer) + 1;
    result->node.type = 0; /* Bound to sireflect string during type resolution. */
    ecs_vec_init_t(&parser->script->allocator, &result->fragments,
        ecs_expr_fragment_t, 0);

    return result;
}

ecs_expr_initializer_t* siecs_expr_initializer(
    ecs_parser_t *parser)
{
    ecs_expr_initializer_t *result = siecs_expr_ast_new(
        parser, ecs_expr_initializer_t, EcsExprInitializer);
    ecs_vec_init_t(&parser->script->allocator, &result->elements,
        ecs_expr_initializer_element_t, 0);
    return result;
}

ecs_expr_identifier_t* siecs_expr_identifier(
    ecs_parser_t *parser,
    const char *value)
{
    ecs_expr_identifier_t *result = siecs_expr_ast_new(
        parser, ecs_expr_identifier_t, EcsExprIdentifier);
    result->value = value;
    result->symbol = -1;
    return result;
}

ecs_expr_variable_t* siecs_expr_variable(
    ecs_parser_t *parser,
    const char *value)
{
    ecs_expr_variable_t *result = siecs_expr_ast_new(
        parser, ecs_expr_variable_t, EcsExprVariable);
    result->name = value;
    result->sp = -1;
    return result;
}

ecs_expr_unary_t* siecs_expr_unary(
    ecs_parser_t *parser)
{
    ecs_expr_unary_t *result = siecs_expr_ast_new(
        parser, ecs_expr_unary_t, EcsExprUnary);
    return result;
}

ecs_expr_binary_t* siecs_expr_binary(
    ecs_parser_t *parser)
{
    ecs_expr_binary_t *result = siecs_expr_ast_new(
        parser, ecs_expr_binary_t, EcsExprBinary);
    return result;
}

ecs_expr_member_t* siecs_expr_member(
    ecs_parser_t *parser)
{
    ecs_expr_member_t *result = siecs_expr_ast_new(
        parser, ecs_expr_member_t, EcsExprMember);
    return result;
}

ecs_expr_function_t* siecs_expr_function(
    ecs_parser_t *parser)
{
    ecs_expr_function_t *result = siecs_expr_ast_new(
        parser, ecs_expr_function_t, EcsExprFunction);
    return result;
}

ecs_expr_element_t* siecs_expr_element(
    ecs_parser_t *parser)
{
    ecs_expr_element_t *result = siecs_expr_ast_new(
        parser, ecs_expr_element_t, EcsExprElement);
    return result;
}

ecs_expr_has_t* siecs_expr_has(
    ecs_parser_t *parser)
{
    ecs_expr_has_t *result = siecs_expr_ast_new(
        parser, ecs_expr_has_t, EcsExprHas);
    return result;
}

ecs_expr_match_t* siecs_expr_match(
    ecs_parser_t *parser)
{
    ecs_expr_match_t *result = siecs_expr_ast_new(
        parser, ecs_expr_match_t, EcsExprMatch);
    return result;
}

ecs_expr_range_t* siecs_expr_range(
    ecs_parser_t *parser)
{
    ecs_expr_range_t *result = siecs_expr_ast_new(
        parser, ecs_expr_range_t, EcsExprRange);
    return result;
}

ecs_expr_new_t* siecs_expr_new(
    ecs_parser_t *parser)
{
    ecs_expr_new_t *result = siecs_expr_ast_new(
        parser, ecs_expr_new_t, EcsExprNew);
    return result;
}

ecs_expr_script_t* siecs_expr_script(
    ecs_parser_t *parser)
{
    ecs_expr_script_t *result = siecs_expr_ast_new(
        parser, ecs_expr_script_t, EcsExprScript);
    return result;
}


#endif
