#include <test.h>
#include "script.h"

static ecs_script_t *parse(const char *source) {
    ecs_script_eval_result_t result = {0};
    ecs_script_t *script = ecs_script_parse("parser-test", source, NULL, &result);
    if (result.error) fprintf(stderr, "%d:%d: %s\n", result.line, result.column, result.error);
    test_true(script != NULL);
    test_true(result.error == NULL);
    free(result.error);
    return script;
}

void Parser_entity_scope(void) {
    ecs_script_t *script = parse("Root {\n Child {}\n}\n");
    ecs_script_scope_t *root = siecs_script_impl(script)->root;
    test_int(ecs_vec_count(&root->stmts), 1);
    ecs_script_entity_t *entity = ecs_vec_first_t(&root->stmts, ecs_script_entity_t*)[0];
    test_int(entity->node.kind, EcsAstEntity);
    test_str(entity->name, "Root");
    test_int(ecs_vec_count(&entity->scope->stmts), 1);
    ecs_script_entity_t *child = ecs_vec_first_t(&entity->scope->stmts, ecs_script_entity_t*)[0];
    test_str(child->name, "Child");
    test_true(entity->node.pos >= script->code);
    test_true(entity->node.end > entity->node.pos);
    test_true(child->scope->parent == entity->scope);
    ecs_script_free(script);
}

void Parser_expression_precedence(void) {
    ecs_script_t *script = parse("const x = 1 + 2 * 3\n");
    ecs_script_var_node_t *var = ecs_vec_first_t(&siecs_script_impl(script)->root->stmts, ecs_script_var_node_t*)[0];
    ecs_expr_binary_t *add = (ecs_expr_binary_t*)var->expr;
    test_int(add->node.kind, EcsExprBinary);
    test_int(add->operator, EcsTokAdd);
    ecs_expr_binary_t *mul = (ecs_expr_binary_t*)add->right;
    test_int(mul->node.kind, EcsExprBinary);
    test_int(mul->operator, EcsTokMul);
    test_int(((ecs_expr_value_node_t*)mul->left)->storage.i64, 2);
    ecs_script_free(script);
}

void Parser_template_and_async(void) {
    ecs_script_t *script = parse("template Tree {\n prop height = 10\n async {\n await true\n }\n}\n");
    ecs_script_template_node_t *node = ecs_vec_first_t(&siecs_script_impl(script)->root->stmts, ecs_script_template_node_t*)[0];
    test_int(node->node.kind, EcsAstTemplate);
    test_str(node->name, "Tree");
    test_int(ecs_vec_count(&node->scope->stmts), 2);
    ecs_script_node_t **statements = node->scope->stmts.data;
    test_int(statements[0]->kind, EcsAstProp);
    test_int(statements[1]->kind, EcsAstAsync);
    ecs_script_free(script);
}

void Parser_diagnostics(void) {
    ecs_script_eval_result_t result = {0};
    ecs_script_t *script = ecs_script_parse("broken", "Root {\n Child {\n", NULL, &result);
    test_true(script == NULL);
    test_true(result.error != NULL);
    test_true(strstr(result.error, "unexpected end") != NULL);
    test_true(result.line >= 2);
    test_true(result.column >= 1);
    free(result.error);
}

void Parser_depth_limit(void) {
    char source[1024] = {0};
    for (int i = 0; i < 100; i++) strcat(source, "A {\n");
    for (int i = 0; i < 100; i++) strcat(source, "}\n");
    ecs_script_eval_result_t result = {0};
    test_true(ecs_script_parse("deep", source, NULL, &result) == NULL);
    test_true(result.error != NULL);
    test_true(strstr(result.error, "nesting depth") != NULL);
    free(result.error);
}

void Parser_repeated_parse(void) {
    for (int i = 0; i < 1000; i++) {
        ecs_script_t *script = parse("const n = [1, 2, 3]\nRoot { Position: {x: 10, y: 20} }\n");
        ecs_script_free(script);
    }
}

void Parser_nested_script(void) {
    ecs_script_t *script = parse("const body = script { Root { Child {} } }\n");
    test_int(siecs_script_impl(script)->nested_scripts.size, 1);
    ecs_script_free(script);
}
