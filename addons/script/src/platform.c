#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "platform.h"

struct script_arena_block_t {
    script_arena_block_t *next;
    size_t used;
    size_t capacity;
    max_align_t data[];
};

void *script_alloc(ecs_allocator_t *allocator, size_t size) {
    const size_t alignment = _Alignof(max_align_t);
    size = (size + alignment - 1) & ~(alignment - 1);
    script_arena_block_t *block = allocator->current;
    while (block && size > block->capacity - block->used)
        block = block->next;
    if (!block) {
        size_t capacity = size > 16384 ? size : 16384;
        block = malloc(sizeof(*block) + capacity);
        if (!block)
            abort();
        block->next = NULL;
        block->used = 0;
        block->capacity = capacity;
        if (allocator->last)
            allocator->last->next = block;
        else
            allocator->blocks = block;
        allocator->last = block;
    }
    allocator->current = block;
    void *result = (char *)block->data + block->used;
    block->used += size;
    memset(result, 0, size);
    return result;
}

char *script_strdup(const char *str) {
    if (!str)
        return NULL;
    size_t size = strlen(str) + 1;
    char *result = malloc(size);
    if (!result)
        abort();
    memcpy(result, str, size);
    return result;
}

void *script_vec_append(ecs_allocator_t *allocator, ecs_vec_t *vec, size_t size) {
    assert(allocator && size <= UINT32_MAX);
    if (!vec->data) {
        if (!allocator->vectors.data) {
            sicore_vec_init(&allocator->vectors, sizeof(ecs_vec_t *));
        }
        sicore_vec_init(vec, (uint32_t)size);
        sicore_vec_push(&allocator->vectors, &vec, sizeof(vec));
    }
    return sicore_vec_push_empty(vec, (uint32_t)size);
}

void script_allocator_retire_vector(ecs_allocator_t *allocator, sicore_vec_t vector) {
    sicore_vec_t *saved = script_alloc(allocator, sizeof(*saved));
    *saved = vector;
    if (!allocator->vectors.data)
        sicore_vec_init(&allocator->vectors, sizeof(sicore_vec_t *));
    sicore_vec_push(&allocator->vectors, &saved, sizeof saved);
}

void script_allocator_adopt(ecs_allocator_t *destination, ecs_allocator_t *source) {
    if (source->blocks) {
        if (destination->last)
            destination->last->next = source->blocks;
        else
            destination->blocks = source->blocks;
        destination->last = source->last;
        destination->current = source->current;
    }
    sicore_vec_t **vectors = source->vectors.data;
    if (source->vectors.size && !destination->vectors.data)
        sicore_vec_init(&destination->vectors, sizeof(sicore_vec_t *));
    for (uint32_t i = 0; i < source->vectors.size; i++)
        sicore_vec_push(&destination->vectors, &vectors[i], sizeof(*vectors));
    sicore_vec_fini(&source->vectors);
    *source = (ecs_allocator_t){ 0 };
}

void script_allocator_reset(ecs_allocator_t *allocator) {
    sicore_vec_t **vectors = allocator->vectors.data;
    for (uint32_t i = 0; i < allocator->vectors.size; i++)
        sicore_vec_fini(vectors[i]);
    allocator->vectors.size = 0;
    for (script_arena_block_t *block = allocator->blocks; block; block = block->next)
        block->used = 0;
    allocator->current = allocator->blocks;
}

void script_allocator_fini(ecs_allocator_t *allocator) {
    ecs_vec_t **vectors = allocator->vectors.data;
    for (uint32_t i = 0; i < allocator->vectors.size; i++) {
        sicore_vec_fini(vectors[i]);
    }
    sicore_vec_fini(&allocator->vectors);
    script_arena_block_t *block = allocator->blocks;
    while (block) {
        script_arena_block_t *next = block->next;
        free(block);
        block = next;
    }
}

void ecs_strbuf_appendstrn(ecs_strbuf_t *buf, const char *str, int32_t len) {
    assert(len >= 0 && (uint64_t)buf->bytes.size + (uint32_t)len < UINT32_MAX);
    if (!buf->bytes.data)
        sicore_vec_init(&buf->bytes, 1);
    uint32_t offset = buf->bytes.size;
    sicore_vec_ensure(&buf->bytes, offset + (uint32_t)len, 1);
    memcpy((char *)buf->bytes.data + offset, str, (size_t)len);
}

void ecs_strbuf_appendstr(ecs_strbuf_t *buf, const char *str) {
    ecs_strbuf_appendstrn(buf, str, (int32_t)strlen(str));
}

void ecs_strbuf_appendch(ecs_strbuf_t *buf, char ch) { ecs_strbuf_appendstrn(buf, &ch, 1); }

char *ecs_strbuf_get(ecs_strbuf_t *buf) {
    ecs_strbuf_appendch(buf, '\0');
    char *result = buf->bytes.data;
    buf->bytes = (sicore_vec_t){ 0 };
    return result;
}

typedef struct script_error_capture_t {
    char *message;
    int32_t line;
    int32_t column;
} script_error_capture_t;

static SCRIPT_THREAD_LOCAL script_error_capture_t captures[128];
static SCRIPT_THREAD_LOCAL uint32_t capture_depth;
static SCRIPT_THREAD_LOCAL int32_t captured_line;
static SCRIPT_THREAD_LOCAL int32_t captured_column;

void siecs_log_capture_push(bool enabled) {
    (void)enabled;
    assert(capture_depth < sizeof(captures) / sizeof(captures[0]));
    captures[capture_depth++] = (script_error_capture_t){ 0 };
}

char *siecs_log_capture_pop(void) {
    assert(capture_depth);
    script_error_capture_t *capture = &captures[--capture_depth];
    captured_line = capture->line;
    captured_column = capture->column;
    return capture->message;
}

void siecs_log_get_captured_error_pos(int32_t *line, int32_t *column) {
    *line = captured_line;
    *column = captured_column;
}

static void script_error_v(
    const char *name,
    const char *code,
    int64_t offset,
    const char *format,
    va_list args
) {
    int32_t line = 0, column = 0;
    if (code) {
        line = column = 1;
        for (int64_t i = 0; i < offset && code[i]; i++) {
            if (code[i] == '\n') {
                line++;
                column = 1;
            } else
                column++;
        }
    }
    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0)
        return;
    char *message = malloc((size_t)length + 1);
    if (!message)
        abort();
    vsnprintf(message, (size_t)length + 1, format, args);
    if (capture_depth) {
        script_error_capture_t *capture = &captures[capture_depth - 1];
        if (!capture->message) {
            capture->message = message;
            capture->line = line;
            capture->column = column;
            return;
        }
    } else {
        fprintf(stderr, "%s:%d:%d: %s\n", name ? name : "<script>", line, column, message);
    }
    free(message);
}

void ecs_parser_error(const char *name, const char *code, int64_t offset, const char *format, ...) {
    va_list args;
    va_start(args, format);
    script_error_v(name, code, offset, format, args);
    va_end(args);
}

void ecs_parser_warning(
    const char *name,
    const char *code,
    int64_t offset,
    const char *format,
    ...
) {
    (void)name;
    (void)code;
    (void)offset;
    (void)format;
}

void ecs_err(const char *format, ...) {
    va_list args;
    va_start(args, format);
    script_error_v(NULL, NULL, 0, format, args);
    va_end(args);
}

#ifdef _WIN32
#include <windows.h>
double script_monotonic_time(void) {
    LARGE_INTEGER frequency, now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)frequency.QuadPart;
}
#else
#include <time.h>
double script_monotonic_time(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}
#endif
