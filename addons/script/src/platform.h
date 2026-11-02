#ifndef SIECS_SCRIPT_PLATFORM_H
#define SIECS_SCRIPT_PLATFORM_H

#include <siecs_script.h>
#include <sicore.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Local platform vocabulary keeps the ported grammar close to upstream.
 * No Flecs ECS headers, objects, or runtime library are used. */
#define SIECS_SCRIPT
#define SIECS_PARSER
#define ECS_SIZEOF(T) ((int32_t)sizeof(T))
#define ECS_CONST_CAST(T, value) ((T)(value))
#define ECS_AUTO_OVERRIDE (UINT64_C(1) << 63)
#define ecs_assert(condition, ...) assert(condition)
#define ecs_check(condition, ...) do { if (!(condition)) goto error; } while (0)
#define siecs_ito(T, value) ((T)(value))
#define siecs_uto(T, value) ((T)(value))
#define ecs_os_zeromem(ptr) memset(ptr, 0, sizeof(*(ptr)))
#define ecs_os_strlen(str) ((int32_t)strlen(str))
#define ecs_os_strcmp strcmp
#define ecs_os_strncmp strncmp
#define ecs_os_memcpy memcpy
#define ecs_os_memmove memmove
#define ecs_os_free free
#define ecs_os_malloc_n(T, count) ((T*)malloc(sizeof(T) * (size_t)(count)))
#define ecs_os_strdup script_strdup

typedef int32_t ecs_size_t;
typedef uint64_t ecs_id_t;
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
void script_allocator_reset(ecs_allocator_t *allocator);
void script_allocator_fini(ecs_allocator_t *allocator);

#define siecs_alloc_w_dbg_info(a, size, ...) script_alloc(a, (size_t)(size))
#define siecs_calloc_w_dbg_info(a, size, ...) script_alloc(a, (size_t)(size))
#define siecs_calloc_t(a, T) ((T*)script_alloc(a, sizeof(T)))
/* AST allocations are reclaimed together, including abandoned parse branches. */
#define siecs_free_t(a, T, ptr) ((void)(a), (void)(ptr))
#define siecs_strdup(a, str) strcpy(script_alloc(a, strlen(str) + 1), str)
#define ecs_vec_init_t(a, vec, T, count) (*(vec) = (ecs_vec_t){0})
#define ecs_vec_append_t(a, vec, T) ((T*)script_vec_append(a, vec, sizeof(T)))
#define ecs_vec_first(vec) ((vec)->data)
#define ecs_vec_first_t(vec, T) ((T*)(vec)->data)
#define ecs_vec_last_t(vec, T) (&((T*)(vec)->data)[(vec)->size - 1])
#define ecs_vec_count(vec) ((int32_t)(vec)->size)
#define ecs_vec_remove_last(vec) sicore_vec_remove_last(vec)

typedef struct ecs_strbuf_t { sicore_vec_t bytes; } ecs_strbuf_t;
#define ECS_STRBUF_INIT {0}
void ecs_strbuf_appendstrn(ecs_strbuf_t *buf, const char *str, int32_t len);
void ecs_strbuf_appendstr(ecs_strbuf_t *buf, const char *str);
void ecs_strbuf_appendch(ecs_strbuf_t *buf, char ch);
#define ecs_strbuf_appendlit ecs_strbuf_appendstr
#define ecs_strbuf_written(buf) ((int32_t)(buf)->bytes.size)
char *ecs_strbuf_get(ecs_strbuf_t *buf);

void siecs_log_capture_push(bool enabled);
char *siecs_log_capture_pop(void);
void siecs_log_get_captured_error_pos(int32_t *line, int32_t *column);
void ecs_parser_error(const char *name, const char *code, int64_t offset,
    const char *format, ...);
void ecs_parser_warning(const char *name, const char *code, int64_t offset,
    const char *format, ...);
void ecs_err(const char *format, ...);
const char *siecs_chrparse(const char *in, char *out);
char *siecs_chresc(char *out, char in, char delimiter);
ecs_size_t siecs_stresc(char *out, ecs_size_t n, char delimiter, const char *in);
char *siecs_astresc(char delimiter, const char *in);
const char *siecs_parse_digit(const char *ptr, char *token, int32_t size);
const char *siecs_parse_ws_eol(const char *ptr);

#endif
