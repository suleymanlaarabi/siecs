#include "script.h"
#include <errno.h>

static int script_file_error(const char *filename, const char *message,
    ecs_script_eval_result_t *result) {
    if (result) siecs_log_capture_push(true);
    ecs_parser_error(filename, NULL, 0, "%s", message);
    if (result) result->error = siecs_log_capture_pop();
    return -1;
}

int ecs_script_run_file_w_desc(const char *filename,
    const ecs_script_eval_desc_t *desc, ecs_script_eval_result_t *result) {
    if (result) *result = (ecs_script_eval_result_t){0};
    if (!filename) return script_file_error(NULL, "filename must not be null", result);
    FILE *file = fopen(filename, "rb");
    if (!file) return script_file_error(filename, strerror(errno), result);
    long length;
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
        length >= INT32_MAX || fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return script_file_error(filename, "cannot read script file size", result);
    }
    char *source = malloc((size_t)length + 1);
    if (!source) {
        fclose(file);
        return script_file_error(filename, "out of memory reading script", result);
    }
    size_t read = fread(source, 1, (size_t)length, file);
    bool failed = ferror(file) || read != (size_t)length;
    fclose(file);
    if (failed || memchr(source, '\0', read)) {
        free(source);
        return script_file_error(filename, "invalid or incomplete script file", result);
    }
    source[read] = '\0';
    int status = ecs_script_run_w_desc(filename, source, desc, result);
    free(source);
    return status;
}

int ecs_script_run_file(const char *filename, ecs_script_eval_result_t *result) {
    return ecs_script_run_file_w_desc(filename, NULL, result);
}

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
