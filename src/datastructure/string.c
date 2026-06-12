#include "string.h"
#include "../utils.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void ecs_str_init(ecs_str_t *str) {
    str->data = NULL;
    str->len = 0;
    str->capacity = 0;
}

void ecs_str_fini(ecs_str_t *str) {
    if (str->data) {
        free(str->data);
    }
    ecs_str_init(str);
}

ecs_str_t ecs_str_new() {
    ecs_str_t str;
    ecs_str_init(&str);
    return str;
}

ecs_str_t ecs_str_with_capacity(uint32_t capacity) {
    ecs_str_t str;
    str.len = 0;
    str.capacity = capacity;
    if (capacity > 0) {
        str.data = malloc(capacity + 1);
        str.data[0] = '\0';
    } else {
        str.data = NULL;
    }
    return str;
}

void ecs_str_reserve(ecs_str_t *str, uint32_t capacity) {
    if (capacity > str->capacity) {
        str->data = realloc(str->data, capacity + 1);
        str->capacity = capacity;
        if (str->len == 0 && str->data) {
            str->data[0] = '\0';
        }
    }
}

void ecs_str_resize(ecs_str_t *str, uint32_t len) {
    ecs_str_reserve(str, len);
    if (str->data) {
        str->data[len] = '\0';
    }
    str->len = len;
}

ecs_str_t ecs_str_from_cstr(const char *cstr) {
    if (!cstr)
        return ecs_str_new();
    uint32_t len = (uint32_t)strlen(cstr);
    ecs_str_t str = ecs_str_with_capacity(len);
    if (len > 0) {
        memcpy(str.data, cstr, len + 1);
        str.len = len;
    }
    return str;
}

ecs_str_t ecs_str_clone(const ecs_str_t *str) {
    if (!str || !str->data)
        return ecs_str_new();
    ecs_str_t new_str = ecs_str_with_capacity(str->len);
    if (str->len > 0) {
        memcpy(new_str.data, str->data, str->len + 1);
        new_str.len = str->len;
    }
    return new_str;
}

const char *ecs_str_cstr(const ecs_str_t *str) { return str->data ? str->data : ""; }

char ecs_str_at(const ecs_str_t *str, uint32_t index) {
    ecs_assert(index < str->len, "index out of bounds: %d (len: %d)", index, str->len);
    return str->data[index];
}

void ecs_str_char_append(ecs_str_t *dst, char src) {
    if (dst->len + 1 > dst->capacity) {
        uint32_t new_cap = dst->capacity == 0 ? 8 : dst->capacity * 2;
        ecs_str_reserve(dst, new_cap);
    }
    dst->data[dst->len++] = src;
    dst->data[dst->len] = '\0';
}

void ecs_str_str_append(ecs_str_t *dst, const ecs_str_t *src) {
    if (!src || src->len == 0)
        return;
    uint32_t required = dst->len + src->len;
    if (required > dst->capacity) {
        ecs_str_reserve(dst, required);
    }
    memcpy(dst->data + dst->len, src->data, src->len);
    dst->len = required;
    dst->data[dst->len] = '\0';
}

void ecs_str_cstr_append(ecs_str_t *dst, const char *src) {
    if (!src || *src == '\0')
        return;
    uint32_t required = dst->len + strlen(src);
    if (required > dst->capacity) {
        ecs_str_reserve(dst, required);
    }
    memcpy(dst->data + dst->len, src, strlen(src));
    dst->len = required;
    dst->data[dst->len] = '\0';
}

void ecs_str_insert(ecs_str_t *str, uint32_t pos, char c) {
    ecs_assert(pos <= str->len, "pos out of bounds: %d (len: %d)", pos, str->len);
    if (str->len + 1 > str->capacity) {
        uint32_t new_cap = str->capacity == 0 ? 8 : str->capacity * 2;
        ecs_str_reserve(str, new_cap);
    }
    memmove(str->data + pos + 1, str->data + pos, str->len - pos + 1);
    str->data[pos] = c;
    str->len++;
}

void ecs_str_remove(ecs_str_t *str, uint32_t pos) {
    ecs_assert(pos < str->len, "pos out of bounds: %d (len: %d)", pos, str->len);
    memmove(str->data + pos, str->data + pos + 1, str->len - pos);
    str->len--;
}

void ecs_str_pop_back(ecs_str_t *str) {
    if (str->len > 0) {
        str->len--;
        str->data[str->len] = '\0';
    }
}

void ecs_str_trim(ecs_str_t *str) {
    if (str->len == 0)
        return;
    uint32_t start = 0;
    while (start < str->len && isspace((unsigned char)str->data[start])) {
        start++;
    }
    if (start == str->len) {
        str->len = 0;
        str->data[0] = '\0';
        return;
    }
    uint32_t end = str->len - 1;
    while (end > start && isspace((unsigned char)str->data[end])) {
        end--;
    }
    uint32_t new_len = end - start + 1;
    if (start > 0) {
        memmove(str->data, str->data + start, new_len);
    }
    str->len = new_len;
    str->data[str->len] = '\0';
}

bool ecs_str_starts_with(const ecs_str_t *str, const ecs_str_t *prefix) {
    if (prefix->len > str->len)
        return false;
    return memcmp(str->data, prefix->data, prefix->len) == 0;
}

bool ecs_str_ends_with(const ecs_str_t *str, const ecs_str_t *suffix) {
    if (suffix->len > str->len)
        return false;
    return memcmp(str->data + (str->len - suffix->len), suffix->data, suffix->len) == 0;
}

bool ecs_str_cmp(const ecs_str_t *a, const ecs_str_t *b) {
    if (a->len != b->len)
        return false;
    if (a->len == 0)
        return true;
    return memcmp(a->data, b->data, a->len) == 0;
}
