#include "script.h"

void *script_node(ecs_parser_t *parser, size_t size, int kind, bool statement) {
    void *memory = script_alloc(&parser->script->allocator, size);
    if (statement) {
        ecs_script_node_t *node = memory;
        node->kind = (ecs_script_node_kind_t)kind;
        node->pos = parser->stmt_pos ? parser->stmt_pos : parser->token.begin;
        ecs_script_node_t **slot =
            script_vec_append(&parser->script->allocator, &parser->scope->stmts, sizeof(*slot));
        *slot = node;
    } else {
        ecs_expr_node_t *node = memory;
        node->kind = (ecs_expr_node_kind_t)kind;
        node->pos = parser->token.begin;
    }
    return memory;
}

ecs_script_scope_t *siecs_script_scope_new(ecs_parser_t *parser) {
    ecs_script_scope_t *scope = script_alloc(&parser->script->allocator, sizeof(*scope));
    scope->node.kind = EcsAstScope;
    scope->node.pos = parser->token.begin;
    scope->parent = parser->scope;
    scope->next = parser->script->scopes;
    parser->script->scopes = scope;
    return scope;
}
