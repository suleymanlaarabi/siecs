#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "script.h"
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

struct ecs_script_document_t {
    ecs_script_t **revisions;
    size_t count;
    size_t capacity;
    size_t current;
};

static int document_error(ecs_script_eval_result_t *result, const char *message) {
    if (result) {
        *result = (ecs_script_eval_result_t){ 0 };
        result->error = script_strdup(message);
    }
    return -1;
}

static bool document_reserve(ecs_script_document_t *document) {
    if (document->count < document->capacity)
        return true;
    size_t capacity = document->capacity ? document->capacity * 2 : 8;
    if (capacity < document->capacity || capacity > SIZE_MAX / sizeof(ecs_script_t *))
        return false;
    ecs_script_t **revisions = realloc(document->revisions, capacity * sizeof(*revisions));
    if (!revisions)
        return false;
    document->revisions = revisions;
    document->capacity = capacity;
    return true;
}

ecs_script_document_t *
ecs_script_document_new(const char *name, const char *code, ecs_script_eval_result_t *result) {
    if (result)
        *result = (ecs_script_eval_result_t){ 0 };
    if (!code) {
        document_error(result, "source must not be null");
        return NULL;
    }
    ecs_script_t *script = ecs_script_parse(name, code, NULL, result);
    if (!script)
        return NULL;
    ecs_script_document_t *document = calloc(1, sizeof(*document));
    if (!document || !document_reserve(document)) {
        ecs_script_free(script);
        free(document);
        document_error(result, "out of memory creating source document");
        return NULL;
    }
    document->revisions[document->count++] = script;
    return document;
}

void ecs_script_document_free(ecs_script_document_t *document) {
    if (!document)
        return;
    for (size_t i = 0; i < document->count; i++)
        ecs_script_free(document->revisions[i]);
    free(document->revisions);
    free(document);
}

const ecs_script_t *ecs_script_document_script(const ecs_script_document_t *document) {
    return document ? document->revisions[document->current] : NULL;
}

int ecs_script_document_edit(
    ecs_script_document_t *document,
    size_t offset,
    size_t length,
    const char *replacement,
    ecs_script_eval_result_t *result
) {
    if (result)
        *result = (ecs_script_eval_result_t){ 0 };
    if (!document || !replacement)
        return document_error(result, "document and replacement must not be null");
    const ecs_script_t *previous = ecs_script_document_script(document);
    size_t size = strlen(previous->code);
    if (offset > size || length > size - offset)
        return document_error(result, "edit range exceeds source size");
    size_t inserted = strlen(replacement);
    if (inserted > SIZE_MAX - (size - length) - 1)
        return document_error(result, "edit exceeds source size limit");
    size_t updated_size = size - length + inserted;
    if (updated_size > (size_t)(INT32_MAX - 1) / 2)
        return document_error(result, "edit exceeds parser size limit");
    if (length == inserted && !memcmp(previous->code + offset, replacement, inserted))
        return 0;
    char *source = malloc(updated_size + 1);
    if (!source)
        return document_error(result, "out of memory editing source");
    memcpy(source, previous->code, offset);
    memcpy(source + offset, replacement, inserted);
    memcpy(
        source + offset + inserted,
        previous->code + offset + length,
        size - offset - length + 1
    );
    ecs_script_t *parsed = ecs_script_parse(previous->name, source, NULL, result);
    free(source);
    if (!parsed)
        return -1;
    if (!document_reserve(document)) {
        ecs_script_free(parsed);
        return document_error(result, "out of memory recording revision");
    }
    /* A successful edit branches history; failures leave redo available. */
    for (size_t i = document->current + 1; i < document->count; i++)
        ecs_script_free(document->revisions[i]);
    document->current++;
    document->count = document->current + 1;
    document->revisions[document->current] = parsed;
    return 0;
}

bool ecs_script_document_undo(ecs_script_document_t *document) {
    if (!document || !document->current)
        return false;
    document->current--;
    return true;
}

bool ecs_script_document_redo(ecs_script_document_t *document) {
    if (!document || document->current + 1 >= document->count)
        return false;
    document->current++;
    return true;
}

int ecs_script_document_save(
    const ecs_script_document_t *document,
    const char *filename,
    ecs_script_eval_result_t *result
) {
    if (result)
        *result = (ecs_script_eval_result_t){ 0 };
    if (!document || !filename || !filename[0])
        return document_error(result, "document and filename must not be empty");
    size_t size = strlen(filename);
    if (size > SIZE_MAX - 32)
        return document_error(result, "filename exceeds size limit");
    char *temporary = malloc(size + 32);
    if (!temporary)
        return document_error(result, "out of memory saving source");
    FILE *file = NULL;
    /* Exclusive creation prevents clobbering another writer's temporary file. */
    for (unsigned i = 0; i < 1024; i++) {
        snprintf(temporary, size + 32, "%s.si-tmp-%u", filename, i);
        file = fopen(temporary, "wbx");
        if (file || errno != EEXIST)
            break;
    }
    if (!file) {
        free(temporary);
        return document_error(result, "could not create source temporary file");
    }
    const char *source = ecs_script_document_script(document)->code;
    size_t length = strlen(source);
    bool failed = fwrite(source, 1, length, file) != length;
    if (fflush(file))
        failed = true;
#ifndef _WIN32
    if (!failed && fsync(fileno(file)))
        failed = true;
#endif
    if (fclose(file))
        failed = true;
    if (!failed) {
#ifdef _WIN32
        failed =
            !MoveFileExA(temporary, filename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
        failed = rename(temporary, filename) != 0;
#endif
    }
    if (failed)
        remove(temporary);
    free(temporary);
    return failed ? document_error(result, "could not commit source file") : 0;
}
