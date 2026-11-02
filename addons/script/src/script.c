#include "script.h"

ecs_script_t *siecs_script_new(void) {
    ecs_script_impl_t *script = calloc(1, sizeof(*script));
    if (!script) abort();
    sicore_vec_init(&script->nested_scripts, sizeof(ecs_script_t*));
    ecs_parser_t parser = { .script = script };
    script->root = siecs_script_scope_new(&parser);
    return &script->pub;
}

void script_adopt_nested(ecs_script_impl_t *parent, ecs_script_t *child) {
    sicore_vec_push(&parent->nested_scripts, &child, sizeof(child));
}

void ecs_script_free(ecs_script_t *script) {
    if (!script) return;
    ecs_script_impl_t *impl = siecs_script_impl(script);
    ecs_script_t **nested = impl->nested_scripts.data;
    for (uint32_t i = 0; i < impl->nested_scripts.size; i++) {
        ecs_script_free(nested[i]);
    }
    sicore_vec_fini(&impl->nested_scripts);
    script_allocator_fini(&impl->allocator);
    free((char*)script->code);
    free((char*)script->name);
    free(impl);
}

void siecs_expr_visit_free(ecs_script_t *script, ecs_expr_node_t *node) {
    /* Parse branches can abandon nodes; the script arena owns all of them. */
    (void)script;
    (void)node;
}
