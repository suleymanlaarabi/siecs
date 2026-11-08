#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif
#include "script.h"
#include <errno.h>

static int
script_file_error(const char *filename, const char *message, ecs_script_eval_result_t *result) {
    if (result)
        siecs_log_capture_push(true);
    ecs_parser_error(filename, NULL, 0, "%s", message);
    if (result)
        result->error = siecs_log_capture_pop();
    return -1;
}

static char *script_read_source(const char *filename, ecs_script_eval_result_t *result) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        script_file_error(filename, strerror(errno), result);
        return NULL;
    }
    long length = 0;
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 || length >= INT32_MAX ||
        fseek(file, 0, SEEK_SET)) {
        fclose(file);
        script_file_error(filename, "cannot read script file size", result);
        return NULL;
    }
    char *source = malloc((size_t)length + 1);
    if (!source) {
        fclose(file);
        script_file_error(filename, "out of memory reading script", result);
        return NULL;
    }
    size_t read = fread(source, 1, (size_t)length, file);
    bool failed = ferror(file) || read != (size_t)length;
    fclose(file);
    if (failed || memchr(source, '\0', read)) {
        free(source);
        script_file_error(filename, "invalid or incomplete script file", result);
        return NULL;
    }
    source[read] = 0;
    return source;
}

ecs_script_t *ecs_script_parse_file(
    const char *filename,
    const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result
) {
    if (result)
        *result = (ecs_script_eval_result_t){ 0 };
    if (!filename) {
        script_file_error(NULL, "filename must not be null", result);
        return NULL;
    }
#ifdef _WIN32
    char *canonical = _fullpath(NULL, filename, 0);
#else
    char *canonical = realpath(filename, NULL);
#endif
    if (!canonical) {
        script_file_error(filename, strerror(errno), result);
        return NULL;
    }
    static SCRIPT_THREAD_LOCAL const char *loading[64];
    static SCRIPT_THREAD_LOCAL uint32_t loading_count;
    for (uint32_t i = 0; i < loading_count; i++) {
        if (!strcmp(loading[i], canonical)) {
            script_file_error(filename, "cyclic include", result);
            free(canonical);
            return NULL;
        }
    }
    if (loading_count == 64) {
        script_file_error(filename, "include nesting depth exceeded", result);
        free(canonical);
        return NULL;
    }
    char *source = script_read_source(canonical, result);
    if (!source) {
        free(canonical);
        return NULL;
    }
    loading[loading_count++] = canonical;
    ecs_script_t *script = ecs_script_parse(canonical, source, desc, result);
    loading_count--;
    free(source);
    free(canonical);
    if (script)
        siecs_script_impl(script)->from_file = true;
    return script;
}

int ecs_script_run_file_w_desc(
    const char *filename,
    const ecs_script_eval_desc_t *desc,
    ecs_script_eval_result_t *result
) {
    ecs_script_t *script = ecs_script_parse_file(filename, desc, result);
    if (!script)
        return -1;
    int status = ecs_script_eval(script, desc, result);
    ecs_script_free(script);
    return status;
}

int ecs_script_run_file(const char *filename, ecs_script_eval_result_t *result) {
    return ecs_script_run_file_w_desc(filename, NULL, result);
}

ecs_script_t *siecs_script_new(void) {
    ecs_script_impl_t *script = calloc(1, sizeof(*script));
    if (!script)
        abort();
    atomic_init(&script->references, 1);
    sicore_vec_init(&script->nested_scripts, sizeof(ecs_script_t *));
    ecs_parser_t parser = { .script = script };
    script->root = siecs_script_scope_new(&parser);
    return &script->pub;
}

void script_adopt_nested(ecs_script_impl_t *parent, ecs_script_t *child) {
    sicore_vec_push(&parent->nested_scripts, &child, sizeof(child));
}

void ecs_script_free(ecs_script_t *script) {
    if (!script)
        return;
    ecs_script_impl_t *impl = siecs_script_impl(script);
    if (atomic_fetch_sub_explicit(&impl->references, 1, memory_order_acq_rel) != 1)
        return;
    ecs_script_t **nested = impl->nested_scripts.data;
    for (uint32_t i = 0; i < impl->nested_scripts.size; i++) {
        ecs_script_free(nested[i]);
    }
    sicore_vec_fini(&impl->nested_scripts);
    /* Parse branches can abandon nodes; the script arena owns all of them. */
    script_allocator_fini(&impl->allocator);
    free((char *)script->code);
    free((char *)script->name);
    free(impl);
}

const ecs_script_t *ecs_script_retain(const ecs_script_t *script) {
    if (script)
        atomic_fetch_add_explicit(&siecs_script_impl(script)->references, 1, memory_order_relaxed);
    return script;
}

uint64_t script_source_hash(const char *begin, const char *end) {
    uint64_t hash = UINT64_C(1469598103934665603);
    if (!begin || !end)
        return hash;
    size_t length = (size_t)(end - begin);
    char *source = malloc(length + 1);
    assert(source != NULL);
    memcpy(source, begin, length);
    source[length] = '\0';
    ecs_script_impl_t scratch = { 0 };
    ecs_parser_t parser = { .script = &scratch,
                            .name = "<continuation>",
                            .code = source,
                            .pos = source };
    script_lex(&parser);
    while (parser.token.kind) {
        if ((int)parser.token.kind != '\n') {
            hash = (hash ^ (uint32_t)parser.token.kind) * UINT64_C(1099511628211);
            for (const unsigned char *at = (const unsigned char *)parser.token.value; *at; at++)
                hash = (hash ^ *at) * UINT64_C(1099511628211);
            hash = (hash ^ UINT64_C(0xff)) * UINT64_C(1099511628211);
        }
        script_lex(&parser);
    }
    script_allocator_fini(&scratch.allocator);
    free(source);
    return hash;
}
