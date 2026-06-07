#ifndef NDEBUG
#include "map.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ECS_MAP_LOAD_NUM 7
#define ECS_MAP_LOAD_DEN 10

static size_t ecs_next_pow2(size_t x) {
    size_t p = 16;
    while (p < x)
        p <<= 1;
    return p;
}

static inline uint64_t ecs_map_hash_cstr(const char *s) {
    uint64_t h = 1469598103934665603ull;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ull;
    }

    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdull;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ull;
    h ^= h >> 33;

    return h ? h : 1;
}

void ecs_map_init(ecs_map_t *m, size_t initial_capacity) {
    size_t cap = ecs_next_pow2(initial_capacity);
    m->slots = (ecs_map_slot_t *)calloc(cap, sizeof(ecs_map_slot_t));

    m->cap = cap;
    m->len = 0;
}

void ecs_map_fini(ecs_map_t *m) {
    if (!m)
        return;

    if (m->slots) {
        free(m->slots);
    }

    m->slots = NULL;
    m->cap = 0;
    m->len = 0;
}

static bool ecs_map_insert_raw(
    ecs_map_slot_t *slots,
    size_t cap,
    const char *key,
    uint64_t hash,
    uint32_t value
) {
    size_t mask = cap - 1;
    size_t i = (size_t)hash & mask;

    for (;;) {
        ecs_map_slot_t *s = &slots[i];

        if (!s->key) {
            s->key = key;
            s->hash = hash;
            s->value = value;
            return true;
        }

        if (s->hash == hash && (s->key == key || strcmp(s->key, key) == 0)) {
            s->value = value;
            return false;
        }

        i = (i + 1) & mask;
    }
}

static void ecs_map_grow(ecs_map_t *m) {
    size_t new_cap = m->cap ? m->cap * 2 : 16;
    ecs_map_slot_t *new_slots = (ecs_map_slot_t *)calloc(new_cap, sizeof(ecs_map_slot_t));

    for (size_t i = 0; i < m->cap; i++) {
        const ecs_map_slot_t *s = &m->slots[i];
        if (s->key) {
            ecs_map_insert_raw(new_slots, new_cap, s->key, s->hash, s->value);
        }
    }

    free(m->slots);
    m->slots = new_slots;
    m->cap = new_cap;
}

void ecs_map_set(ecs_map_t *m, const char *key, uint32_t value) {
    if (!m->slots) {
        ecs_map_init(m, 16);
    }

    if ((m->len + 1) * ECS_MAP_LOAD_DEN > m->cap * ECS_MAP_LOAD_NUM) {
        ecs_map_grow(m);
    }

    uint64_t hash = ecs_map_hash_cstr(key);
    bool inserted = ecs_map_insert_raw(m->slots, m->cap, key, hash, value);

    if (inserted) {
        m->len++;
    }
}

uint32_t ecs_map_get(const ecs_map_t *m, const char *key) {
    if (!m->slots)
        return UINT32_MAX;

    uint64_t hash = ecs_map_hash_cstr(key);
    size_t mask = m->cap - 1;
    size_t i = (size_t)hash & mask;

    for (;;) {
        const ecs_map_slot_t *s = &m->slots[i];

        if (!s->key)
            return UINT32_MAX;

        if (s->hash == hash && (s->key == key || strcmp(s->key, key) == 0)) {
            return s->value;
        }

        i = (i + 1) & mask;
    }
}

bool ecs_map_has(const ecs_map_t *m, const char *key) { return ecs_map_get(m, key) != UINT32_MAX; }
#endif
