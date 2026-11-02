/* Derived from Flecs. Copyright (c) 2025 Sander Mertens.
 * Portions Copyright (c) Meta Platforms, Inc. and affiliates.
 * Distributed under the MIT license in addons/script/LICENSE.flecs. */
/* Ported lexical fragmentation from Flecs expr/visit_type.c. */
#include "../script.h"

int script_prepare_interpolation(
    ecs_script_t *script,
    ecs_expr_interpolated_string_t *node)
{
    if (node->prepared) return 0;
    ecs_allocator_t *a = &siecs_script_impl(script)->allocator;
    char *ptr, *frag = NULL;
    char ch;

    for (ptr = node->value; (ch = ptr[0]); ) {
        if (ch == '\\') {
            ptr ++;
            if (ptr[0]) {
                ptr ++;
            }

            continue;
        }

        if ((ch == '$') && (isspace((unsigned char)ptr[1]) || !ptr[1])) {
            ptr ++;
            continue;
        }

        if (ch == '$' || ch == '{') {
            if (!frag) {
                frag = node->value;
            }

            char *frag_end = ptr;

            ecs_expr_fragment_t *fragment = ecs_vec_append_t(
                a, &node->fragments, ecs_expr_fragment_t);
            *fragment = (ecs_expr_fragment_t){.text = frag != ptr ? frag : NULL};
            ecs_expr_node_t **result = &fragment->expr;

            if (ch == '$') {
                char *var_name = ++ ptr;
                ptr = ECS_CONST_CAST(char*, siecs_tokenizer_identifier(
                    NULL, ptr, NULL));
                if (!ptr) {
                    goto error;
                }

                ecs_size_t var_name_pos = siecs_ito(int32_t, var_name - node->value);
                var_name = &node->buffer[var_name_pos];
                ecs_size_t var_name_end = siecs_ito(int32_t, ptr - node->value);
                node->buffer[var_name_end] = '\0';

                ecs_expr_variable_t *var = siecs_expr_variable_from(
                    script, (ecs_expr_node_t*)node, var_name);
                if (!var) {
                    goto error;
                }

                *result = (ecs_expr_node_t*)var;
            } else {
                ecs_script_impl_t *impl = siecs_script_impl(script);

                ecs_parser_t parser = {
                    .name = script->name,
                    .code = script->code,
                    .pos = node->node.pos,
                    .fixed_pos = node->node.pos,
                    .script = impl,
                    .scope = impl->root,
                    .significant_newline = false,
                    .token_cur = impl->token_remaining,
                    .token_end = &impl->token_buffer[impl->token_buffer_size]
                };

                ptr = ECS_CONST_CAST(char*, siecs_script_parse_expr(
                    &parser, ptr + 1, 0, result));
                if (!ptr) {
                    goto error;
                }

                impl->token_remaining = parser.token_cur;

                if (ptr[0] == ':') {
                    ecs_parser_error(script->name, script->code,
                        node->node.pos - script->code,
                        "interpolation format specifiers not implemented");
                    goto error;
                }

                if (ptr[0] != '}') {
                    ecs_parser_error(script->name, script->code, node->node.pos - script->code,
                        "expected '}' at end of interpolated expression");

                    goto error;
                }

                ptr ++;
            }

            ecs_assert(*result != NULL, ECS_INTERNAL_ERROR, NULL);

            frag_end[0] = '\0';

            frag = ptr;
            if (!ptr[0]) {
                break;
            }
        } else {
            ptr ++;
        }
    }

    ecs_assert(frag != NULL, ECS_INTERNAL_ERROR, NULL);

    if (frag != ptr) {
        ecs_vec_append_t(a, &node->fragments, ecs_expr_fragment_t)[0] =
            (ecs_expr_fragment_t){.text = frag};
    }

    ecs_expr_fragment_t *fragments = ecs_vec_first(&node->fragments);
    int32_t i, count = ecs_vec_count(&node->fragments);
    for (i = 0; i < count; i ++) {
        if (fragments[i].text && !siecs_string_escape(fragments[i].text)) {
            goto error;
        }
    }

    node->prepared = true;
    return 0;
error:
    return -1;
}
