#ifndef ECS_STRING_H
#define ECS_STRING_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char *data; // null terminated string
    uint32_t len;
    uint32_t capacity;
} ecs_str_t;

void ecs_str_init(ecs_str_t *str);
void ecs_str_fini(ecs_str_t *str);
ecs_str_t ecs_str_new();
ecs_str_t ecs_str_with_capacity(uint32_t capacity);

void ecs_str_reserve(ecs_str_t *str, uint32_t capacity);
void ecs_str_resize(ecs_str_t *str, uint32_t len);
ecs_str_t ecs_str_from_cstr(const char *cstr);
ecs_str_t ecs_str_clone(const ecs_str_t *str);

const char *ecs_str_cstr(const ecs_str_t *str);
char ecs_str_at(const ecs_str_t *str, uint32_t index);


void ecs_str_char_append(ecs_str_t *dst, char src);
void ecs_str_str_append(ecs_str_t *dst, const ecs_str_t *src);
void ecs_str_insert(ecs_str_t *str, uint32_t pos, char c);
void ecs_str_remove(ecs_str_t *str, uint32_t pos);
void ecs_str_pop_back(ecs_str_t *str);

void ecs_str_trim(ecs_str_t *str);

bool ecs_str_starts_with(const ecs_str_t *str, const ecs_str_t *prefix);
bool ecs_str_ends_with(const ecs_str_t *str, const ecs_str_t *suffix);
bool ecs_str_cmp(const ecs_str_t *a, const ecs_str_t *b);

#endif
