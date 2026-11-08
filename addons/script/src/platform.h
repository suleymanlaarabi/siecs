#ifndef SIECS_SCRIPT_PLATFORM_H
#if defined(_MSC_VER)
#define SCRIPT_THREAD_LOCAL __declspec(thread)
#else
#define SCRIPT_THREAD_LOCAL _Thread_local
#endif

#define SIECS_SCRIPT_PLATFORM_H

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <sicore.h>
#include <siecs_script.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ecs_assert(condition, ...) assert(condition)

typedef sicore_vec_t ecs_vec_t;

typedef struct script_arena_block_t script_arena_block_t;
typedef struct ecs_allocator_t {
    script_arena_block_t *blocks;
    script_arena_block_t *current;
    script_arena_block_t *last;
    sicore_vec_t vectors;
} ecs_allocator_t;

void *script_alloc(ecs_allocator_t *allocator, size_t size);
char *script_strdup(const char *str);
void *script_vec_append(ecs_allocator_t *allocator, ecs_vec_t *vec, size_t size);
void script_allocator_adopt(ecs_allocator_t *destination, ecs_allocator_t *source);
void script_allocator_retire_vector(ecs_allocator_t *allocator, sicore_vec_t vector);
void script_allocator_reset(ecs_allocator_t *allocator);
void script_allocator_fini(ecs_allocator_t *allocator);

/* AST allocations are reclaimed together, including abandoned parse branches. */
#define siecs_strdup(a, str) strcpy(script_alloc(a, strlen(str) + 1), str)
#define ecs_vec_first_t(vec, T) ((T *)(vec)->data)
#define ecs_vec_count(vec) ((int32_t)(vec)->size)

typedef struct ecs_strbuf_t {
    sicore_vec_t bytes;
} ecs_strbuf_t;
#define ECS_STRBUF_INIT { 0 }
void ecs_strbuf_appendstrn(ecs_strbuf_t *buf, const char *str, int32_t len);
void ecs_strbuf_appendstr(ecs_strbuf_t *buf, const char *str);
void ecs_strbuf_appendch(ecs_strbuf_t *buf, char ch);
char *ecs_strbuf_get(ecs_strbuf_t *buf);

double script_monotonic_time(void);
void siecs_log_capture_push(bool enabled);
char *siecs_log_capture_pop(void);
void siecs_log_get_captured_error_pos(int32_t *line, int32_t *column);
void ecs_parser_error(const char *name, const char *code, int64_t offset, const char *format, ...);
#endif
