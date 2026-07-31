#include "siecs_no_addons.h"

#if SICORE_VEC

#if SICORE_HAS_MAP
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#include <emmintrin.h>
#define SICORE_MAP_SSE2 1
#elif defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#define SICORE_MAP_NEON 1
#endif

#define SICORE_GROUP_WIDTH 16u
#define SICORE_INITIAL_CAPACITY 16u
#define SICORE_CTRL_EMPTY UINT8_C(0x80)
#define SICORE_CTRL_DELETED UINT8_C(0xfe)

/* 16 octets sur ABI 64 bits: 1/4 de ligne de cache de 64 octets. */
typedef struct {
    const char *key;
    uint32_t value;
    uint32_t key_length;
} sicore_map_entry_t;

/*
 * Hash de chaîne basé sur wyhash final v4 (domaine public / Unlicense), adapté
 * et préfixé pour rester entièrement interne à cette unité de compilation.
 */
static const uint64_t sicore_hash_secret[5] = { UINT64_C(0xa0761d6478bd642f),
                                                UINT64_C(0xe7037ed1a0b428db),
                                                UINT64_C(0x8ebc6af09c88c6e3),
                                                UINT64_C(0x589965cc75374cc3),
                                                UINT64_C(0x1d8e4e27c47d124f) };

static inline void sicore_mul128(uint64_t *a, uint64_t *b) {
#if defined(__SIZEOF_INT128__)
    __uint128_t r = (__uint128_t)(*a) * (*b);
    *a = (uint64_t)r;
    *b = (uint64_t)(r >> 64);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
    *a = _umul128(*a, *b, b);
#else
    const uint64_t ah = *a >> 32;
    const uint64_t al = (uint32_t)*a;
    const uint64_t bh = *b >> 32;
    const uint64_t bl = (uint32_t)*b;
    const uint64_t rh = ah * bh;
    const uint64_t rm0 = ah * bl;
    const uint64_t rm1 = bh * al;
    const uint64_t rl = al * bl;
    const uint64_t t = rl + (rm0 << 32);
    uint64_t carry = t < rl;
    const uint64_t lo = t + (rm1 << 32);
    carry += lo < t;
    *a = lo;
    *b = rh + (rm0 >> 32) + (rm1 >> 32) + carry;
#endif
}

static inline uint64_t sicore_mix(uint64_t a, uint64_t b) {
    sicore_mul128(&a, &b);
    return a ^ b;
}

static inline uint64_t sicore_read64(const uint8_t *p) {
    uint64_t v;
    memcpy(&v, p, sizeof(v));
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    v = __builtin_bswap64(v);
#endif
    return v;
}

static inline uint64_t sicore_read32(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    v = __builtin_bswap32(v);
#endif
    return v;
}

static inline uint64_t sicore_read3(const uint8_t *p, size_t len) {
    return ((uint64_t)p[0] << 16) | ((uint64_t)p[len >> 1] << 8) | (uint64_t)p[len - 1];
}

static inline uint64_t
sicore_hash_finish16(const uint8_t *p, uint64_t len, uint64_t seed, size_t remaining) {
    uint64_t a;
    uint64_t b;

    if (remaining <= 8) {
        if (remaining >= 4) {
            a = sicore_read32(p);
            b = sicore_read32(p + remaining - 4);
        } else if (remaining != 0) {
            a = sicore_read3(p, remaining);
            b = 0;
        } else {
            a = 0;
            b = 0;
        }
    } else {
        a = sicore_read64(p);
        b = sicore_read64(p + remaining - 8);
    }

    return sicore_mix(sicore_hash_secret[1] ^ len, sicore_mix(a ^ sicore_hash_secret[1], b ^ seed));
}

static inline uint64_t sicore_hash_bytes(const uint8_t *p, size_t len) {
    size_t remaining = len;
    uint64_t seed = sicore_hash_secret[0];

    if (SICORE_UNLIKELY(remaining > 64)) {
        uint64_t seed2 = seed;
        do {
            seed =
                sicore_mix(sicore_read64(p) ^ sicore_hash_secret[1], sicore_read64(p + 8) ^ seed) ^
                sicore_mix(
                    sicore_read64(p + 16) ^ sicore_hash_secret[2],
                    sicore_read64(p + 24) ^ seed
                );
            seed2 = sicore_mix(
                        sicore_read64(p + 32) ^ sicore_hash_secret[3],
                        sicore_read64(p + 40) ^ seed2
                    ) ^
                    sicore_mix(
                        sicore_read64(p + 48) ^ sicore_hash_secret[4],
                        sicore_read64(p + 56) ^ seed2
                    );
            p += 64;
            remaining -= 64;
        } while (remaining > 64);
        seed ^= seed2;
    }

    while (remaining > 16) {
        seed = sicore_mix(sicore_read64(p) ^ sicore_hash_secret[1], sicore_read64(p + 8) ^ seed);
        p += 16;
        remaining -= 16;
    }

    return sicore_hash_finish16(p, (uint64_t)len, seed, remaining);
}

static inline uint64_t sicore_hash_string(const char *key, uint32_t *length) {
    const uint32_t len = (uint32_t)strlen(key);
    *length = len;
    return sicore_hash_bytes((const uint8_t *)key, len);
}

static inline uint32_t sicore_ctz32(uint32_t x) {
#if defined(_MSC_VER)
    unsigned long bit;
    _BitScanForward(&bit, x);
    return (uint32_t)bit;
#else
    return (uint32_t)__builtin_ctz(x);
#endif
}

#if defined(SICORE_MAP_SSE2)
static inline uint32_t sicore_match_byte(const uint8_t *ctrl, uint8_t byte) {
    const __m128i group = _mm_loadu_si128((const __m128i *)(const void *)ctrl);
    const __m128i wanted = _mm_set1_epi8((char)byte);
    return (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(group, wanted));
}
#elif defined(SICORE_MAP_NEON)
static inline uint32_t sicore_match_byte(const uint8_t *ctrl, uint8_t byte) {
    static const uint8_t weights_data[16] = { 1, 2, 4, 8, 16, 32, 64, 128,
                                              1, 2, 4, 8, 16, 32, 64, 128 };
    const uint8x16_t group = vld1q_u8(ctrl);
    const uint8x16_t equal = vceqq_u8(group, vdupq_n_u8(byte));
    const uint8x16_t bits = vandq_u8(equal, vld1q_u8(weights_data));
    const uint32_t low = vaddv_u8(vget_low_u8(bits));
    const uint32_t high = vaddv_u8(vget_high_u8(bits));
    return low | (high << 8);
}
#else
static inline uint32_t sicore_match_byte(const uint8_t *ctrl, uint8_t byte) {
    uint32_t mask = 0;
    for (uint32_t i = 0; i < SICORE_GROUP_WIDTH; ++i) {
        mask |= (uint32_t)(ctrl[i] == byte) << i;
    }
    return mask;
}
#endif

static inline uint32_t sicore_max_load(uint32_t capacity) {
    return capacity - (capacity >> 3); /* 87,5 % */
}

static inline uint8_t sicore_hash_h2(uint64_t hash) { return (uint8_t)(hash & UINT64_C(0x7f)); }

static inline uint32_t sicore_hash_group(uint64_t hash, uint32_t group_mask) {
    return (uint32_t)(hash >> 7) & group_mask;
}

static inline void sicore_allocate(sicore_map_t *map, uint32_t capacity) {
    const size_t ctrl_bytes = capacity;
    const size_t entries_bytes = (size_t)capacity * sizeof(sicore_map_entry_t);
    uint8_t *const block = (uint8_t *)malloc(ctrl_bytes + entries_bytes);

    memset(block, SICORE_CTRL_EMPTY, ctrl_bytes);

    map->ctrl = block;
    map->entries = block + ctrl_bytes;
    map->size = 0;
    map->capacity = capacity;
    map->growth_left = sicore_max_load(capacity);
    map->group_mask = (capacity / SICORE_GROUP_WIDTH) - 1u;
}

static inline void sicore_insert_absent_hashed(
    sicore_map_t *map,
    const char *key,
    uint32_t value,
    uint32_t key_length,
    uint64_t hash
) {
    sicore_map_entry_t *const entries = (sicore_map_entry_t *)map->entries;
    const uint8_t h2 = sicore_hash_h2(hash);
    uint32_t group = sicore_hash_group(hash, map->group_mask);
    uint32_t probe = 0;

    for (;;) {
        const uint32_t base = group * SICORE_GROUP_WIDTH;
        const uint32_t empties = sicore_match_byte(map->ctrl + base, SICORE_CTRL_EMPTY);

        if (empties != 0) {
            const uint32_t index = base + sicore_ctz32(empties);
            entries[index].key = key;
            entries[index].value = value;
            entries[index].key_length = key_length;
            map->ctrl[index] = h2;
            ++map->size;
            --map->growth_left;
            return;
        }

        ++probe;
        group = (group + probe) & map->group_mask;
    }
}

static inline uint32_t
sicore_find_index(const sicore_map_t *map, const char *key, uint32_t key_length, uint64_t hash) {
    const sicore_map_entry_t *const entries = (const sicore_map_entry_t *)map->entries;
    const uint8_t h2 = sicore_hash_h2(hash);
    uint32_t group = sicore_hash_group(hash, map->group_mask);
    uint32_t probe = 0;

    for (;;) {
        const uint32_t base = group * SICORE_GROUP_WIDTH;
        uint32_t candidates = sicore_match_byte(map->ctrl + base, h2);

        while (candidates != 0) {
            const uint32_t bit = sicore_ctz32(candidates);
            const uint32_t index = base + bit;
            const char *const candidate_key = entries[index].key;

            if (candidate_key == key || (entries[index].key_length == key_length &&
                                         memcmp(candidate_key, key, key_length) == 0)) {
                return index;
            }
            candidates &= candidates - 1u;
        }

        if (sicore_match_byte(map->ctrl + base, SICORE_CTRL_EMPTY) != 0) {
            return UINT32_MAX;
        }

        ++probe;
        group = (group + probe) & map->group_mask;
    }
}

void sicore_map_init(sicore_map_t *map) { sicore_allocate(map, SICORE_INITIAL_CAPACITY); }

void sicore_map_fini(sicore_map_t *map) { free(map->ctrl); }

SICORE_HOT uint32_t sicore_map_get(const sicore_map_t *map, const char *key) {
    uint32_t key_length;
    const uint64_t hash = sicore_hash_string(key, &key_length);
    const uint32_t index = sicore_find_index(map, key, key_length, hash);
    return index == UINT32_MAX ? UINT32_MAX
                               : ((const sicore_map_entry_t *)map->entries)[index].value;
}

SICORE_HOT bool sicore_map_has(const sicore_map_t *map, const char *key) {
    uint32_t key_length;
    const uint64_t hash = sicore_hash_string(key, &key_length);
    return sicore_find_index(map, key, key_length, hash) != UINT32_MAX;
}

static void sicore_rehash(sicore_map_t *map, uint32_t new_capacity) {
    sicore_map_t rebuilt;
    const uint32_t old_capacity = map->capacity;
    uint8_t *const old_ctrl = map->ctrl;
    sicore_map_entry_t *const old_entries = (sicore_map_entry_t *)map->entries;

    sicore_allocate(&rebuilt, new_capacity);

    for (uint32_t i = 0; i < old_capacity; ++i) {
        if (old_ctrl[i] < SICORE_CTRL_EMPTY) {
            const char *const key = old_entries[i].key;

            sicore_insert_absent_hashed(
                &rebuilt,
                key,
                old_entries[i].value,
                old_entries[i].key_length,
                sicore_hash_bytes((const uint8_t *)key, old_entries[i].key_length)
            );
        }
    }

    free(old_ctrl);
    *map = rebuilt;
}

SICORE_HOT void sicore_map_set(sicore_map_t *map, const char *key, uint32_t value) {
    sicore_map_entry_t *entries = (sicore_map_entry_t *)map->entries;

    uint32_t key_length;
    const uint64_t hash = sicore_hash_string(key, &key_length);
    const uint8_t h2 = sicore_hash_h2(hash);

    uint32_t group = sicore_hash_group(hash, map->group_mask);
    uint32_t probe = 0;
    uint32_t first_deleted = UINT32_MAX;

    for (;;) {
        const uint32_t base = group * SICORE_GROUP_WIDTH;
        uint32_t candidates = sicore_match_byte(map->ctrl + base, h2);

        while (candidates != 0) {
            const uint32_t bit = sicore_ctz32(candidates);
            const uint32_t index = base + bit;
            const char *const candidate_key = entries[index].key;

            if (candidate_key == key || (entries[index].key_length == key_length &&
                                         memcmp(candidate_key, key, key_length) == 0)) {
                entries[index].value = value;
                return;
            }

            candidates &= candidates - 1u;
        }

        if (first_deleted == UINT32_MAX) {
            const uint32_t deleted = sicore_match_byte(map->ctrl + base, SICORE_CTRL_DELETED);

            if (deleted != 0) {
                first_deleted = base + sicore_ctz32(deleted);
            }
        }

        const uint32_t empties = sicore_match_byte(map->ctrl + base, SICORE_CTRL_EMPTY);

        if (empties != 0) {
            if (first_deleted != UINT32_MAX) {
                entries[first_deleted].key = key;
                entries[first_deleted].value = value;
                entries[first_deleted].key_length = key_length;

                map->ctrl[first_deleted] = h2;
                ++map->size;
                return;
            }

            if (SICORE_UNLIKELY(map->growth_left == 0)) {
                const uint32_t max_load = sicore_max_load(map->capacity);

                sicore_rehash(map, map->size < max_load ? map->capacity : map->capacity << 1);

                sicore_insert_absent_hashed(map, key, value, key_length, hash);

                return;
            }

            const uint32_t index = base + sicore_ctz32(empties);

            entries[index].key = key;
            entries[index].value = value;
            entries[index].key_length = key_length;

            map->ctrl[index] = h2;
            ++map->size;
            --map->growth_left;
            return;
        }

        ++probe;
        group = (group + probe) & map->group_mask;
    }
}

SICORE_HOT bool sicore_map_unset(sicore_map_t *map, const char *key) {
    uint32_t key_length;
    const uint64_t hash = sicore_hash_string(key, &key_length);

    const uint32_t index = sicore_find_index(map, key, key_length, hash);

    if (index == UINT32_MAX) {
        return false;
    }

    const uint32_t base = index & ~(SICORE_GROUP_WIDTH - 1u);

    --map->size;

    if (sicore_match_byte(map->ctrl + base, SICORE_CTRL_EMPTY) != 0) {
        map->ctrl[index] = SICORE_CTRL_EMPTY;
        ++map->growth_left;
    } else {
        map->ctrl[index] = SICORE_CTRL_DELETED;
    }

    return true;
}

#endif

#if SICORE_HAS_VEC
#include <stdlib.h>
#include <string.h>

void sicore_vec_init(sicore_vec_t *vec, uint32_t element_size) {
    vec->data = malloc(element_size);
    vec->size = 0;
    vec->capacity = 1;
}

void sicore_vec_init_w_size(sicore_vec_t *vec, uint32_t element_size, uint32_t size) {
    vec->data = malloc(element_size * size);
    vec->size = 0;
    vec->capacity = size;
}

void sicore_vec_fini(sicore_vec_t *vec) { free(vec->data); }

void sicore_vec_grow(sicore_vec_t *vec, uint32_t element_size) {
    vec->capacity *= 2;
    vec->data = realloc(vec->data, element_size * vec->capacity);
}

void sicore_vec_push(sicore_vec_t *vec, const void *element, const uint32_t element_size) {
    if (SICORE_UNLIKELY(vec->size >= vec->capacity)) {
        sicore_vec_grow(vec, element_size);
    }
    memcpy((uint8_t *)vec->data + (vec->size * element_size), element, element_size);
    vec->size++;
}

void sicore_vec_ensure(sicore_vec_t *vec, uint32_t count, const uint32_t element_size) {
    if (count <= vec->size)
        return;
    while (vec->capacity < count)
        sicore_vec_grow(vec, element_size);
    memset((uint8_t *)vec->data + vec->size * element_size, 0, (count - vec->size) * element_size);
    vec->size = count;
}

void sicore_vec_remove_fast(sicore_vec_t *vec, uint32_t index, const uint32_t element_size) {
    if (index < vec->size - 1) {
        void *dst = (uint8_t *)vec->data + (index * element_size);
        const void *src = (uint8_t *)vec->data + ((vec->size - 1) * element_size);
        memcpy(dst, src, element_size);
    }
    vec->size--;
}

bool sicore_vec_contains_u16(const sicore_vec_t *vec, const uint16_t value) {
    sicore_vec_iter(vec, uint16_t, current, {
        if (*current == value) {
            return true;
        }
    });
    return false;
}

static inline void sicore_vec_remove_fast_u16(sicore_vec_t *vec, uint32_t index) {
    if (index < vec->size - 1) {
        uint16_t *data = vec->data;
        data[index] = data[vec->size - 1];
    }
    vec->size--;
}

void sicore_vec_remove_u16(sicore_vec_t *vec, const uint16_t value) {
    sicore_vec_iter(vec, uint16_t, current, {
        if (*current == value) {
            sicore_vec_remove_fast_u16(vec, i);
            return;
        }
    });
}

static inline void sicore_vec_remove_fast_u64(sicore_vec_t *vec, uint32_t index) {
    if (index < vec->size - 1) {
        uint64_t *data = vec->data;
        data[index] = data[vec->size - 1];
    }
    vec->size--;
}

void sicore_vec_remove_u64(sicore_vec_t *vec, uint64_t value) {
    sicore_vec_iter(vec, uint64_t, current, {
        if (*current == value) {
            sicore_vec_remove_fast_u64(vec, i);
            return;
        }
    });
}
#endif

#endif /* SICORE_VEC */
#ifndef ECS_ADDONS_H
#define ECS_ADDONS_H

#endif

#ifndef SIECS_HELPER_H
#define SIECS_HELPER_H

#define ECS_LIKELY(x) __builtin_expect(!!(x), 1)
#define ECS_UNLIKELY(x) __builtin_expect(!!(x), 0)

#define ecs_entity(index, generation) (((uint64_t)(index) << 32) | (generation & 0xffffffff))

#define ecs_first(id) ((uint32_t)((id) >> 32))
#define ecs_second(id) ((uint32_t)((id) & 0xffffffff))

#endif

#include <stdio.h>
#ifndef SIECS_STORAGE_TABLE_INDEX_H
#define SIECS_STORAGE_TABLE_INDEX_H
#ifndef SIECS_TABLE_H
#define SIECS_TABLE_H
#ifndef SIECS_DATASTRUCTURE_IDMAP_H
#define SIECS_DATASTRUCTURE_IDMAP_H
#include <stdint.h>

typedef struct {
    uint16_t *ids;
    uint16_t capacity;
} ecs_id_map_t;

void ecs_id_map_init(ecs_id_map_t *map);
void ecs_id_map_fini(ecs_id_map_t *map);

void ecs_id_map_ensure(ecs_id_map_t *map, uint16_t id);

static inline void ecs_id_map_set(ecs_id_map_t *map, uint16_t id, uint16_t value) {
    ecs_id_map_ensure(map, id);
    map->ids[id] = value;
}

static inline uint16_t ecs_id_map_at(const ecs_id_map_t *map, uint16_t id) { return map->ids[id]; }

static inline uint16_t ecs_id_map_at_or_invalid(const ecs_id_map_t *map, uint16_t id) {
    return map->capacity > id ? map->ids[id] : UINT16_MAX;
}

#endif

#ifndef SIECS_TYPE_H
#define SIECS_TYPE_H
#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t *ids;
    uint16_t count;
    // Table metadata stored in the alignment gap before base. It is not part of
    // type identity; transient types may leave it zero until table creation.
    uint16_t data_count;
    uint32_t hash;
    ecs_entity_t base;
} ecs_type_t;

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id);
ecs_type_t ecs_type_with_remove_at(const ecs_type_t *type, uint16_t index);
ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base);

uint64_t ecs_type_bloom(const ecs_type_t *type);

void ecs_type_fini(ecs_type_t *type);

static inline int ecs_type_equals(const ecs_type_t *a, const ecs_type_t *b) {
    if (a->base != b->base)
        return 0;
    if (a->count != b->count)
        return 0;
    if (a->count == 0)
        return 1;
    return memcmp(a->ids, b->ids, (size_t)a->count * sizeof(uint16_t)) == 0;
}

#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EcsColumnTrivialMove = 1 << 0,
    EcsColumnNoDtor = 1 << 1,
    EcsColumnZeroCtor = 1 << 2,
} ecs_column_flags_t;

typedef struct {
    void *data;
    uint32_t size;
    uint16_t remove_edge; // the table that has the component removed or UINT16_MAX if the edge is
                          // not set
    uint16_t flags;
} ecs_column_t;

typedef struct ecs_table_s {
    ecs_id_map_t add_edge; // maps component id to the table that has the component added or column
                           // index if the component is in the table
    uint32_t entity_capacity;
    uint32_t entity_count;
    ecs_entity_t *entities;
    ecs_column_t *cls;
    uint16_t *data_columns;
    ecs_type_t type;
    uint64_t bloom;
    sicore_vec_t observers_by_event; // sicore_vec_t per event id; each holds uint16_t observer ids.
} ecs_table_t;

void ecs_table_init(ecs_table_t *table, ecs_type_t type, uint16_t table_id);
void ecs_table_fini(ecs_table_t *table);
uint32_t ecs_table_add_entity(ecs_table_t *table, ecs_entity_t entity);
// if the entity is not the last one, the last entity will be moved to the removed entity's
// position, and the moved entity will be returned
ecs_entity_t ecs_table_remove_entity(ecs_table_t *table, uint32_t row, bool row_values_live);

void *ecs_table_get_component(ecs_table_t *table, ecs_component_t component_id, uint32_t row);

// Append an observer id to this table's dense list for the given event,
// growing the per-event slot array on demand.
void ecs_table_add_observer(ecs_table_t *table, uint16_t event, uint16_t observer_id);

static inline uint16_t
ecs_table_get_add_edge(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_id_map_at_or_invalid(&table->add_edge, component_id);
}

static inline void *
ecs_table_component_at_column(const ecs_table_t *table, uint16_t column_index, uint32_t row) {
    ecs_column_t *column = &table->cls[column_index];
    return column->size != 0 ? (uint8_t *)column->data + (column->size * row) : NULL;
}

static inline uint16_t
ecs_table_column_or_invalid(const ecs_table_t *table, ecs_component_t component_id) {
    uint16_t column_index = ecs_table_get_add_edge(table, component_id);
    if (column_index < table->type.count && table->type.ids[column_index] == component_id) {
        return column_index;
    }
    return UINT16_MAX;
}

bool ecs_table_has(const ecs_table_t *table, ecs_component_t component_id);
bool ecs_table_is_a(const ecs_table_t *table, ecs_entity_t base);

static inline uint16_t
ecs_table_get_column_index(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_id_map_at(&table->add_edge, component_id);
}

static inline bool ecs_table_has_owned(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_table_column_or_invalid(table, component_id) != UINT16_MAX;
}

void *ecs_table_field(const ecs_table_t *table, ecs_component_t component_id, bool *is_shared);

#endif

#include <stdint.h>

struct ecs_world_s;

typedef struct {
    uint16_t table_index; // UINT16_MAX for empty
    uint16_t hash;
} ecs_type_slot_t;

typedef struct {
    ecs_table_t *tables;
    ecs_type_slot_t *slots;
    uint16_t table_count;
    uint16_t table_capacity;
    uint8_t slot_shift; // slot_count = 1 << slot_shift
} ecs_table_index_t;

void ecs_table_index_init();
void ecs_table_index_fini();

#define ecs_table_index_at(index) (&ecs_world.table_index.tables[index])

uint16_t ecs_table_index_get_or_create(
    ecs_type_t type
);

#endif

#ifndef SIECS_WORLD_INTERNAL_H
#define SIECS_WORLD_INTERNAL_H
#ifndef SIECS_COMMAND_BUFFER_H
#define SIECS_COMMAND_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct ecs_world_s ecs_world_t;

typedef struct {
    ecs_component_t id;
    void *data;
} ecs_deferred_set_t;

typedef struct {
    ecs_entity_t entity;
    bool kill;
    bool has_base;
    ecs_entity_t base;
    sicore_vec_t add_ids;
    sicore_vec_t remove_ids;
    sicore_vec_t sets;
} ecs_entity_command_t;

typedef struct ecs_command_buffer_s {
    sicore_vec_t commands;
    uint32_t *entity_to_command;
    uint32_t entity_capacity;
} ecs_command_buffer_t;

void ecs_command_buffer_init();
void ecs_command_buffer_fini();

void ecs_command_buffer_add(ecs_entity_t entity, ecs_component_t id);
void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id);
void ecs_command_buffer_set(ecs_entity_t entity, ecs_component_t id, const void *data);
void ecs_command_buffer_move(ecs_entity_t entity, ecs_component_t id, void *data);
void ecs_command_buffer_kill(ecs_entity_t entity);
void ecs_command_buffer_set_base(ecs_entity_t entity, ecs_entity_t target);
void ecs_command_buffer_flush();

void ecs_add_cid_now(ecs_entity_t entity, ecs_component_t id);
void ecs_remove_cid_now(ecs_entity_t entity, ecs_component_t id);
void ecs_set_cid_now(ecs_entity_t entity, ecs_component_t id, const void *data);
void ecs_move_cid_now(ecs_entity_t entity, ecs_component_t id, void *data);
void ecs_kill_now(ecs_entity_t entity);
void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target);

#endif

#ifndef ECS_ARENA_H
#define ECS_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct ecs_arena_block_s {
    struct ecs_arena_block_s *next;
    uint32_t capacity;
    uint32_t cursor;
    max_align_t data[];
} ecs_arena_block_t;

typedef struct {
    ecs_arena_block_t *first;
    ecs_arena_block_t *current;
    ecs_arena_block_t *last;
} ecs_arena_t;

void ecs_arena_init();
void ecs_arena_fini();
void *ecs_arena_alloc_slow(ecs_arena_t *allocator, uint32_t size);

static inline void *ecs_arena_alloc(ecs_arena_t *allocator, uint32_t size) {
    ecs_arena_block_t *block = allocator->current;
    const uint32_t alignment = (uint32_t)_Alignof(max_align_t);
    const uint32_t cursor = (block->cursor + alignment - 1u) & ~(alignment - 1u);
    if (ECS_LIKELY(cursor <= block->capacity && size <= block->capacity - cursor)) {
        block->cursor = cursor + size;
        return (uint8_t *)block->data + cursor;
    }
    return ecs_arena_alloc_slow(allocator, size);
}

static inline void ecs_arena_reset(ecs_arena_t *allocator) {
    for (ecs_arena_block_t *block = allocator->first; block; block = block->next) {
        block->cursor = 0;
    }
    allocator->current = allocator->first;
}

#endif

#ifndef SIECS_STORAGE_COMPONENT_INDEX_H
#define SIECS_STORAGE_COMPONENT_INDEX_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    ecs_component_info_t *info;
    uint16_t *required;
    uint32_t required_count;
    uint32_t size;
    ecs_type_ops_t ops;
    ecs_component_on_set_t on_set;
    ecs_component_on_remove_t on_remove;
    ecs_component_on_add_t on_add;
    uint32_t relation_flags;
    sicore_vec_t tables; // uint16_t
} ecs_component_record_t;

typedef struct ecs_component_index_s {
    sicore_vec_t components; // ecs_component_record_t
} ecs_component_index_t;

void ecs_component_index_register(
    ecs_component_t id,
    uint64_t size,
    ecs_type_ops_t ops,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    uint32_t relation_flags
);

#define ecs_component_index_get(id)                                                                \
    sicore_vec_get(&ecs_world.component_index.components, id, ecs_component_record_t)
#define ecs_component_index_get_mut(id)                                                            \
    sicore_vec_get_mut(&ecs_world.component_index.components, id, ecs_component_record_t)

void ecs_component_index_init();
void ecs_component_index_fini();

void ecs_component_value_ctor(const ecs_component_record_t *record, void *dst, uint32_t count);
void ecs_component_value_dtor(const ecs_component_record_t *record, void *ptr, uint32_t count);
void ecs_component_value_copy_ctor(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
);
void ecs_component_value_copy(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
);
void ecs_component_value_move_ctor(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
);
void ecs_component_value_move(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
);

#endif

#ifndef SIECS_STORAGE_ENTITY_INDEX_H
#define SIECS_STORAGE_ENTITY_INDEX_H
#include <stdint.h>

typedef struct {
    uint16_t generation;
    uint16_t table_id;
    // Alive records store the row in their table. Dead records reuse this field
    // as the next entity id in the free list headed by first_available.
    uint32_t table_row;
} ecs_entity_record_t;

typedef struct {
    sicore_vec_t entities;    // ecs_entity_record_t
    uint32_t first_available; // UINT32_MAX when no dead entity can be reused
} ecs_entity_index_t;

#define ecs_entity_index_get_record(entity_id)                                                     \
    sicore_vec_get_mut(&ecs_world.entity_index.entities, entity_id, ecs_entity_record_t)

bool ecs_entity_index_is_alive(ecs_entity_t entity);

void ecs_entity_index_init();
void ecs_entity_index_fini();

#endif

#ifndef SIECS_STORAGE_MODULE_INDEX_H
#define SIECS_STORAGE_MODULE_INDEX_H

typedef struct {
    ecs_module_id_t *id;
    sicore_vec_t observers; // ecs_observer_id_t
    sicore_vec_t systems;   // ecs_system_id_t
    bool enabled;
} ecs_module_t;

typedef struct {
    sicore_vec_t modules; // ecs_module_t
} ecs_module_index_t;

void ecs_module_index_init();
void ecs_module_index_fini();

ecs_module_id_t ecs_module_index_create(
    ecs_module_id_t *id
);
ecs_module_t *ecs_module_index_get(ecs_module_id_t module);
const ecs_module_t *ecs_module_index_get_const(ecs_module_id_t module);
ecs_module_id_t ecs_module_index_find(const ecs_module_id_t *id);

#endif

#ifndef SIECS_STORAGE_OBSERVER_INDEX_H
#define SIECS_STORAGE_OBSERVER_INDEX_H
#ifndef SIECS_STORAGE_QUERY_INDEX_H
#define SIECS_STORAGE_QUERY_INDEX_H
#include <stdint.h>

typedef struct {
    uint64_t bloom;
    ecs_entity_t is_a;
    ecs_query_term_t *terms;
    uint16_t term_count;
    uint16_t field_count;
    uint16_t field_mask;
} ecs_query_t;

typedef struct ecs_query_cache_s {
    ecs_query_t query;
    sicore_vec_t table_ids; // uint16_t
    void **fields_ptr;
    uint32_t *field_kind_bits;
    uint16_t field_table_capacity;
    uint32_t active_index;
    uint16_t next_free;
    bool alive;
} ecs_query_cache_t;

typedef struct {
    sicore_vec_t queries;
    sicore_vec_t active_ids; // ecs_query_id_t
    uint16_t first_free;
} ecs_query_index_t;

void ecs_query_index_init();
void ecs_query_index_fini();
uint16_t ecs_query_index_create(const ecs_query_desc_t *desc);
void ecs_query_index_update_matches(ecs_query_cache_t *query_cache);
void ecs_query_index_add_table(const ecs_table_t *table, uint16_t table_id);
void ecs_query_index_refresh_table_fields(const ecs_table_t *table, uint16_t table_id);

// Reusable query helpers shared with the observer index.
void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query);
void ecs_query_index_destroy(ecs_query_t *query);

static inline bool ecs_query_term_requires_owned(ecs_query_term_t term) {
    return term.access == EcsOut || term.access == EcsInOut || term.access == EcsInOutOptional;
}

static inline bool ecs_query_match_table(const ecs_query_t *query, const ecs_table_t *table) {
    if (ECS_LIKELY((query->bloom & table->bloom) != query->bloom)) {
        return false;
    }

    if (query->is_a && !ecs_table_is_a(table, query->is_a)) {
        return false;
    }

    for (uint16_t i = 0; i < query->term_count; i++) {
        ecs_query_term_t term = query->terms[i];
        if (term.access == EcsInOptional || term.access == EcsInOutOptional) {
            continue;
        } else if (term.access == EcsNot) {
            if (ecs_table_has(table, term.id)) {
                return false;
            }
        } else if (ecs_query_term_requires_owned(term)) {
            if (ecs_table_column_or_invalid(table, term.id) == UINT16_MAX) {
                return false;
            }
        } else if (!ecs_table_has(table, term.id)) {
            return false;
        }
    }
    return true;
}

#endif

#include <stdint.h>

typedef struct {
    ecs_event_t event;
    ecs_query_t query;
    ecs_observer_callback_t callback;
    uintptr_t user_data;
    bool enabled;
} ecs_observer_t;

typedef struct {
    sicore_vec_t observers; // ecs_observer_t
    uint16_t event_count;   // next free event id; starts past the builtin events
} ecs_observer_index_t;

void ecs_observer_index_init();
void ecs_observer_index_fini();

uint16_t ecs_observer_index_create(const ecs_observer_desc_t *desc);

// Cache a freshly created observer onto every existing table it matches.
void ecs_observer_index_match_tables(
    ecs_table_t *tables,
    uint16_t table_count,
    uint16_t observer_id
);

// Cache every existing observer that matches a freshly created table.
void ecs_observer_index_add_table(ecs_table_t *table);

#endif

#ifndef SIECS_STORAGE_RESOURCE_INDEX_H
#define SIECS_STORAGE_RESOURCE_INDEX_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t size;
    ecs_type_ops_t ops;
    ecs_resource_hook_t on_set;
    ecs_resource_hook_t on_remove;
} ecs_resource_record_t;

typedef struct {
    ecs_resource_record_t *records;
    void **data;
    bool *registered;
    bool *present;
    uint64_t capacity;
    uint64_t count;
} ecs_resource_index_t;

void ecs_resource_index_init();
void ecs_resource_index_fini();

ecs_resource_t ecs_resource_index_register(
    ecs_resource_t id,
    const ecs_resource_desc_t *desc
);
bool ecs_resource_index_is_registered(ecs_resource_t id);
void ecs_resource_index_set(
    ecs_resource_t id,
    const void *data
);
void ecs_resource_index_move(
    ecs_resource_t id,
    void *data
);
void *ecs_resource_index_get(ecs_resource_t id);
bool ecs_resource_index_has(ecs_resource_t id);
void ecs_resource_index_remove(ecs_resource_t id);

#endif

#ifndef SIECS_STORAGE_SYSTEM_INDEX_H
#define SIECS_STORAGE_SYSTEM_INDEX_H
#include <stdint.h>

typedef struct {
    ecs_query_id_t qid;
    void (*callback)(ecs_iter_t *);
    uintptr_t user_data;
    void (*user_data_dtor)(uintptr_t user_data);
    ecs_phase_t phase;
    ecs_system_id_t after[ECS_SYSTEM_AFTER_CAPACITY];
    bool enabled;
} ecs_system_t;

typedef struct {
    sicore_vec_t systems;
    sicore_vec_t phase_order[EcsPhaseCount];
    bool plan_dirty;
} ecs_system_index_t;

void ecs_system_index_init();
void ecs_system_index_fini();

ecs_system_id_t ecs_system_index_create(const ecs_system_t *system);
ecs_system_t *ecs_system_index_get(ecs_system_id_t system);
void ecs_system_index_build_plan();

#endif

typedef struct ecs_world_s ecs_world_t;

struct ecs_world_s {
    ecs_entity_index_t entity_index;
    ecs_component_index_t component_index;
    ecs_table_index_t table_index;
    ecs_query_index_t query_index;
    ecs_observer_index_t observer_index;
    ecs_system_index_t system_index;
    ecs_module_index_t module_index;
    ecs_resource_index_t resource_index;
    ecs_module_id_t active_module;
    ecs_world_feat_desc_t features;
    ecs_arena_t arena_allocator;
    ecs_command_buffer_t commands;
    uint32_t defer_depth;
    bool flushing_commands;
    bool did_start;
    bool exit;
    double delta_time;
    double last_time;
};

extern ecs_world_t ecs_world;

typedef struct {
    ecs_entity_t target;
} RelationTarget;

typedef struct {
    sicore_vec_t entities;
} RelationSource;

#define ecs_get_record(entity)                                                                     \
    sicore_vec_get_mut(&ecs_world.entity_index.entities, ecs_first(entity), ecs_entity_record_t)
#define ecs_get_table(tid) ecs_table_index_at(tid)

static inline void
ecs_emit(ecs_table_t *table, ecs_entity_t entity, ecs_event_t event, const void *trigger_data) {
    if (table->observers_by_event.size <= event) {
        return;
    }
    const sicore_vec_t *list = sicore_vec_get(&table->observers_by_event, event, sicore_vec_t);
    uint32_t n = list->size;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t oid = *sicore_vec_get(list, i, uint16_t);
        ecs_observer_t *obs =
            sicore_vec_get_mut(&ecs_world.observer_index.observers, oid, ecs_observer_t);
        if (!obs->enabled) {
            continue;
        }
        ecs_observer_event_t observer_event = {
            .entity = entity,
            .event = event,
            .user_data = obs->user_data,
            .trigger_data = trigger_data,
        };
        obs->callback(&observer_event);
    }
}

static inline bool ecs_is_deferred(void) {
    return ecs_world.defer_depth != 0 || ecs_world.flushing_commands;
}

void ecs_bootstrap(void);

#endif

ECS_RELATION_DEFINE(ChildOf, EcsRelationCascadeDelete);
ECS_TAG_DEFINE(Disabled);
ECS_TAG_DEFINE(Abstract);

void ecs_bootstrap() {
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create((ecs_type_t){ 0 });
    sicore_vec_push_u64(&ecs_world.entity_index.entities, 0);
    ecs_component({ SIECS_NAME_INIT("Invalid") });

    ECS_COMPONENT_REGISTER(ChildOf);
    ECS_COMPONENT_REGISTER(Disabled);
    ECS_COMPONENT_REGISTER(Abstract);

}

#ifndef SIECS_EVENT_OPS_H
#define SIECS_EVENT_OPS_H

static inline void ecs_emit_added_components(
    const ecs_table_t *from_table,
    ecs_table_t *to_table,
    ecs_entity_t entity,
    uint32_t row
) {
    uint16_t from_i = 0;
    for (uint16_t to_i = 0; to_i < to_table->type.count; to_i++) {
        ecs_component_t added = to_table->type.ids[to_i];
        while (from_i < from_table->type.count && from_table->type.ids[from_i] < added) {
            from_i++;
        }
        if (from_i < from_table->type.count && from_table->type.ids[from_i] == added) {
            continue;
        }

        void *data = ecs_table_component_at_column(to_table, to_i, row);
        const ecs_component_record_t *record = ecs_component_index_get(added);
        if (record->on_add) {
            record->on_add(entity, added, data);
        }
        ecs_emit(to_table, entity, EcsOnAdd, data);
    }
}

#endif

#ifndef SIECS_TABLE_MIGRATION_H
#define SIECS_TABLE_MIGRATION_H
#ifndef SIECS_TABLE_OPS_H
#define SIECS_TABLE_OPS_H

#include <string.h>

static inline void ecs_table_move_column(
    const ecs_table_t *from_table,
    uint16_t from_col,
    uint32_t from_row,
    ecs_table_t *to_table,
    uint16_t to_col,
    uint32_t to_row
) {
    const ecs_column_t *from_column = &from_table->cls[from_col];
    void *src = ecs_table_component_at_column(from_table, from_col, from_row);
    void *dst = ecs_table_component_at_column(to_table, to_col, to_row);
    if (from_column->flags & EcsColumnTrivialMove) {
        memcpy(dst, src, from_column->size);
        return;
    }

    ecs_component_t component = from_table->type.ids[from_col];
    const ecs_component_record_t *record = ecs_component_index_get(component);
    ecs_component_value_move_ctor(record, dst, src, 1);
}

static inline void ecs_table_ctor_column(
    const ecs_table_t *table,
    uint16_t col,
    uint32_t row
) {
    const ecs_column_t *column = &table->cls[col];
    if (column->size == 0) {
        return;
    }

    void *dst = ecs_table_component_at_column(table, col, row);
    if (column->flags & EcsColumnZeroCtor) {
        memset(dst, 0, column->size);
        return;
    }

    ecs_component_t component = table->type.ids[col];
    const ecs_component_record_t *record = ecs_component_index_get(component);
    ecs_component_value_ctor(record, dst, 1);
}

static inline void ecs_table_dtor_column(
    const ecs_table_t *table,
    uint16_t col,
    uint32_t row
) {
    const ecs_column_t *column = &table->cls[col];
    if (column->flags & EcsColumnNoDtor) {
        return;
    }

    ecs_component_t component = table->type.ids[col];
    const ecs_component_record_t *record = ecs_component_index_get(component);
    void *ptr = ecs_table_component_at_column(table, col, row);
    ecs_component_value_dtor(record, ptr, 1);
}

static inline void ecs_table_remove_entity_update_record(
    ecs_table_t *table,
    ecs_entity_t entity,
    uint32_t row,
    bool row_values_live
) {
    ecs_entity_t moved = ecs_table_remove_entity(table, row, row_values_live);
    if (moved != entity) {
        ecs_get_record(moved)->table_row = row;
    }
}

static inline void ecs_table_finish_migration(
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint32_t old_row,
    uint16_t to_table_id,
    uint32_t new_row
) {
    ecs_table_remove_entity_update_record(from_table, entity, old_row, false);
    record->table_id = to_table_id;
    record->table_row = new_row;
}

#endif

#include <stdint.h>

#define ECS_ADD_PLAN_MAX_COMPONENTS 64

ecs_type_t ecs_type_with_requirements(
        ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec
);

#ifndef NDEBUG
bool ecs_component_requires(
    const     ecs_component_t component,
    ecs_component_t require
);
#endif

static inline void ecs_migrate_same_layout(
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);
    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    for (uint16_t i = 0; i < from_table->type.data_count; i++) {
        uint16_t col = from_table->data_columns[i];
        ecs_table_move_column(from_table, col, old_row, to_table, col, new_row);
    }

    ecs_table_finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
}

void ecs_migrate_to_table(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
);

void *ecs_migrate_add(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    uint16_t to_table_id,
    ecs_component_t added_id
);

void *ecs_migrate_add_many(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    uint16_t to_table_id,
    ecs_component_t requested_id
);

void ecs_migrate_remove(
        ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id,
    uint16_t col_idx
);

#endif

#ifndef SIECS_UTILS_H
#define SIECS_UTILS_H
#ifndef NDEBUG
#include <stdio.h>
#include <stdlib.h>
#define ecs_cid_valid(id) ((id) != 0)
#define ecs_entity_valid(entity) (ecs_first(entity) != 0)

#define ecs_assert(condition, ...) \
    if (!(condition)) { \
        fprintf(stderr, __VA_ARGS__); \
        abort(); \
    }

#define ecs_assert_id_valid(id) ecs_assert(ecs_cid_valid(id), "invalid id: %d, id must be registered\n", id)
#define ecs_assert_not_null(ptr) ecs_assert((ptr) != NULL, "null pointer: %s\n", #ptr)
#define ecs_assert_entity_valid(entity) ecs_assert(ecs_entity_valid(entity), "invalid entity: %d, entity must be registered\n", ecs_first(entity))
#define ecs_assert_is_alive(entity) ecs_assert(ecs_is_alive(entity), "entity is dead: %d\n", ecs_first(entity))

#else
#define ecs_assert(condition, ...)
#define ecs_assert_id_valid(id)
#define ecs_assert_not_null(ptr)
#define ecs_assert_entity_valid(entity)
#define ecs_assert_is_alive(entity)
#endif

#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ECS_COMMAND_NONE UINT32_MAX

static inline void id_vec_push_unique(sicore_vec_t *vec, ecs_component_t id) {
    if (!sicore_vec_contains_u16(vec, id)) {
        sicore_vec_push_u16(vec, id);
    }
}

static inline void deferred_set_fini(ecs_deferred_set_t *set) {
    if (!set->data) {
        return;
    }
    const ecs_component_record_t *record = ecs_component_index_get(set->id);
    ecs_component_value_dtor(record, set->data, 1);
    set->data = NULL;
}

static inline void set_vec_remove(sicore_vec_t *vec, ecs_component_t id) {
    ecs_deferred_set_t *sets = sicore_vec_data(vec, ecs_deferred_set_t);
    for (uint32_t i = 0; i < vec->size; i++) {
        if (sets[i].id == id) {
            deferred_set_fini(&sets[i]);
            sicore_vec_remove_fast(vec, i, sizeof(ecs_deferred_set_t));
            return;
        }
    }
}

static inline ecs_deferred_set_t *set_vec_find(sicore_vec_t *vec, ecs_component_t id) {
    ecs_deferred_set_t *sets = sicore_vec_data(vec, ecs_deferred_set_t);
    for (uint32_t i = 0; i < vec->size; i++) {
        if (sets[i].id == id) {
            return &sets[i];
        }
    }
    return NULL;
}

static inline void command_init(ecs_entity_command_t *command, ecs_entity_t entity) {
    *command = (ecs_entity_command_t){ .entity = entity };
    sicore_vec_init(&command->add_ids, sizeof(ecs_component_t));
    sicore_vec_init(&command->remove_ids, sizeof(ecs_component_t));
    sicore_vec_init(&command->sets, sizeof(ecs_deferred_set_t));
}

static inline void command_fini(ecs_entity_command_t *command) {
    ecs_deferred_set_t *sets = sicore_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size; i++) {
        deferred_set_fini(&sets[i]);
    }
    sicore_vec_fini(&command->add_ids);
    sicore_vec_fini(&command->remove_ids);
    sicore_vec_fini(&command->sets);
}

void ecs_command_buffer_init() {
    ecs_command_buffer_t *buffer = &ecs_world.commands;
    sicore_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));
    buffer->entity_to_command = NULL;
    buffer->entity_capacity = 0;
}

void ecs_command_buffer_fini() {
    ecs_command_buffer_t *buffer = &ecs_world.commands;
    ecs_entity_command_t *commands = sicore_vec_data(&buffer->commands, ecs_entity_command_t);
    for (uint32_t i = 0; i < buffer->commands.size; i++) {
        command_fini(&commands[i]);
    }
    sicore_vec_fini(&buffer->commands);
    free(buffer->entity_to_command);
}

static void command_buffer_ensure_entity(ecs_command_buffer_t *buffer, uint32_t entity_id) {
    if (entity_id < buffer->entity_capacity) {
        return;
    }

    uint32_t new_capacity = buffer->entity_capacity ? buffer->entity_capacity : 256;
    while (new_capacity <= entity_id) {
        new_capacity *= 2;
    }

    buffer->entity_to_command = realloc(buffer->entity_to_command, sizeof(uint32_t) * new_capacity);
    for (uint32_t i = buffer->entity_capacity; i < new_capacity; i++) {
        buffer->entity_to_command[i] = ECS_COMMAND_NONE;
    }
    buffer->entity_capacity = new_capacity;
}

static ecs_entity_command_t *command_for_entity(ecs_entity_t entity) {
    ecs_command_buffer_t *buffer = &ecs_world.commands;
    uint32_t entity_id = ecs_first(entity);
    command_buffer_ensure_entity(buffer, entity_id);

    uint32_t command_index = buffer->entity_to_command[entity_id];
    if (command_index != ECS_COMMAND_NONE) {
        return sicore_vec_get_mut(&buffer->commands, command_index, ecs_entity_command_t);
    }

    command_index = buffer->commands.size;
    ecs_entity_command_t *command =
        sicore_vec_push_empty(&buffer->commands, sizeof(ecs_entity_command_t));
    command_init(command, entity);
    buffer->entity_to_command[entity_id] = command_index;
    return command;
}

void ecs_command_buffer_add(ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_command_t *command = command_for_entity(entity);
    sicore_vec_remove_u16(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);
}

void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_command_t *command = command_for_entity(entity);
    sicore_vec_remove_u16(&command->add_ids, id);
    set_vec_remove(&command->sets, id);
    id_vec_push_unique(&command->remove_ids, id);
}

void ecs_command_buffer_set(ecs_entity_t entity, ecs_component_t id, const void *data) {
    ecs_entity_command_t *command = command_for_entity(entity);
    const ecs_component_record_t *record = ecs_component_index_get(id);

    sicore_vec_remove_u16(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);

    ecs_deferred_set_t *set = set_vec_find(&command->sets, id);
    if (set) {
        ecs_component_value_dtor(record, set->data, 1);
        ecs_component_value_copy_ctor(record, set->data, data, 1);
        return;
    }

    uint32_t size = record->size ? record->size : 1;
    void *copy = ecs_arena_alloc(&ecs_world.arena_allocator, size);
    ecs_component_value_copy_ctor(record, copy, data, 1);
    ecs_deferred_set_t new_set = { .id = id, .data = copy };
    sicore_vec_push(&command->sets, &new_set, sizeof(ecs_deferred_set_t));
}

void ecs_command_buffer_move(ecs_entity_t entity, ecs_component_t id, void *data) {
    ecs_entity_command_t *command = command_for_entity(entity);
    const ecs_component_record_t *record = ecs_component_index_get(id);

    sicore_vec_remove_u16(&command->remove_ids, id);
    id_vec_push_unique(&command->add_ids, id);

    ecs_deferred_set_t *set = set_vec_find(&command->sets, id);
    if (set) {
        ecs_component_value_dtor(record, set->data, 1);
        ecs_component_value_move_ctor(record, set->data, data, 1);
        return;
    }

    uint32_t size = record->size ? record->size : 1;
    void *copy = ecs_arena_alloc(&ecs_world.arena_allocator, size);
    ecs_component_value_move_ctor(record, copy, data, 1);
    ecs_deferred_set_t new_set = { .id = id, .data = copy };
    sicore_vec_push(&command->sets, &new_set, sizeof(ecs_deferred_set_t));
}

void ecs_command_buffer_kill(ecs_entity_t entity) {
    ecs_entity_command_t *command = command_for_entity(entity);
    command->kill = true;
    command->has_base = false;
    sicore_vec_clear(&command->add_ids);
    sicore_vec_clear(&command->remove_ids);
    ecs_deferred_set_t *sets = sicore_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size; i++) {
        deferred_set_fini(&sets[i]);
    }
    sicore_vec_clear(&command->sets);
}

void ecs_command_buffer_set_base(ecs_entity_t entity, ecs_entity_t target) {
    ecs_entity_command_t *command = command_for_entity(entity);
    command->has_base = true;
    command->base = target;
}

static void final_ids_push_sorted(sicore_vec_t *final_ids, ecs_component_t id) {
    ecs_component_t *ids = sicore_vec_data(final_ids, ecs_component_t);
    uint32_t i = 0;
    while (i < final_ids->size && ids[i] < id) {
        i++;
    }
    if (i < final_ids->size && ids[i] == id) {
        return;
    }

    sicore_vec_push_empty(final_ids, sizeof(ecs_component_t));
    ids = sicore_vec_data(final_ids, ecs_component_t);
    for (uint32_t j = final_ids->size - 1; j > i; j--) {
        ids[j] = ids[j - 1];
    }
    ids[i] = id;
}

static void final_ids_collect_requirements(sicore_vec_t *final_ids, ecs_component_t id) {
    const ecs_component_record_t *record = ecs_component_index_get(id);
    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t required = record->required[i];
        if (sicore_vec_contains_u16(final_ids, required)) {
            continue;
        }
        final_ids_collect_requirements(final_ids, required);
        final_ids_push_sorted(final_ids, required);
    }
}

static ecs_type_t
command_build_type(const ecs_table_t *table, const ecs_entity_command_t *command) {
    sicore_vec_t final_ids;
    sicore_vec_init(&final_ids, sizeof(ecs_component_t));

    for (uint16_t i = 0; i < table->type.count; i++) {
        ecs_component_t id = table->type.ids[i];
        if (!sicore_vec_contains_u16(&command->remove_ids, id)) {
            sicore_vec_push_u16(&final_ids, id);
        }
    }

    const ecs_component_t *adds = sicore_vec_data(&command->add_ids, ecs_component_t);
    for (uint32_t i = 0; i < command->add_ids.size; i++) {
        final_ids_collect_requirements(&final_ids, adds[i]);
        final_ids_push_sorted(&final_ids, adds[i]);
    }

    return (ecs_type_t){
        .ids = sicore_vec_data(&final_ids, ecs_component_t),
        .count = (uint16_t)final_ids.size,
        .base = command->has_base ? command->base : table->type.base,
    };
}

static void command_emit_removed(
    ecs_table_t *table,
    ecs_entity_t entity,
    uint32_t row,
    const ecs_type_t *final_type
) {
    uint16_t final_i = 0;
    for (uint16_t i = 0; i < table->type.count; i++) {
        ecs_component_t id = table->type.ids[i];
        while (final_i < final_type->count && final_type->ids[final_i] < id) {
            final_i++;
        }
        if (final_i < final_type->count && final_type->ids[final_i] == id) {
            continue;
        }

        void *data = ecs_table_component_at_column(table, i, row);
        const ecs_component_record_t *record = ecs_component_index_get(id);
        if (record->on_remove) {
            record->on_remove(entity, id, data);
        }
        ecs_emit(table, entity, EcsOnRemove, data);
    }
}

static bool command_type_unchanged(const ecs_table_t *table, const ecs_entity_command_t *command) {
    if (command->remove_ids.size != 0) {
        return false;
    }
    if (command->has_base && command->base != table->type.base) {
        return false;
    }

    const ecs_component_t *adds = sicore_vec_data(&command->add_ids, ecs_component_t);
    for (uint32_t i = 0; i < command->add_ids.size; i++) {
        if (!ecs_table_has_owned(table, adds[i])) {
            return false;
        }
    }
    return true;
}

static void command_apply_sets(ecs_entity_command_t *command) {
    ecs_deferred_set_t *sets = sicore_vec_data(&command->sets, ecs_deferred_set_t);
    for (uint32_t i = 0; i < command->sets.size && ecs_is_alive(command->entity); i++) {
        ecs_component_t id = sets[i].id;
        const ecs_component_record_t *record = ecs_component_index_get(id);
        ecs_entity_record_t *entity_record = ecs_get_record(command->entity);
        ecs_table_t *table = ecs_get_table(entity_record->table_id);
        uint16_t column = ecs_table_get_column_index(table, id);
        void *dst = ecs_table_component_at_column(table, column, entity_record->table_row);

        if (record->on_set) {
            record->on_set(command->entity, id, sets[i].data, dst);
            if (!ecs_is_alive(command->entity)) {
                return;
            }
            entity_record = ecs_get_record(command->entity);
            table = ecs_get_table(entity_record->table_id);
            column = ecs_table_get_column_index(table, id);
            dst = ecs_table_component_at_column(table, column, entity_record->table_row);
        }
        ecs_emit(table, command->entity, EcsOnSet, sets[i].data);
        ecs_component_value_move(record, dst, sets[i].data, 1);
        sets[i].data = NULL;
    }
}

static void command_apply(ecs_entity_command_t *command) {
    if (!ecs_is_alive(command->entity)) {
        return;
    }

    if (command->kill) {
        ecs_kill_now(command->entity);
        return;
    }

    ecs_entity_record_t *record = ecs_get_record(command->entity);
    uint16_t old_table_id = record->table_id;
    ecs_table_t *old_table = ecs_get_table(old_table_id);
    if (command_type_unchanged(old_table, command)) {
        command_apply_sets(command);
        return;
    }

    ecs_type_t final_type = command_build_type(old_table, command);

    if (!ecs_type_equals(&old_table->type, &final_type)) {
        uint32_t old_row = record->table_row;
        command_emit_removed(old_table, command->entity, old_row, &final_type);
        if (!ecs_is_alive(command->entity)) {
            ecs_type_fini(&final_type);
            return;
        }

        uint16_t new_table_id = ecs_table_index_get_or_create(final_type);
        record = ecs_get_record(command->entity);
        old_table = ecs_get_table(old_table_id);
        const ecs_table_t *emit_old_table = old_table;
        ecs_migrate_to_table(record, command->entity, old_table, new_table_id);
        record = ecs_get_record(command->entity);
        ecs_table_t *new_table = ecs_get_table(record->table_id);
        ecs_emit_added_components(emit_old_table, new_table, command->entity, record->table_row);
    } else {
        ecs_type_fini(&final_type);
    }

    command_apply_sets(command);
}

void ecs_command_buffer_flush() {
    ecs_command_buffer_t *buffer = &ecs_world.commands;
    if (buffer->commands.size == 0) {
        ecs_arena_reset(&ecs_world.arena_allocator);
        return;
    }

    ecs_world.flushing_commands = true;
    while (buffer->commands.size != 0) {
        sicore_vec_t commands = buffer->commands;
        sicore_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));

        ecs_entity_command_t *items = sicore_vec_data(&commands, ecs_entity_command_t);
        for (uint32_t i = 0; i < commands.size; i++) {
            uint32_t entity_id = ecs_first(items[i].entity);
            buffer->entity_to_command[entity_id] = ECS_COMMAND_NONE;
        }

        for (uint32_t i = 0; i < commands.size; i++) {
            command_apply(&items[i]);
            command_fini(&items[i]);
        }
        sicore_vec_fini(&commands);
    }
    ecs_world.flushing_commands = false;
    ecs_arena_reset(&ecs_world.arena_allocator);
}

void ecs_defer_begin(void) { ecs_world.defer_depth++; }

void ecs_defer_end(void) {
    ecs_assert(ecs_world.defer_depth > 0, "ecs_defer_end called without ecs_defer_begin\n");
    ecs_world.defer_depth--;
    if (ecs_world.defer_depth == 0) {
        ecs_command_buffer_flush();
    }
}

static ecs_component_t ecs_component_alloc_ids(uint16_t count) {
    uint32_t id = ecs_world.component_index.components.size;
    ecs_assert(id + count <= UINT16_MAX, "component id overflow\n");
    return id;
}

void RelationOnSet(
    ecs_entity_t entity,
    ecs_component_t target_component,
    const void *new_value,
    void *current_value
) {
    const RelationTarget *target_data = new_value;
    ecs_component_t source_component = target_component + 1;

    const RelationTarget *old_target_data = current_value;

    ecs_assert_entity_valid(target_data->target);
    ecs_assert_is_alive(target_data->target);

    if (old_target_data->target == target_data->target) {
        return;
    }

    if (old_target_data->target) {
        RelationSource *source = ecs_get_cid(old_target_data->target, source_component);

        sicore_vec_remove_u64(&source->entities, entity);
        if (source->entities.size == 0) {
            ecs_remove_cid(old_target_data->target, source_component);
        }
    }

    if (ecs_has_cid(target_data->target, source_component)) {
        RelationSource *source_data = ecs_get_cid(target_data->target, source_component);
        sicore_vec_push_u64(&source_data->entities, entity);
    } else {
        RelationSource source_data = {};
        sicore_vec_init(&source_data.entities, sizeof(ecs_entity_t));
        sicore_vec_push_u64(&source_data.entities, entity);
        ecs_set_cid(target_data->target, source_component, &source_data);
    }
}

void RelationOnRemove(ecs_entity_t entity, ecs_component_t component, void *ptr) {
    const RelationTarget *target_data = ptr;
    ecs_component_t source_component = component + 1;
    RelationSource *target_source_data = ecs_get_cid(target_data->target, source_component);

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    if (target_source_data->entities.size == UINT32_MAX) {
        return;
    }

    sicore_vec_remove_u64(&target_source_data->entities, entity);

    if (target_source_data->entities.size == 0) {
        ecs_remove_cid(target_data->target, source_component);
    }
}

static void RelationSourceDtor(void *ptr, uint32_t count) {
    RelationSource *source_data = ptr;
    for (uint32_t i = 0; i < count; i++) {
        sicore_vec_fini(&source_data[i].entities);
    }
}

void RelationSourceOnRemove(ecs_entity_t, ecs_component_t component, void *ptr) {
    RelationSource *source_data = ptr;

    const ecs_entity_t *entities = source_data->entities.data;
    const uint32_t count = source_data->entities.size;
    const ecs_component_record_t *crec = ecs_component_index_get(component);
    const bool cascade_delete = crec->relation_flags & EcsRelationCascadeDelete;

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    source_data->entities.size = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        if (cascade_delete) {
            ecs_kill(entities[i]);
        } else {
            ecs_remove_cid(entities[i], component - 1);
        }
    }
}

static ecs_component_t ecs_component_register_type(
    ecs_component_t *id,
    const ecs_component_desc_t *desc
) {
    ecs_assert_not_null(id);
    ecs_assert_not_null(desc);

    if (*id != 0) {
        const ecs_component_record_t *existing = ecs_component_index_get(*id);
        if (existing->tables.data) {
            return *id;
        }
    }

    if (ECS_UNLIKELY(desc->relation_flags & EcsRelationTarget)) {
        if (*id == 0) {
            *id = ecs_component_alloc_ids(2);
        }

        ecs_component_t component = *id;
        ecs_component_index_register(
            component,
            desc->size,
            desc->ops,
            RelationOnSet,
            RelationOnRemove,
            desc->on_add,
            desc->relation_flags
        );

        ecs_component_t source = component + 1;
        ecs_component_index_register(
            source,
            desc->relation_flags & EcsRelationOneToOne ? sizeof(RelationTarget)
                                                       : sizeof(RelationSource),
            (ecs_type_ops_t){
                .dtor = desc->relation_flags & EcsRelationOneToOne ? NULL : RelationSourceDtor,
            },
            NULL,
            RelationSourceOnRemove,
            desc->on_add,
            (desc->relation_flags & ~EcsRelationTarget) | EcsRelationSource
        );
        return component;
    } else {
        if (*id == 0) {
            *id = ecs_component_alloc_ids(1);
        }

        ecs_component_t component = *id;
        ecs_component_index_register(
            component,
            desc->size,
            desc->ops,
            desc->on_set,
            desc->on_remove,
            desc->on_add,
            0
        );
        return component;
    }
}

ecs_component_t ecs_component_register(ecs_component_t *id, const ecs_component_desc_t *desc) {
    return ecs_component_register_type(id, desc);
}

ecs_component_t ecs_component_init(const ecs_component_desc_t *desc) {
    ecs_component_t id = 0;
    return ecs_component_register(&id, desc);
}

const ecs_component_info_t *ecs_component_info(ecs_component_t component) {
    if (component == 0 || component >= ecs_world.component_index.components.size) {
        return NULL;
    }
    return ecs_component_index_get(component)->info;
}

#define ecs_assert_can_be_updated(entity, ...)                                                     \
    ecs_assert(!ecs_has_cid_owned(entity, ecs_id(Abstract)), __VA_ARGS__)

#define entity_edit(entity, table, record)                                                         \
    ecs_entity_record_t *record = ecs_get_record(entity);                                          \
    ecs_table_t *table = ecs_get_table(record->table_id);

void ecs_add_cid_now(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    ecs_assert_can_be_updated(entity, "An abstract entity cannot be updated.");

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(from_id);
    uint16_t edge = ecs_table_get_add_edge(table, cid);

    if (ECS_UNLIKELY(edge < table->type.count && table->type.ids[edge] == cid)) {
        return;
    }

    const ecs_component_record_t *crec = ecs_component_index_get(cid);

    if (crec->required_count == 0) {
        if (edge == UINT16_MAX) {
            ecs_type_t new_type = ecs_type_with_add(&table->type, cid);
            edge = ecs_table_index_get_or_create(new_type);

            table = ecs_get_table(from_id);
            ecs_id_map_set(&table->add_edge, cid, edge);
        }

        ecs_table_t *new_table = ecs_get_table(edge);
        void *component_data = ecs_migrate_add(record, entity, table, new_table, edge, cid);

        if (crec->on_add) {
            crec->on_add(entity, cid, component_data);
        }
        ecs_emit(new_table, entity, EcsOnAdd, component_data);
        return;
    }

    if (edge == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_requirements(table, cid, crec);
        edge = ecs_table_index_get_or_create(new_type);

        table = ecs_get_table(from_id);
        ecs_id_map_set(&table->add_edge, cid, edge);
    }

    ecs_table_t *new_table = ecs_get_table(edge);
    bool add_many = new_table->type.count > table->type.count + 1;

    void *component_data = add_many
                               ? ecs_migrate_add_many(record, entity, table, new_table, edge, cid)
                               : ecs_migrate_add(record, entity, table, new_table, edge, cid);

    if (add_many) {
        ecs_emit_added_components(table, new_table, entity, record->table_row);
        return;
    }
    if (crec->on_add) {
        crec->on_add(entity, cid, component_data);
    }

    ecs_emit(new_table, entity, EcsOnAdd, component_data);
}

void ecs_add_cid(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    ecs_assert_can_be_updated(entity, "An abstract entity cannot be updated.");

    if (ecs_is_deferred()) {
        ecs_command_buffer_add(entity, cid);
        return;
    }

    ecs_add_cid_now(entity, cid);
}

void ecs_remove_cid_now(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(from_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);

    if (ECS_UNLIKELY(col_idx == UINT16_MAX)) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_remove_at(&table->type, col_idx);
        new_table_id = ecs_table_index_get_or_create(new_type);
        table = ecs_get_table(from_id);
        table->cls[col_idx].remove_edge = new_table_id;
    }

    void *removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);

    const ecs_component_record_t *crec = ecs_component_index_get(cid);
    if (crec->on_remove) {
        crec->on_remove(entity, cid, removed_data);
    }
    ecs_emit(table, entity, EcsOnRemove, removed_data);

    ecs_migrate_remove(record, entity, table, new_table_id, (uint16_t)col_idx);
}

void ecs_remove_cid(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_remove(entity, cid);
        return;
    }

    ecs_remove_cid_now(entity, cid);
}

/*
 * Resolve an owned or inherited component from a live entity record.
 * The record and every base in its type chain are trusted SIECS invariants;
 * callers perform the public entity validation before entering this helper.
 */
static inline void *
ecs_component_get_from_record(const ecs_entity_record_t *record, ecs_component_t component) {
    ecs_table_t *table = ecs_get_table(record->table_id);
    uint16_t col_idx = ecs_table_column_or_invalid(table, component);
    if (col_idx != UINT16_MAX) {
        return ecs_table_component_at_column(table, col_idx, record->table_row);
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *base_record = ecs_get_record(base);
        ecs_table_t *base_table = ecs_get_table(base_record->table_id);

        col_idx = ecs_table_column_or_invalid(base_table, component);
        if (col_idx != UINT16_MAX) {
            return ecs_table_component_at_column(base_table, col_idx, base_record->table_row);
        }

        base = base_table->type.base;
    }

    return NULL;
}

void *ecs_get_cid(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    return ecs_component_get_from_record(ecs_get_record(entity), cid);
}

void *ecs_try_get_cid(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);
    return ecs_component_get_from_record(ecs_get_record(entity), cid);
}

void ecs_set_cid_now(ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_add_cid_now(entity, cid);
    ecs_defer_begin();
    const ecs_component_record_t *crec = ecs_component_index_get(cid);
    entity_edit(entity, table, record);
    void *dst = ecs_table_get_component(table, cid, record->table_row);

    if (crec->on_set) {
        crec->on_set(entity, cid, data, dst);
    }
    ecs_emit(table, entity, EcsOnSet, data);
    ecs_component_value_copy(crec, dst, data, 1);
    ecs_defer_end();
}

void ecs_set_cid(ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_set(entity, cid, data);
        return;
    }

    ecs_set_cid_now(entity, cid, data);
}

void ecs_move_cid_now(ecs_entity_t entity, ecs_component_t cid, void *data) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    bool had_value = ecs_has_cid_owned(entity, cid);
    ecs_add_cid_now(entity, cid);
    const ecs_component_record_t *crec = ecs_component_index_get(cid);
    entity_edit(entity, table, record);
    void *dst = ecs_table_get_component(table, cid, record->table_row);

    if (crec->on_set) {
        crec->on_set(entity, cid, data, dst);
    }
    ecs_emit(table, entity, EcsOnSet, data);
    if (had_value || crec->ops.ctor) {
        ecs_component_value_move(crec, dst, data, 1);
    } else {
        ecs_component_value_move_ctor(crec, dst, data, 1);
    }
}

void ecs_move_cid(ecs_entity_t entity, ecs_component_t cid, void *data) {
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_move(entity, cid, data);
        return;
    }

    ecs_move_cid_now(entity, cid, data);
}

bool ecs_has_cid(const ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    uint16_t tid = ecs_get_record(entity)->table_id;
    return ecs_table_has(ecs_get_table(tid), id);
}

bool ecs_has_cid_owned(const ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    uint16_t tid = ecs_get_record(entity)->table_id;
    return ecs_table_has_owned(ecs_get_table(tid), id);
}

void ecs_with(ecs_component_t component, ecs_component_t require) {
    ecs_assert_id_valid(component);
    ecs_assert_id_valid(require);
    ecs_assert(component != require, "component cannot require itself: %d\n", component);
#ifndef NDEBUG
    ecs_assert(
        !ecs_component_requires(require, component),
        "cyclic component requirement: %d requires %d\n",
        component,
        require
    );
#endif

    ecs_component_record_t *record = ecs_component_index_get_mut(component);

    ecs_assert(record->tables.size == 0, "component already used cannot register requirement");

#ifndef NDEBUG
    for (uint32_t i = 0; i < record->required_count; i++) {
        if (record->required[i] == require) {
            ecs_assert(true, "required component already registered");
        }
    }
#endif

    record->required =
        realloc(record->required, sizeof(ecs_component_t) * (record->required_count + 1));
    record->required[record->required_count++] = require;
}

#include <stdbool.h>

typedef struct {
    ecs_component_t ids[ECS_ADD_PLAN_MAX_COMPONENTS];
    uint16_t count;
} ecs_add_plan_t;

static inline bool ecs_add_plan_has(const ecs_add_plan_t *plan, ecs_component_t id) {
    for (uint16_t i = 0; i < plan->count; i++) {
        if (plan->ids[i] == id) {
            return true;
        }
    }
    return false;
}

static inline void ecs_add_plan_push(ecs_add_plan_t *plan, ecs_component_t id) {
#ifndef NDEBUG
    if (plan->count == ECS_ADD_PLAN_MAX_COMPONENTS) {
        abort();
    }
#endif
    plan->ids[plan->count++] = id;
}

static inline void ecs_add_plan_collect_requirements(
    ecs_table_t *from_table,
    ecs_add_plan_t *plan,
    const ecs_component_record_t *crec
) {
    for (uint32_t i = 0; i < crec->required_count; i++) {
        ecs_component_t required = crec->required[i];
        if (ecs_table_has_owned(from_table, required) || ecs_add_plan_has(plan, required)) {
            continue;
        }

        const ecs_component_record_t *required_rec = ecs_component_index_get(required);
        if (required_rec->required_count) {
            ecs_add_plan_collect_requirements(from_table, plan, required_rec);
        }
        ecs_add_plan_push(plan, required);
    }
}

static inline void ecs_sort_component_ids(ecs_component_t *ids, uint16_t count) {
    for (uint16_t i = 1; i < count; i++) {
        ecs_component_t id = ids[i];
        uint16_t j = i;
        while (j > 0 && ids[j - 1] > id) {
            ids[j] = ids[j - 1];
            j--;
        }
        ids[j] = id;
    }
}

ecs_type_t ecs_type_with_requirements(
    ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec
) {
    ecs_add_plan_t plan = { 0 };
    ecs_add_plan_collect_requirements(from_table, &plan, crec);
    ecs_add_plan_push(&plan, cid);
    ecs_sort_component_ids(plan.ids, plan.count);

    ecs_type_t type = {
        .ids = malloc(sizeof(ecs_component_t) * (from_table->type.count + plan.count)),
        .count = from_table->type.count + plan.count,
        .base = from_table->type.base,
    };

    uint16_t from_i = 0;
    uint16_t add_i = 0;
    uint16_t out_i = 0;
    while (from_i < from_table->type.count && add_i < plan.count) {
        ecs_component_t from_id = from_table->type.ids[from_i];
        ecs_component_t add_id = plan.ids[add_i];
        if (from_id < add_id) {
            type.ids[out_i++] = from_id;
            from_i++;
        } else {
            type.ids[out_i++] = add_id;
            add_i++;
        }
    }
    while (from_i < from_table->type.count) {
        type.ids[out_i++] = from_table->type.ids[from_i++];
    }
    while (add_i < plan.count) {
        type.ids[out_i++] = plan.ids[add_i++];
    }

    return type;
}

#ifndef NDEBUG
bool ecs_component_requires(const ecs_component_t component, ecs_component_t require) {
    const ecs_component_record_t *record = ecs_component_index_get(component);

    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t current = record->required[i];
        if (current == require || ecs_component_requires(current, require)) {
            return true;
        }
    }

    return false;
}
#endif

static inline ecs_entity_t ecs_entity_index_create(uint32_t row) {
    ecs_entity_index_t *index = &ecs_world.entity_index;
    uint32_t entity_id;
    uint32_t generation;
    if (index->first_available != UINT32_MAX) {
        entity_id = index->first_available;
        ecs_entity_record_t *record = ecs_entity_index_get_record(entity_id);
        index->first_available = record->table_row;
        generation = record->generation;
        record->table_id = 0;
        record->table_row = row;
    } else {
        entity_id = index->entities.size;
        generation = 0;
        ecs_entity_record_t *record =
            sicore_vec_push_empty(&index->entities, sizeof(ecs_entity_record_t));
        *record = (ecs_entity_record_t){ .generation = 0, .table_row = row, .table_id = 0 };
    }
    return ecs_entity(entity_id, generation);
}

ecs_entity_t ecs_new(void) {
    ecs_table_t *table = ecs_get_table(0);

    ecs_entity_t entity = ecs_entity_index_create(table->entity_count);
    ecs_table_add_entity(table, entity);

    return entity;
}

bool ecs_is_alive(const ecs_entity_t entity) { return ecs_entity_index_is_alive(entity); }

#ifndef NDEBUG
static inline bool ecs_would_create_base_cycle(const ecs_entity_t entity, ecs_entity_t target) {
    while (target != 0) {
        if (target == entity) {
            return true;
        }
        const ecs_entity_record_t *target_record = ecs_get_record(target);
        const ecs_table_t *target_table = ecs_get_table(target_record->table_id);
        target = target_table->type.base;
    }
    return false;
}
#endif

bool ecs_is(ecs_entity_t entity, ecs_entity_t target) {
    ecs_entity_t base = ecs_get_table(ecs_get_record(entity)->table_id)->type.base;
    if (base == target) {
        return true;
    }
    if (base == 0) {
        return false;
    }
    return ecs_is(base, target);
}

void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target) {
    ecs_assert_entity_valid(entity);
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(entity);
    ecs_assert_is_alive(target);
    ecs_assert(entity != target, "entity cannot inherit itself: %d\n", ecs_first(entity));
    ecs_assert(
        !ecs_would_create_base_cycle(entity, target),
        "cyclic inheritance: %d inherits from %d\n",
        ecs_first(entity),
        ecs_first(target)
    );
    if (!ecs_has_cid_owned(target, ecs_id(Abstract))) {
        ecs_add_cid_now(target, ecs_id(Abstract));
    }

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_table_id = record->table_id;
    ecs_table_t *from_table = ecs_get_table(from_table_id);
    if (from_table->type.base == target) {
        return;
    }

    ecs_type_t new_type = ecs_type_with_base(&from_table->type, target);
    uint16_t to_table_id = ecs_table_index_get_or_create(new_type);
    if (to_table_id == from_table_id) {
        return;
    }

    from_table = ecs_get_table(from_table_id);
    ecs_migrate_same_layout(record, entity, from_table, to_table_id);
}

void ecs_is_a(ecs_entity_t entity, ecs_entity_t target) {
    ecs_assert_entity_valid(entity);
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(entity);
    ecs_assert_is_alive(target);

    if (ecs_is_deferred()) {
        ecs_add_cid(target, ecs_id(Abstract));
        ecs_command_buffer_set_base(entity, target);
        return;
    }

    ecs_is_a_now(entity, target);
}

static inline void ecs_entity_index_kill(uint32_t entity_id) {
    ecs_entity_index_t *index = &ecs_world.entity_index;
    ecs_entity_record_t *record = ecs_entity_index_get_record(entity_id);
    record->generation += 1;
    record->table_row = index->first_available;
    record->table_id = UINT16_MAX;
    index->first_available = entity_id;
}

void ecs_kill_now(ecs_entity_t entity) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *initial_table = ecs_get_table(record->table_id);
    const ecs_component_t *components = initial_table->type.ids;
    uint16_t component_count = initial_table->type.count;
    ecs_table_t *table = initial_table;

    for (uint16_t i = 0; i < component_count && ecs_is_alive(entity); i++) {
        ecs_component_t component = components[i];
        record = ecs_get_record(entity);
        table = ecs_get_table(record->table_id);

        uint16_t col_idx = ecs_table_column_or_invalid(table, component);
        if (col_idx == UINT16_MAX) {
            continue;
        }

        void *removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);
        const ecs_component_record_t *crec = ecs_component_index_get(component);

        if (crec->on_remove) {
            crec->on_remove(entity, component, removed_data);
            if (!ecs_is_alive(entity)) {
                break;
            }
            record = ecs_get_record(entity);
            table = ecs_get_table(record->table_id);
            col_idx = ecs_table_column_or_invalid(table, component);
            if (col_idx == UINT16_MAX) {
                continue;
            }
            removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);
        }
        ecs_emit(table, entity, EcsOnRemove, removed_data);
    }

    if (!ecs_is_alive(entity)) {
        return;
    }

    record = ecs_get_record(entity);
    table = ecs_get_table(record->table_id);

    // Remove from table
    ecs_table_remove_entity_update_record(table, entity, record->table_row, true);

    ecs_entity_index_kill(ecs_first(entity));
}

void ecs_kill(ecs_entity_t entity) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_kill(entity);
        return;
    }

    ecs_kill_now(entity);
}

#ifndef SIECS_MODULE_H
#define SIECS_MODULE_H

void ecs_module_record_system(ecs_system_id_t system);
void ecs_module_record_observer(ecs_observer_id_t observer);

#endif

ecs_module_id_t ecs_module_init(const ecs_module_desc_t *desc) {
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->import);

    ecs_module_id_t existing = ecs_module_index_find(desc->id);
    if (existing) {
        return existing;
    }

    ecs_module_id_t module = ecs_module_index_create(
        desc->id
    );
    if (desc->id) {
        *desc->id = module;
    }

    ecs_module_id_t prev = ecs_world.active_module;
    ecs_world.active_module = module;
    desc->import(desc->desc);
    ecs_world.active_module = prev;

    if (desc->disabled) {
        ecs_module_disable(module);
    }

    return module;
}

void ecs_module_enable(ecs_module_id_t module) {

    ecs_module_t *record = ecs_module_index_get(module);
    if (record->enabled) {
        return;
    }

    const ecs_system_id_t *systems = sicore_vec_data(&record->systems, ecs_system_id_t);
    for (uint32_t i = 0; i < record->systems.size; i++) {
        ecs_system_enable(systems[i]);
    }

    const ecs_observer_id_t *observers = sicore_vec_data(&record->observers, ecs_observer_id_t);
    for (uint32_t i = 0; i < record->observers.size; i++) {
        ecs_observer_enable(observers[i]);
    }

    record->enabled = true;
}

ecs_module_id_t ecs_module_find(const ecs_module_id_t *id) { return ecs_module_index_find(id); }

void ecs_module_disable(ecs_module_id_t module) {

    ecs_module_t *record = ecs_module_index_get(module);
    if (!record->enabled) {
        return;
    }

    const ecs_system_id_t *systems = sicore_vec_data(&record->systems, ecs_system_id_t);
    for (uint32_t i = 0; i < record->systems.size; i++) {
        ecs_system_disable(systems[i]);
    }

    const ecs_observer_id_t *observers = sicore_vec_data(&record->observers, ecs_observer_id_t);
    for (uint32_t i = 0; i < record->observers.size; i++) {
        ecs_observer_disable(observers[i]);
    }

    record->enabled = false;
}

bool ecs_module_is_enabled(const ecs_module_id_t module) {
    return ecs_module_index_get_const(module)->enabled;
}

void ecs_module_record_system(ecs_system_id_t system) {
    ecs_module_id_t module = ecs_world.active_module;
    if (!module) {
        return;
    }

    ecs_module_t *record = ecs_module_index_get(module);
    sicore_vec_push_u16(&record->systems, system);
}

void ecs_module_record_observer(ecs_observer_id_t observer) {
    ecs_module_id_t module = ecs_world.active_module;
    if (!module) {
        return;
    }

    ecs_module_t *record = ecs_module_index_get(module);
    sicore_vec_push(&record->observers, &observer, sizeof(ecs_observer_id_t));
}

ecs_event_t ecs_event(void) { return ecs_world.observer_index.event_count++; }

ecs_event_t ecs_event_register(ecs_event_t *id) {
    ecs_assert_not_null(id);

    if (*id == UINT16_MAX) {
        *id = ecs_event();
        return *id;
    }

    if (ecs_world.observer_index.event_count <= *id) {
        ecs_world.observer_index.event_count = *id + 1;
    }

    return *id;
}

ecs_observer_id_t ecs_observer_init(const ecs_observer_desc_t *desc) {
    ecs_assert(desc->callback != NULL, "Observer callback cannot be NULL");
    ecs_observer_id_t oid = ecs_observer_index_create(desc);
    ecs_observer_index_match_tables(
        ecs_world.table_index.tables,
        ecs_world.table_index.table_count,
        oid
    );
    ecs_module_record_observer(oid);
    return oid;
}

void ecs_observer_enable(ecs_observer_id_t id) {
    sicore_vec_get_mut(&ecs_world.observer_index.observers, id, ecs_observer_t)->enabled = true;
}

void ecs_observer_disable(ecs_observer_id_t id) {
    sicore_vec_get_mut(&ecs_world.observer_index.observers, id, ecs_observer_t)->enabled = false;
}

void ecs_observer_trigger(ecs_entity_t entity, ecs_event_t event, const void *trigger_data) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    ecs_emit(table, entity, event, trigger_data);
}

static void ecs_query_index_remove_active_id(ecs_query_index_t *index, ecs_query_id_t qid) {
    ecs_query_cache_t *cache = sicore_vec_get_mut(&index->queries, qid, ecs_query_cache_t);
    uint32_t active_index = cache->active_index;
    uint32_t last_index = index->active_ids.size - 1;

    if (active_index != last_index) {
        ecs_query_id_t moved = *sicore_vec_get(&index->active_ids, last_index, ecs_query_id_t);
        ((ecs_query_id_t *)index->active_ids.data)[active_index] = moved;
        sicore_vec_get_mut(&index->queries, moved, ecs_query_cache_t)->active_index = active_index;
    }

    sicore_vec_remove_last(&index->active_ids);
}

ecs_query_id_t ecs_query_init(const ecs_query_desc_t *desc) {
    ecs_query_id_t qid = ecs_query_index_create(desc);
    ecs_query_index_update_matches(
        sicore_vec_get_mut(&ecs_world.query_index.queries, qid, ecs_query_cache_t)
    );
    return qid;
}

ecs_iter_t ecs_query_iter(ecs_query_id_t query_id) {
    ecs_assert(query_id < ecs_world.query_index.queries.size, "invalid query id: %u\n", query_id);

    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&ecs_world.query_index.queries, query_id, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", query_id);
    return (ecs_iter_t){
        .cache = cache,
        .table_idx = UINT16_MAX,
        .table_count = cache->table_ids.size,
        .count = 0,
    };
}

uint32_t ecs_query_count(ecs_query_id_t query_id) {
    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&ecs_world.query_index.queries, query_id, ecs_query_cache_t);
    uint16_t *tids = cache->table_ids.data;

    uint32_t count = 0;
    for (uint32_t i = 0; i < cache->table_ids.size; i++) {
        count += ecs_world.table_index.tables[tids[i]].entity_count;
    }
    return count;
}

bool ecs_iter_next(ecs_iter_t *it) {
    uint16_t *tids = it->cache->table_ids.data;
    do {
        if (++it->table_idx >= it->table_count)
            return false;
        it->count = ecs_world.table_index.tables[tids[it->table_idx]].entity_count;
    } while (it->count == 0);
    if (it->cache->query.field_count == 0) {
        it->ptrs = NULL;
        it->field_kind_bits = 0;
    } else {
        it->ptrs = &it->cache->fields_ptr[it->table_idx * it->cache->query.field_count];
        it->field_kind_bits = it->cache->field_kind_bits[it->table_idx];
    }
    it->entities = ecs_world.table_index.tables[tids[it->table_idx]].entities;
    return true;
}

void ecs_query_fini(ecs_query_id_t qid) {
    ecs_assert(qid < ecs_world.query_index.queries.size, "invalid query id: %u\n", qid);

    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&ecs_world.query_index.queries, qid, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", qid);

    ecs_query_index_destroy(&cache->query);
    free(cache->fields_ptr);
    free(cache->field_kind_bits);
    sicore_vec_fini(&cache->table_ids);
    cache->fields_ptr = NULL;
    cache->field_kind_bits = NULL;
    cache->field_table_capacity = 0;

    ecs_query_index_remove_active_id(&ecs_world.query_index, qid);
    cache->next_free = ecs_world.query_index.first_free;
    cache->alive = false;
    ecs_world.query_index.first_free = qid;
}

static ecs_resource_t ecs_resource_alloc_id(void) {
    ecs_resource_t id = ecs_world.resource_index.count;
    ecs_assert(id < UINT16_MAX, "resource id overflow\n");
    return id;
}

ecs_resource_t ecs_resource_init(const ecs_resource_desc_t *desc) {
        return ecs_resource_index_register(ecs_resource_alloc_id(), desc);
}

bool ecs_resource_is_registered_rid(ecs_resource_t id) {
    return ecs_resource_index_is_registered(id);
}

ecs_resource_t
ecs_resource_register(ecs_resource_t *id, const ecs_resource_desc_t *desc) {
        ecs_assert_not_null(id);
    if (*id == 0) {
        *id = ecs_resource_alloc_id();
    }
    return ecs_resource_index_register(*id, desc);
}

void ecs_set_resource_rid(ecs_resource_t id, const void *data) {
        ecs_assert_id_valid(id);
    ecs_assert_not_null(data);

    ecs_resource_index_set(id, data);
}

void ecs_move_resource_rid(ecs_resource_t id, void *data) {
        ecs_assert_id_valid(id);
    ecs_assert_not_null(data);

    ecs_resource_index_move(id, data);
}

void *ecs_resource_rid(ecs_resource_t id) {
        ecs_assert_id_valid(id);

    void *resource = ecs_resource_index_get(id);
    ecs_assert(resource != NULL, "resource does not exist: %d\n", id);
    return resource;
}

void *ecs_try_resource_rid(ecs_resource_t id) {
        ecs_assert_id_valid(id);

    return ecs_resource_index_get(id);
}

bool ecs_has_resource_rid(const ecs_resource_t id) {
        ecs_assert_id_valid(id);

    return ecs_resource_index_has(id);
}

void ecs_remove_resource_rid(ecs_resource_t id) {
        ecs_assert_id_valid(id);

    ecs_resource_index_remove(id);
}

#include <time.h>

#define ECS_SYSTEM_NO_QUERY UINT16_MAX

ecs_system_id_t ecs_system_init(const ecs_system_desc_t *desc) {
    ecs_assert_not_null(desc);
    ecs_assert(desc->callback, "system requires callback function\n");
    ecs_assert(desc->phase < EcsPhaseCount, "invalid system phase: %u\n", desc->phase);

    ecs_system_t sys = {
        .qid = desc->query.terms[0].id ? ecs_query_init(&desc->query) : ECS_SYSTEM_NO_QUERY,
        .callback = desc->callback,
        .user_data = desc->user_data,
        .user_data_dtor = desc->user_data_dtor,
        .phase = desc->phase,
        .enabled = !desc->disabled,
    };

    memcpy(sys.after, desc->after, sizeof(sys.after));

    ecs_system_id_t system = ecs_system_index_create(&sys);
    ecs_module_record_system(system);
    return system;
}

void ecs_run_system(ecs_system_id_t system) {

    ecs_system_t *sys = ecs_system_index_get(system);
    if (!sys->enabled) {
        return;
    }

    ecs_defer_begin();
    if (sys->qid != ECS_SYSTEM_NO_QUERY) {
        ecs_iter_t it = ecs_query_iter(sys->qid);
        it.user_data = sys->user_data;
        it.delta_time = ecs_world.delta_time;
        while (ecs_iter_next(&it)) {
            sys->callback(&it);
        }
    } else {
        ecs_iter_t it = {
            .count = 1,
            .user_data = sys->user_data,
            .delta_time = ecs_world.delta_time,
        };
        sys->callback(&it);
    }
    ecs_defer_end();
}

void ecs_run_phase(ecs_phase_t phase) {
    ecs_assert(phase < EcsPhaseCount, "invalid system phase: %u\n", phase);

    ecs_system_index_t *index = &ecs_world.system_index;
    if (index->plan_dirty) {
        ecs_system_index_build_plan();
    }

    sicore_vec_t *order = &index->phase_order[phase];
    for (uint32_t i = 0; i < order->size; i++) {
        ecs_system_id_t system = *sicore_vec_get(order, i, ecs_system_id_t);
        ecs_run_system(system);
    }
}

static inline double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static inline void sleep_sec(double seconds) {
    if (seconds <= 0.0)
        return;

    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);

    nanosleep(&ts, NULL);
}

bool ecs_progress(void) {
    double frame_start = now_sec();

    if (ecs_world.last_time == 0.0) {
        ecs_world.delta_time = 0.0;
    } else {
        ecs_world.delta_time = frame_start - ecs_world.last_time;
    }

    ecs_world.last_time = frame_start;

    if (!ecs_world.did_start) {
        ecs_run_phase(EcsPreStart);
        ecs_run_phase(EcsStart);
        ecs_run_phase(EcsPostStart);
        ecs_world.did_start = true;
    }

    for (ecs_phase_t phase = EcsOnLoad; phase < EcsPhaseCount; phase++) {
        ecs_run_phase(phase);
    }

    if (ecs_world.features.target_fps) {
        double target_dt = 1.0 / (double)ecs_world.features.target_fps;
        double elapsed = now_sec() - frame_start;
        double remaining = target_dt - elapsed;

        sleep_sec(remaining);
    }

    return !ecs_world.exit;
}

void ecs_run(void) {
    while (ecs_progress()) {
    }
    ecs_fini();
}

void ecs_system_enable(ecs_system_id_t system) {

    ecs_system_t *sys = ecs_system_index_get(system);
    if (sys->enabled == true) {
        return;
    }

    sys->enabled = true;
    ecs_world.system_index.plan_dirty = true;
}

void ecs_system_disable(ecs_system_id_t system) {

    ecs_system_t *sys = ecs_system_index_get(system);
    if (sys->enabled == false) {
        return;
    }

    sys->enabled = false;
    ecs_world.system_index.plan_dirty = true;
}

void ecs_table_init(ecs_table_t *table, ecs_type_t type, uint16_t table_id) {
    table->type = type;
    table->entity_capacity = 1;
    table->entity_count = 0;
    // data_count belongs to the canonical table layout, not to transient types.
    table->type.data_count = 0;
    table->entities = malloc(sizeof(ecs_entity_t) * table->entity_capacity);
    table->cls = type.count == 0 ? NULL : malloc(sizeof(ecs_column_t) * type.count);
    table->data_columns = type.count == 0 ? NULL : malloc(sizeof(uint16_t) * type.count);
    table->bloom = ecs_type_bloom(&type);

    sicore_vec_init(&table->observers_by_event, sizeof(sicore_vec_t));
    ecs_id_map_init(&table->add_edge);

    for (uint16_t i = 0; i < type.count; i++) {
        ecs_component_record_t *rec = ecs_component_index_get_mut(type.ids[i]);
        sicore_vec_push_u16(&rec->tables, table_id);
        table->cls[i].size = rec->size;
        table->cls[i].data = rec->size != 0 ? calloc(table->entity_capacity, rec->size) : NULL;
        if (rec->size != 0) {
            table->data_columns[table->type.data_count++] = i;
        }
        ecs_id_map_set(&table->add_edge, type.ids[i], i);
        table->cls[i].remove_edge = UINT16_MAX;
        table->cls[i].flags = 0;
        if (rec->size == 0 || (!rec->ops.move_ctor && !rec->ops.copy_ctor)) {
            table->cls[i].flags |= EcsColumnTrivialMove;
        }
        if (rec->size == 0 || !rec->ops.dtor) {
            table->cls[i].flags |= EcsColumnNoDtor;
        }
        if (rec->size == 0 || !rec->ops.ctor) {
            table->cls[i].flags |= EcsColumnZeroCtor;
        }
    }

    if (table->type.data_count == 0) {
        free(table->data_columns);
        table->data_columns = NULL;
    } else if (table->type.data_count < type.count) {
        table->data_columns =
            realloc(table->data_columns, sizeof(uint16_t) * table->type.data_count);
    }
}

static inline void ecs_table_grow(ecs_table_t *table) {
    uint64_t new_capacity = table->entity_capacity * (uint64_t)2;
    table->entities = realloc(table->entities, sizeof(ecs_entity_t) * new_capacity);
    for (uint16_t i = 0; i < table->type.data_count; i++) {
        uint16_t column_index = table->data_columns[i];
        ecs_column_t *column = &table->cls[column_index];

        if (column->flags & EcsColumnTrivialMove) {
            void *new_data = realloc(column->data, (size_t)new_capacity * column->size);
            ecs_assert_not_null(new_data);
            column->data = new_data;
            continue;
        }

        const ecs_component_record_t *record =
            ecs_component_index_get(table->type.ids[column_index]);
        void *new_data = malloc((size_t)new_capacity * column->size);
        ecs_assert_not_null(new_data);
        ecs_component_value_move_ctor(record, new_data, column->data, table->entity_count);
        free(column->data);
        column->data = new_data;
    }
    table->entity_capacity = new_capacity;
    ecs_query_index_refresh_table_fields(table, (uint16_t)(table - ecs_world.table_index.tables));
}

uint32_t ecs_table_add_entity(ecs_table_t *table, ecs_entity_t entity) {
    if (ECS_UNLIKELY(table->entity_count >= table->entity_capacity)) {
        ecs_table_grow(table);
    }
    uint32_t row = table->entity_count++;
    table->entities[row] = entity;
    return row;
}

// if the entity is not the last one, the last entity will be moved to the removed entity's
// position, and the moved entity will be returned
ecs_entity_t ecs_table_remove_entity(ecs_table_t *table, uint32_t row, bool row_values_live) {
    ecs_entity_t removed_entity = table->entities[row];
    uint32_t last_row = table->entity_count - 1;
    if (row_values_live) {
        for (uint16_t i = 0; i < table->type.data_count; i++) {
            uint16_t column_index = table->data_columns[i];
            ecs_table_dtor_column(table, column_index, row);
        }
    }
    if (row != last_row) {
        ecs_entity_t moved_entity = table->entities[last_row];
        table->entities[row] = moved_entity;
        for (uint16_t i = 0; i < table->type.data_count; i++) {
            uint16_t column_index = table->data_columns[i];
            ecs_table_move_column(table, column_index, last_row, table, column_index, row);
        }
        table->entity_count -= 1;
        return moved_entity;
    }
    table->entity_count -= 1;
    return removed_entity;
}

void *ecs_table_get_component(ecs_table_t *table, ecs_component_t component_id, uint32_t row) {
    return ecs_table_component_at_column(
        table,
        ecs_table_get_column_index(table, component_id),
        row
    );
}

void ecs_table_add_observer(ecs_table_t *table, uint16_t event, uint16_t observer_id) {
    sicore_vec_ensure(&table->observers_by_event, event + 1, sizeof(sicore_vec_t));
    sicore_vec_t *list = sicore_vec_get_mut(&table->observers_by_event, event, sicore_vec_t);
    if (list->capacity == 0) {
        sicore_vec_init(list, sizeof(uint16_t));
    }
    sicore_vec_push_u16(list, observer_id);
}

static void ecs_table_fini_component_values(ecs_table_t *table) {
    for (uint16_t c = 0; c < table->type.count; c++) {
        ecs_component_t component = table->type.ids[c];
        const ecs_component_record_t *crec = ecs_component_index_get(component);

        if (crec->relation_flags & EcsRelationSource) {
            for (uint32_t row = 0; row < table->entity_count; row++) {
                void *ptr = ecs_table_component_at_column(table, c, row);
                ecs_component_value_dtor(crec, ptr, 1);
            }
            continue;
        }

        if (crec->relation_flags & EcsRelationTarget) {
            continue;
        }

        for (uint32_t row = 0; row < table->entity_count; row++) {
            void *ptr = ecs_table_component_at_column(table, c, row);
            if (crec->on_remove) {
                crec->on_remove(table->entities[row], component, ptr);
            }
            ecs_component_value_dtor(crec, ptr, 1);
        }
    }
}

void ecs_table_fini(ecs_table_t *table) {
    ecs_table_fini_component_values(table);

    for (uint16_t i = 0; i < table->type.count; i++) {
        free(table->cls[i].data);
    }
    for (uint32_t e = 0; e < table->observers_by_event.size; e++) {
        sicore_vec_fini(sicore_vec_get_mut(&table->observers_by_event, e, sicore_vec_t));
    }
    sicore_vec_fini(&table->observers_by_event);
    ecs_id_map_fini(&table->add_edge);
    free(table->entities);
    free(table->cls);
    free(table->data_columns);
    ecs_type_fini(&table->type);
}

bool ecs_table_has(const ecs_table_t *table, ecs_component_t component_id) {
    if (ecs_table_column_or_invalid(table, component_id) != UINT16_MAX) {
        return true;
    }

    // Abstract is not inherited for query matching; only exclude tables that own it.
    if (component_id == ecs_id(Abstract)) {
        return false;
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);
        if (ecs_table_column_or_invalid(base_table, component_id) != UINT16_MAX) {
            return true;
        }
        base = base_table->type.base;
    }

    return false;
}

bool ecs_table_is_a(const ecs_table_t *table, ecs_entity_t base) {
    ecs_assert_entity_valid(base);

    ecs_entity_t current = table->type.base;
    while (current != 0) {
        if (current == base) {
            return true;
        }

        const ecs_entity_record_t *record = ecs_get_record(current);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);
        current = base_table->type.base;
    }

    return false;
}

void *ecs_table_field(const ecs_table_t *table, ecs_component_t component_id, bool *is_shared) {
    uint16_t cidx = ecs_table_column_or_invalid(table, component_id);
    if (cidx != UINT16_MAX) {
        *is_shared = false;
        return table->cls[cidx].data;
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);

        cidx = ecs_table_column_or_invalid(base_table, component_id);
        if (cidx != UINT16_MAX) {
            *is_shared = true;
            return ecs_table_component_at_column(base_table, cidx, record->table_row);
        }

        base = base_table->type.base;
    }

    *is_shared = false;
    return NULL;
}

void ecs_migrate_to_table(
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t fi = 0, ti = 0;
    while (fi < from_table->type.count && ti < to_table->type.count) {
        uint16_t fid = from_table->type.ids[fi];
        uint16_t tid = to_table->type.ids[ti];
        if (fid == tid) {
            ecs_table_move_column(from_table, fi, old_row, to_table, ti, new_row);
            fi++;
            ti++;
        } else if (fid < tid) {
            ecs_table_dtor_column(from_table, fi, old_row);
            fi++;
        } else {
            ecs_table_ctor_column(to_table, ti, new_row);
            ti++;
        }
    }
    for (; fi < from_table->type.count; fi++) {
        ecs_table_dtor_column(from_table, fi, old_row);
    }
    for (; ti < to_table->type.count; ti++) {
        ecs_table_ctor_column(to_table, ti, new_row);
    }

    ecs_table_finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
}

void *ecs_migrate_add(
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    const uint16_t to_table_id,
    const ecs_component_t added_id
) {
    const uint32_t old_row = record->table_row;
    const uint32_t new_row = ecs_table_add_entity(to_table, entity);

    const uint16_t k = ecs_table_get_column_index(to_table, added_id);
    ecs_table_ctor_column(to_table, k, new_row);

    uint16_t i = 0;
    for (; i < from_table->type.data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= k) {
            break;
        }
        ecs_table_move_column(from_table, from_col, old_row, to_table, from_col, new_row);
    }
    for (; i < from_table->type.data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        ecs_table_move_column(from_table, from_col, old_row, to_table, from_col + 1, new_row);
    }

    ecs_table_finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(to_table, k, new_row);
}

void *ecs_migrate_add_many(
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    const uint16_t to_table_id,
    const ecs_component_t requested_id
) {
    const uint32_t old_row = record->table_row;
    const uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t from_data = 0;
    for (uint16_t to_data = 0; to_data < to_table->type.data_count; to_data++) {
        const uint16_t to_col = to_table->data_columns[to_data];
        const ecs_component_t to_id = to_table->type.ids[to_col];

        while (from_data < from_table->type.data_count) {
            const uint16_t from_col = from_table->data_columns[from_data];
            const ecs_component_t from_id = from_table->type.ids[from_col];
            if (from_id >= to_id) {
                break;
            }
            from_data++;
        }

        if (from_data < from_table->type.data_count) {
            const uint16_t from_col = from_table->data_columns[from_data];
            if (from_table->type.ids[from_col] == to_id) {
                ecs_table_move_column(from_table, from_col, old_row, to_table, to_col, new_row);
                from_data++;
                continue;
            }
        }

        ecs_table_ctor_column(to_table, to_col, new_row);
    }

    ecs_table_finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(
        to_table,
        ecs_table_get_column_index(to_table, requested_id),
        new_row
    );
}

void ecs_migrate_remove(
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id,
    uint16_t col_idx
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t i = 0;
    for (; i < from_table->type.data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= col_idx) {
            break;
        }
        ecs_table_move_column(from_table, from_col, old_row, to_table, from_col, new_row);
    }
    if (i < from_table->type.data_count && from_table->data_columns[i] == col_idx) {
        ecs_table_dtor_column(from_table, col_idx, old_row);
        i++;
    }
    for (; i < from_table->type.data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        ecs_table_move_column(from_table, from_col, old_row, to_table, from_col - 1, new_row);
    }

    ecs_table_finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
}

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id) {
    ecs_type_t new_type = {
        .ids = malloc((type->count + 1) * sizeof(uint16_t)),
        .count = type->count + 1,
        .base = type->base,
    };

    uint16_t i = 0;
    while (i < type->count && type->ids[i] < id) {
        new_type.ids[i] = type->ids[i];
        i++;
    }
    new_type.ids[i] = id;
    if (i < type->count) {
        memcpy(&new_type.ids[i + 1], &type->ids[i], (type->count - i) * sizeof(uint16_t));
    }

    return new_type;
}

ecs_type_t ecs_type_with_remove_at(const ecs_type_t *type, uint16_t index) {
    ecs_type_t new_type = {
        .ids = malloc((type->count - 1) * sizeof(uint16_t)),
        .count = type->count - 1,
        .base = type->base,
    };
    if (index > 0) {
        memcpy(new_type.ids, type->ids, index * sizeof(uint16_t));
    }
    if (index + 1 < type->count) {
        memcpy(
            &new_type.ids[index],
            &type->ids[index + 1],
            (type->count - index - 1) * sizeof(uint16_t)
        );
    }
    return new_type;
}

ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base) {
    ecs_type_t new_type = {
        .ids = type->count == 0 ? NULL : malloc(type->count * sizeof(uint16_t)),
        .count = type->count,
        .base = base,
    };
    if (type->count != 0) {
        memcpy(new_type.ids, type->ids, type->count * sizeof(uint16_t));
    }
    return new_type;
}

void ecs_type_fini(ecs_type_t *type) {
    if (type->ids) {
        free(type->ids);
        type->ids = NULL;
    }
}

uint64_t ecs_type_bloom(const ecs_type_t *type) {
    uint64_t filter = 0;

    for (uint16_t i = 0; i < type->count; i++) {
        filter |= (1ull << (type->ids[i] % 64));
    }

    return filter;
}

ecs_world_t ecs_world;
static bool ecs_world_started;
static bool ecs_world_finished;

void ecs_init_w_features(const ecs_world_feat_desc_t *features) {
    ecs_assert(!ecs_world_started, "ecs_init called while ECS is already running\n");
    if (ecs_world_finished) {
        memset(&ecs_world, 0, sizeof ecs_world);
        ecs_world_finished = false;
    }
    ecs_world_started = true;

    ecs_entity_index_init();
    ecs_component_index_init();
    ecs_table_index_init();
    ecs_query_index_init();
    ecs_observer_index_init();
    ecs_system_index_init();
    ecs_module_index_init();
    ecs_resource_index_init();
    ecs_arena_init();
    ecs_command_buffer_init();
    ecs_world.active_module = 0;
    ecs_world.features = *features;
    ecs_world.defer_depth = 0;
    ecs_world.flushing_commands = false;
    ecs_world.did_start = false;
    ecs_world.exit = false;
    ecs_world.delta_time = 0;
    ecs_world.last_time = 0;
    ecs_bootstrap();
}

void ecs_init(void) { ecs_init_w_features(&(ecs_world_feat_desc_t){}); }

void ecs_fini(void) {
    ecs_assert(ecs_world_started && !ecs_world_finished, "ecs_fini called outside ECS lifetime\n");
    ecs_world_finished = true;

    ecs_resource_index_fini();
    ecs_table_index_fini();
    ecs_entity_index_fini();
    ecs_component_index_fini();
    ecs_query_index_fini();
    ecs_observer_index_fini();
    ecs_system_index_fini();
    ecs_module_index_fini();
    ecs_command_buffer_fini();
    ecs_arena_fini();
    ecs_world_started = false;
}

void ecs_quit(void) { ecs_world.exit = true; }

#define ECS_ARENA_INITIAL_CAPACITY 4096u

static ecs_arena_block_t *ecs_arena_block_new(uint32_t capacity) {
    ecs_arena_block_t *block = malloc(sizeof(ecs_arena_block_t) + capacity);
    *block = (ecs_arena_block_t){ .capacity = capacity };
    return block;
}

void ecs_arena_init() {
    ecs_arena_t *allocator = &ecs_world.arena_allocator;
    ecs_arena_block_t *block = ecs_arena_block_new(ECS_ARENA_INITIAL_CAPACITY);
    *allocator = (ecs_arena_t){
        .first = block,
        .current = block,
        .last = block,
    };
}

void *ecs_arena_alloc_slow(ecs_arena_t *allocator, uint32_t size) {
    for (ecs_arena_block_t *block = allocator->current->next; block; block = block->next) {
        if (size <= block->capacity) {
            allocator->current = block;
            block->cursor = size;
            return block->data;
        }
    }

    uint32_t capacity = allocator->last->capacity;
    if (capacity <= UINT32_MAX / 2u) {
        capacity *= 2u;
    }
    while (capacity < size) {
        if (capacity > UINT32_MAX / 2u) {
            capacity = size;
            break;
        }
        capacity *= 2u;
    }

    ecs_arena_block_t *block = ecs_arena_block_new(capacity);
    allocator->last->next = block;
    allocator->last = block;
    allocator->current = block;
    block->cursor = size;
    return block->data;
}

void ecs_arena_fini() {
    ecs_arena_t *allocator = &ecs_world.arena_allocator;
    ecs_arena_block_t *block = allocator->first;
    while (block) {
        ecs_arena_block_t *next = block->next;
        free(block);
        block = next;
    }
}

void ecs_id_map_init(ecs_id_map_t *map) {
    map->capacity = 1;
    map->ids = malloc(sizeof(uint16_t));
    *map->ids = UINT16_MAX;
}

void ecs_id_map_fini(ecs_id_map_t *map) { free(map->ids); }

void ecs_id_map_ensure(ecs_id_map_t *map, uint16_t id) {
    if (ECS_UNLIKELY(id >= map->capacity)) {
        uint16_t new_cap = map->capacity;
        while (new_cap <= id)
            new_cap *= 2;
        map->ids = realloc(map->ids, sizeof(uint16_t) * new_cap);
        memset(map->ids + map->capacity, 0xFF, sizeof(uint16_t) * (new_cap - map->capacity));
        map->capacity = new_cap;
    }
}

void ecs_component_index_register(
    ecs_component_t id,
    uint64_t size,
    ecs_type_ops_t ops,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    uint32_t relation_flags
) {
    sicore_vec_ensure(
        &ecs_world.component_index.components,
        (uint32_t)id + 1,
        sizeof(ecs_component_record_t)
    );

    ecs_component_record_t *existing =
        sicore_vec_get_mut(&ecs_world.component_index.components, id, ecs_component_record_t);
    if (existing->tables.data) {
        return;
    }

    ecs_component_info_t *info = malloc(sizeof *info);
    if (!info) {
        abort();
    }
    *info = (ecs_component_info_t){
        .size = size,
        .relation_flags = relation_flags,
    };

    ecs_component_record_t record = {
        .info = info,
        .required = NULL,
        .required_count = 0,
        .size = size,
        .ops = ops,
        .on_set = on_set,
        .on_remove = on_remove,
        .on_add = on_add,
        .relation_flags = relation_flags,
        .tables = { 0 },
    };
    sicore_vec_init(&record.tables, sizeof(uint16_t));

    *existing = record;
}

void ecs_component_index_init() {
    sicore_vec_init_w_size(
        &ecs_world.component_index.components,
        sizeof(ecs_component_record_t),
        256
    );
}

void ecs_component_index_fini() {
    ecs_component_record_t *records = ecs_world.component_index.components.data;

    for (uint32_t i = 0; i < ecs_world.component_index.components.size; i++) {
        if (records[i].info) {
            free(records[i].info);
        }
        free(records[i].required);
        sicore_vec_fini(&records[i].tables);
    }
    sicore_vec_fini(&ecs_world.component_index.components);
}

void ecs_component_value_ctor(const ecs_component_record_t *record, void *dst, uint32_t count) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.ctor) {
        record->ops.ctor(dst, count);
        return;
    }

    memset(dst, 0, (size_t)record->size * count);
}

void ecs_component_value_dtor(const ecs_component_record_t *record, void *ptr, uint32_t count) {
    if (record->size == 0 || count == 0 || !record->ops.dtor) {
        return;
    }

    record->ops.dtor(ptr, count);
}

void ecs_component_value_copy_ctor(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

void ecs_component_value_copy(
    const ecs_component_record_t *record,
    void *dst,
    const void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.copy) {
        record->ops.copy(dst, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

void ecs_component_value_move_ctor(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.move_ctor) {
        record->ops.move_ctor(dst, src, count);
        return;
    }
    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, count);
        ecs_component_value_dtor(record, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

void ecs_component_value_move(
    const ecs_component_record_t *record,
    void *dst,
    void *src,
    uint32_t count
) {
    if (record->size == 0 || count == 0) {
        return;
    }

    if (record->ops.move) {
        record->ops.move(dst, src, count);
        return;
    }
    if (record->ops.copy) {
        record->ops.copy(dst, src, count);
        ecs_component_value_dtor(record, src, count);
        return;
    }

    memcpy(dst, src, (size_t)record->size * count);
}

bool ecs_entity_index_is_alive(ecs_entity_t entity) {
    return ecs_entity_index_get_record(ecs_first(entity))->generation == ecs_second(entity);
}

void ecs_entity_index_init() {
    ecs_entity_index_t *index = &ecs_world.entity_index;
    sicore_vec_init_w_size(&index->entities, sizeof(ecs_entity_record_t), 256);
    index->first_available = UINT32_MAX;
}

void ecs_entity_index_fini() { sicore_vec_fini(&ecs_world.entity_index.entities); }

static bool ecs_module_id_valid(const ecs_module_index_t *index, ecs_module_id_t module) {
    return module != 0 && module < index->modules.size;
}

static void ecs_module_record_init(
    ecs_module_t *module,
    ecs_module_id_t *id
) {
    module->id = id;
    module->enabled = true;
    sicore_vec_init(&module->observers, sizeof(ecs_observer_id_t));
    sicore_vec_init(&module->systems, sizeof(ecs_system_id_t));
}

static void ecs_module_record_fini(ecs_module_t *module) {
    if (module->id) {
        *module->id = 0;
    }

    sicore_vec_fini(&module->observers);
    sicore_vec_fini(&module->systems);
}

void ecs_module_index_init() {
    ecs_module_index_t *index = &ecs_world.module_index;
    sicore_vec_init(&index->modules, sizeof(ecs_module_t));
    sicore_vec_ensure(&index->modules, 1, sizeof(ecs_module_t));
}

void ecs_module_index_fini() {
    ecs_module_index_t *index = &ecs_world.module_index;
    for (uint32_t i = 1; i < index->modules.size; i++) {
        ecs_module_t *module = sicore_vec_get_mut(&index->modules, i, ecs_module_t);
        ecs_module_record_fini(module);
    }
    sicore_vec_fini(&index->modules);
}

ecs_module_id_t ecs_module_index_create(
    ecs_module_id_t *id
) {
    ecs_module_index_t *index = &ecs_world.module_index;
    ecs_module_t module;
    ecs_module_record_init(
        &module,
        id
    );
    sicore_vec_push(&index->modules, &module, sizeof(ecs_module_t));
    return index->modules.size - 1;
}

ecs_module_t *ecs_module_index_get(ecs_module_id_t module) {
    ecs_module_index_t *index = &ecs_world.module_index;
    ecs_assert(ecs_module_id_valid(index, module), "invalid module id: %u\n", module);
    return sicore_vec_get_mut(&index->modules, module, ecs_module_t);
}

const ecs_module_t *ecs_module_index_get_const(ecs_module_id_t module) {
    const ecs_module_index_t *index = &ecs_world.module_index;
    ecs_assert(ecs_module_id_valid(index, module), "invalid module id: %u\n", module);
    return sicore_vec_get(&index->modules, module, ecs_module_t);
}

ecs_module_id_t ecs_module_index_find(const ecs_module_id_t *id) {
    const ecs_module_index_t *index = &ecs_world.module_index;
    if (!id || !*id) {
        return 0;
    }

    ecs_module_id_t module = *id;
    if (ecs_module_id_valid(index, module)) {
        return module;
    }

    return 0;
}

#define ECS_BUILTIN_EVENT_COUNT 3 // EcsOnAdd, EcsOnRemove, EcsOnSet

void ecs_observer_index_init() {
    ecs_observer_index_t *index = &ecs_world.observer_index;
    sicore_vec_init(&index->observers, sizeof(ecs_observer_t));
    index->event_count = ECS_BUILTIN_EVENT_COUNT;
}

void ecs_observer_index_fini() {
    ecs_observer_index_t *index = &ecs_world.observer_index;
    for (uint32_t i = 0; i < index->observers.size; i++) {
        ecs_observer_t *obs = sicore_vec_get_mut(&index->observers, i, ecs_observer_t);
        ecs_query_index_destroy(&obs->query);
    }
    sicore_vec_fini(&index->observers);
}

uint16_t ecs_observer_index_create(const ecs_observer_desc_t *desc) {
    ecs_observer_index_t *index = &ecs_world.observer_index;
    ecs_observer_t *obs = sicore_vec_push_empty(&index->observers, sizeof(ecs_observer_t));
    obs->event = desc->on;
    obs->callback = desc->callback;
    obs->user_data = desc->user_data;
    obs->enabled = true;
    ecs_query_from_desc(&desc->query, &obs->query);
    return index->observers.size - 1;
}

void ecs_observer_index_match_tables(
    ecs_table_t *tables,
    uint16_t table_count,
    uint16_t observer_id
) {
    ecs_observer_t *obs =
        sicore_vec_get_mut(&ecs_world.observer_index.observers, observer_id, ecs_observer_t);
    for (uint16_t i = 0; i < table_count; i++) {
        if (ecs_query_match_table(&obs->query, &tables[i])) {
            ecs_table_add_observer(&tables[i], obs->event, observer_id);
        }
    }
}

void ecs_observer_index_add_table(ecs_table_t *table) {
    for (uint32_t i = 0; i < ecs_world.observer_index.observers.size; i++) {
        ecs_observer_t *obs =
            sicore_vec_get_mut(&ecs_world.observer_index.observers, i, ecs_observer_t);
        if (ecs_query_match_table(&obs->query, table)) {
            ecs_table_add_observer(table, obs->event, i);
        }
    }
}

void ecs_query_index_init() {
    ecs_query_index_t *index = &ecs_world.query_index;
    sicore_vec_init(&index->queries, sizeof(ecs_query_cache_t));
    sicore_vec_init(&index->active_ids, sizeof(ecs_query_id_t));
    index->first_free = UINT16_MAX;
}

void ecs_query_index_fini() {
    ecs_query_index_t *index = &ecs_world.query_index;
    const ecs_query_id_t *active_ids = index->active_ids.data;
    for (uint32_t i = 0; i < index->active_ids.size; i++) {
        ecs_query_cache_t *cache =
            sicore_vec_get_mut(&index->queries, active_ids[i], ecs_query_cache_t);
        sicore_vec_fini(&cache->table_ids);
        free(cache->fields_ptr);
        free(cache->field_kind_bits);
        ecs_query_index_destroy(&cache->query);
    }
    sicore_vec_fini(&index->active_ids);
    sicore_vec_fini(&index->queries);
}

void ecs_query_index_destroy(ecs_query_t *query) { free(query->terms); }

static uint16_t ecs_query_count_terms(const ecs_query_term_t *terms) {
    uint16_t i = 0;
    while (terms[i].id) {
        i++;
    }
    return i;
}

static ecs_query_term_t *ecs_query_copy_terms(const ecs_query_term_t *terms, uint16_t count) {
    if (count == 0) {
        return NULL;
    }
    ecs_query_term_t *copy = malloc(sizeof(ecs_query_term_t) * count);
    memcpy(copy, terms, sizeof(ecs_query_term_t) * count);
    return copy;
}

static bool
ecs_query_has_term(const ecs_query_term_t *terms, uint16_t term_count, ecs_component_t id) {
    for (uint16_t i = 0; i < term_count; i++) {
        if (terms[i].id == id) {
            return true;
        }
    }
    return false;
}

static ecs_query_term_t *
ecs_query_copy_terms_with_implicit_excludes(const ecs_query_term_t *terms, uint16_t *count) {
    const ecs_component_t excludes[] = {
        ecs_id(Disabled),
        ecs_id(Abstract),
    };
    const uint16_t explicit_count = *count;
    uint16_t exclude_count = 0;

    for (uint32_t i = 0; i < sizeof(excludes) / sizeof(excludes[0]); i++) {
        if (excludes[i] && !ecs_query_has_term(terms, explicit_count, excludes[i])) {
            exclude_count++;
        }
    }

    if (exclude_count == 0) {
        return ecs_query_copy_terms(terms, *count);
    }

    ecs_assert(*count + exclude_count < 16, "query has no room for implicit exclude terms\n");

    ecs_query_term_t *copy = malloc(sizeof(ecs_query_term_t) * (*count + exclude_count));
    if (*count != 0) {
        memcpy(copy, terms, sizeof(ecs_query_term_t) * *count);
    }

    for (uint32_t i = 0; i < sizeof(excludes) / sizeof(excludes[0]); i++) {
        if (excludes[i] && !ecs_query_has_term(terms, explicit_count, excludes[i])) {
            copy[*count] = (ecs_query_term_t){ .id = excludes[i], .access = EcsNot };
            *count += 1;
        }
    }

    return copy;
}

static bool ecs_query_term_is_field(ecs_query_term_t term) {
    return term.access == EcsIn || term.access == EcsOut || term.access == EcsInOut ||
           term.access == EcsInOptional || term.access == EcsInOutOptional;
}

static bool ecs_query_term_is_positive(ecs_query_term_t term) {
    return term.access == EcsIn || term.access == EcsOut || term.access == EcsInOut ||
           term.access == EcsFilter;
}

static void ecs_query_validate_terms(const ecs_query_term_t *terms, uint16_t term_count) {
    for (uint16_t i = 0; i < term_count; i++) {
        ecs_assert_id_valid(terms[i].id);
        ecs_assert(
            terms[i].access == EcsIn || terms[i].access == EcsOut || terms[i].access == EcsInOut ||
                terms[i].access == EcsInOptional || terms[i].access == EcsInOutOptional ||
                terms[i].access == EcsFilter || terms[i].access == EcsNot,
            "invalid query term access: %d\n",
            terms[i].access
        );

        for (uint16_t j = i + 1; j < term_count; j++) {
            ecs_assert(
                terms[i].id != terms[j].id,
                "duplicate query term component: %d\n",
                terms[i].id
            );
        }
    }
}

void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query) {
    query->term_count = ecs_query_count_terms(desc->terms);

    query->terms = ecs_query_copy_terms_with_implicit_excludes(desc->terms, &query->term_count);
    ecs_query_validate_terms(query->terms, query->term_count);

    query->is_a = desc->is_a;

    query->field_count = 0;
    query->field_mask = 0;
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_field(query->terms[i])) {
            query->field_mask |= (uint16_t)(1u << i);
            query->field_count++;
        }
    }

    query->bloom = 0;
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_positive(query->terms[i])) {
            query->bloom |= 1ull << (query->terms[i].id % 64);
        }
    }
}

static void ecs_query_cache_set_table_fields(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_index
) {
    const uint16_t field_count = cache->query.field_count;
    const uint32_t base = (uint32_t)table_index * field_count;
    uint16_t remaining_fields = cache->query.field_mask;
    uint16_t field_index = 0;
    uint32_t field_kind_bits = 0;

    while (remaining_fields) {
        const uint16_t term_index = (uint16_t)__builtin_ctz((unsigned)remaining_fields);
        remaining_fields &= (uint16_t)(remaining_fields - 1);
        const ecs_query_term_t term = cache->query.terms[term_index];
        void *field_ptr = NULL;
        ecs_field_kind_t field_kind = EcsFieldNone;

        if (ecs_query_term_requires_owned(term)) {
            uint16_t column = ecs_table_column_or_invalid(table, term.id);
            if (column != UINT16_MAX) {
                field_ptr = table->cls[column].data;
                field_kind = EcsFieldOwned;
            }
        } else {
            bool is_shared = false;
            field_ptr = ecs_table_field(table, term.id, &is_shared);
            if (field_ptr || is_shared) {
                field_kind = is_shared ? EcsFieldShared : EcsFieldOwned;
            }
        }

        ecs_assert(
            field_kind != EcsFieldNone || term.access == EcsInOptional ||
                term.access == EcsInOutOptional,
            "query cache matched table without field component: %d\n",
            term.id
        );

        cache->fields_ptr[base + field_index] = field_ptr;
        field_kind_bits |= (uint32_t)field_kind << (field_index * 2);
        field_index++;
    }

    cache->field_kind_bits[table_index] = field_kind_bits;
}

static void
ecs_query_cache_add_table(ecs_query_cache_t *cache, const ecs_table_t *table, uint16_t table_id) {
    sicore_vec_push_u16(&cache->table_ids, table_id);
    const uint16_t table_count = cache->table_ids.size;
    const uint16_t field_count = cache->query.field_count;

    if (table_count > cache->field_table_capacity) {
        uint16_t capacity = cache->field_table_capacity ? cache->field_table_capacity : 4;
        while (capacity < table_count) {
            capacity *= 2;
        }

        const uint32_t slot_count = (uint32_t)capacity * field_count;
        cache->fields_ptr = realloc(cache->fields_ptr, sizeof(void *) * slot_count);
        if (field_count != 0) {
            cache->field_kind_bits = realloc(cache->field_kind_bits, sizeof(uint32_t) * capacity);
        }
        cache->field_table_capacity = capacity;
    }

    if (field_count != 0) {
        ecs_query_cache_set_table_fields(cache, table, table_count - 1);
    }
}

ecs_query_id_t ecs_query_index_create(const ecs_query_desc_t *desc) {
    ecs_query_index_t *index = &ecs_world.query_index;
    ecs_query_id_t id;
    ecs_query_cache_t *query_cache;

    if (index->first_free != UINT16_MAX) {
        id = index->first_free;
        query_cache = sicore_vec_get_mut(&index->queries, id, ecs_query_cache_t);
        index->first_free = query_cache->next_free;
    } else {
        query_cache = sicore_vec_push_empty(&index->queries, sizeof(ecs_query_cache_t));
        id = index->queries.size - 1;
    }

    ecs_query_from_desc(desc, &query_cache->query);
    sicore_vec_init(&query_cache->table_ids, sizeof(uint16_t));
    query_cache->fields_ptr = NULL;
    query_cache->field_kind_bits = NULL;
    query_cache->field_table_capacity = 0;
    query_cache->active_index = index->active_ids.size;
    query_cache->next_free = UINT16_MAX;
    query_cache->alive = true;
    sicore_vec_push_u16(&index->active_ids, id);

    return id;
}

static ecs_component_t ecs_query_rarest_positive_term(const ecs_query_t *query) {
    ecs_component_t rarest = 0;
    uint32_t rarest_table_count = UINT32_MAX;

    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_positive(query->terms[i])) {
            const ecs_component_t component = query->terms[i].id;
            const uint32_t table_count = ecs_component_index_get(component)->tables.size;
            if (table_count < rarest_table_count) {
                rarest = component;
                rarest_table_count = table_count;
                if (table_count == 0) {
                    break;
                }
            }
        }
    }

    return rarest;
}

void ecs_query_index_update_matches(ecs_query_cache_t *query_cache) {
    uint16_t component = ecs_query_rarest_positive_term(&query_cache->query);

    if (ECS_LIKELY(component)) {
        const sicore_vec_t *tables_vec = &ecs_component_index_get(component)->tables;

        sicore_vec_iter(tables_vec, uint16_t, table_index, {
            const ecs_table_t *table = &ecs_world.table_index.tables[*table_index];

            if (ecs_query_match_table(&query_cache->query, table)) {
                ecs_query_cache_add_table(query_cache, table, *table_index);
            }
        });
    } else {
        const uint16_t table_count = ecs_world.table_index.table_count;
        const ecs_table_t *tables = ecs_world.table_index.tables;

        for (uint16_t i = 0; i < table_count; i++) {
            if (ecs_query_match_table(&query_cache->query, &tables[i])) {
                ecs_query_cache_add_table(query_cache, &tables[i], i);
            }
        }
    }
}

void ecs_query_index_add_table(const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = ecs_world.query_index.active_ids.data;
    for (uint32_t i = 0; i < ecs_world.query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            sicore_vec_get_mut(&ecs_world.query_index.queries, active_ids[i], ecs_query_cache_t);
        if (ecs_query_match_table(&cache->query, table)) {
            ecs_query_cache_add_table(cache, table, table_id);
        }
    }
}

void ecs_query_index_refresh_table_fields(const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = ecs_world.query_index.active_ids.data;

    for (uint32_t i = 0; i < ecs_world.query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            sicore_vec_get_mut(&ecs_world.query_index.queries, active_ids[i], ecs_query_cache_t);
        if (cache->query.field_count == 0) {
            continue;
        }

        const uint16_t *table_ids = cache->table_ids.data;
        for (uint16_t table_index = 0; table_index < cache->table_ids.size; table_index++) {
            if (table_ids[table_index] == table_id) {
                ecs_query_cache_set_table_fields(cache, table, table_index);
                break;
            }
        }
    }
}

static uint64_t ecs_resource_storage_size(const ecs_resource_record_t *record) {
    return record->size ? record->size : 1;
}

static void ecs_resource_value_copy_ctor(
    const ecs_resource_record_t *record,
    void *dst,
    const void *src
) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, 1);
        return;
    }
    memcpy(dst, src, record->size);
}

static void
ecs_resource_value_copy(const ecs_resource_record_t *record, void *dst, const void *src) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.copy) {
        record->ops.copy(dst, src, 1);
        return;
    }
    memcpy(dst, src, record->size);
}

static void ecs_resource_value_move_ctor(ecs_resource_record_t *record, void *dst, void *src) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.move_ctor) {
        record->ops.move_ctor(dst, src, 1);
        return;
    }
    if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, 1);
        if (record->ops.dtor) {
            record->ops.dtor(src, 1);
        }
        return;
    }
    memcpy(dst, src, record->size);
}

static void ecs_resource_value_move(ecs_resource_record_t *record, void *dst, void *src) {
    if (record->size == 0) {
        return;
    }
    if (record->ops.move) {
        record->ops.move(dst, src, 1);
        return;
    }
    if (record->ops.copy) {
        record->ops.copy(dst, src, 1);
        if (record->ops.dtor) {
            record->ops.dtor(src, 1);
        }
        return;
    }
    memcpy(dst, src, record->size);
}

static void ecs_resource_value_dtor(const ecs_resource_record_t *record, void *ptr) {
    if (record->size != 0 && record->ops.dtor) {
        record->ops.dtor(ptr, 1);
    }
}

static bool
ecs_resource_index_registered(const ecs_resource_index_t *index, ecs_resource_t id) {
    return index->registered[id];
}

static void
ecs_resource_index_assert_registered(const ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_assert(
        id != 0 && id < index->count && id < index->capacity &&
            ecs_resource_index_registered(index, id),
        "invalid resource id: %d\n",
        id
    );
}

static void ecs_resource_index_ensure(ecs_resource_index_t *index, ecs_resource_t id) {
    if (id < index->capacity) {
        return;
    }

    uint64_t capacity = index->capacity ? index->capacity : 16;
    while (id >= capacity) {
        capacity *= 2;
    }

    ecs_resource_record_t *records =
        realloc(index->records, sizeof(ecs_resource_record_t) * capacity);
    ecs_assert_not_null(records);
    void **data = realloc(index->data, sizeof(void *) * capacity);
    ecs_assert_not_null(data);
    bool *registered = realloc(index->registered, sizeof(bool) * capacity);
    ecs_assert_not_null(registered);
    bool *present = realloc(index->present, sizeof(bool) * capacity);
    ecs_assert_not_null(present);

    for (uint64_t i = index->capacity; i < capacity; i++) {
        records[i] = (ecs_resource_record_t){ 0 };
        data[i] = NULL;
        registered[i] = false;
        present[i] = false;
    }

    index->records = records;
    index->data = data;
    index->registered = registered;
    index->present = present;
    index->capacity = capacity;
}

void ecs_resource_index_init() {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    index->records = NULL;
    index->data = NULL;
    index->registered = NULL;
    index->present = NULL;
    index->capacity = 0;
    index->count = 1;
}

void ecs_resource_index_fini() {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    for (uint64_t id = 1; id < index->capacity; id++) {
        if (!index->present[id]) {
            continue;
        }

        const ecs_resource_record_t *record = &index->records[id];
        if (record->on_remove) {
            record->on_remove(index->data[id]);
        }
        ecs_resource_value_dtor(record, index->data[id]);
        free(index->data[id]);
    }

    free(index->records);
    free(index->data);
    free(index->registered);
    free(index->present);
}

ecs_resource_t
ecs_resource_index_register(ecs_resource_t id, const ecs_resource_desc_t *desc) {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_assert_not_null(desc);
    ecs_assert_id_valid(id);

    ecs_resource_index_ensure(index, id);
    if (ecs_resource_index_registered(index, id)) {
        return id;
    }
    index->records[id] = (ecs_resource_record_t){
        .size = desc->size,
        .ops = desc->ops,
        .on_set = desc->on_set,
        .on_remove = desc->on_remove,
    };
    index->registered[id] = true;
    if (id >= index->count) {
        index->count = (uint64_t)id + 1;
    }
    return id;
}

bool ecs_resource_index_is_registered(ecs_resource_t id) {
    const ecs_resource_index_t *index = &ecs_world.resource_index;
    return id != 0 && id < index->count && id < index->capacity &&
        ecs_resource_index_registered(index, id);
}

void ecs_resource_index_set(
    ecs_resource_t id,
    const void *data
) {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_resource_index_assert_registered(index, id);

    const ecs_resource_record_t *record = &index->records[id];
    bool was_present = index->present[id];
    if (!index->present[id]) {
        index->data[id] = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(index->data[id]);
        index->present[id] = true;
    }

    if (record->on_set) {
        record->on_set(data);
    }

    if (record->size != 0) {
        if (was_present) {
            ecs_resource_value_copy(record, index->data[id], data);
        } else {
            ecs_resource_value_copy_ctor(record, index->data[id], data);
        }
    }
}

void ecs_resource_index_move(
    ecs_resource_t id,
    void *data
) {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_resource_index_assert_registered(index, id);

    ecs_resource_record_t *record = &index->records[id];
    bool was_present = index->present[id];
    if (!index->present[id]) {
        index->data[id] = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(index->data[id]);
        index->present[id] = true;
    }

    if (record->on_set) {
        record->on_set(data);
    }

    if (record->size != 0) {
        if (was_present) {
            ecs_resource_value_move(record, index->data[id], data);
        } else {
            ecs_resource_value_move_ctor(record, index->data[id], data);
        }
    }
}

void *ecs_resource_index_get(ecs_resource_t id) {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_resource_index_assert_registered(index, id);
    if (!index->present[id]) {
        return NULL;
    }

    return index->data[id];
}

bool ecs_resource_index_has(ecs_resource_t id) {
    const ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_resource_index_assert_registered(index, id);
    return index->present[id];
}

void ecs_resource_index_remove(ecs_resource_t id) {
    ecs_resource_index_t *index = &ecs_world.resource_index;
    ecs_resource_index_assert_registered(index, id);
    if (!index->present[id]) {
        return;
    }

    const ecs_resource_record_t *record = &index->records[id];
    if (record->on_remove) {
        record->on_remove(index->data[id]);
    }
    ecs_resource_value_dtor(record, index->data[id]);

    free(index->data[id]);
    index->data[id] = NULL;
    index->present[id] = false;
}

static bool ecs_system_id_valid(const ecs_system_index_t *index, ecs_system_id_t system) {
    return system != 0 && system < index->systems.size;
}

static void ecs_system_index_plan_one(
    ecs_system_index_t *index,
    ecs_system_id_t system,
    uint8_t *state,
    sicore_vec_t *order
) {
    if (!ecs_system_id_valid(index, system)) {
        ecs_assert(false, "invalid system dependency: %u\n", system);
        return;
    }

    if (state[system] == 2) {
        return;
    }

    if (state[system] == 1) {
        ecs_assert(false, "system dependency cycle detected at system %u\n", system);
        return;
    }

    state[system] = 1;

    ecs_system_t *sys = ecs_system_index_get(system);
    for (uint32_t i = 0; i < ECS_SYSTEM_AFTER_CAPACITY; i++) {
        ecs_system_id_t after = sys->after[i];
        if (after == 0) {
            continue;
        }

        if (!ecs_system_id_valid(index, after)) {
            ecs_assert(false, "invalid system dependency: %u\n", after);
            continue;
        }

        ecs_system_t *dep = ecs_system_index_get(after);
        if (dep->phase != sys->phase) {
            ecs_assert(false, "system dependency must be in the same phase\n");
            continue;
        }

        ecs_system_index_plan_one(index, after, state, order);
    }

    state[system] = 2;

    if (sys->enabled) {
        sicore_vec_push_u16(order, system);
    }
}

void ecs_system_index_init() {
    ecs_system_index_t *index = &ecs_world.system_index;
    sicore_vec_init(&index->systems, sizeof(ecs_system_t));
    sicore_vec_ensure(&index->systems, 1, sizeof(ecs_system_t));

    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        sicore_vec_init(&index->phase_order[i], sizeof(ecs_system_id_t));
    }

    index->plan_dirty = true;
}

ecs_system_id_t ecs_system_index_create(const ecs_system_t *system) {
    ecs_system_index_t *index = &ecs_world.system_index;
    sicore_vec_push(&index->systems, system, sizeof(ecs_system_t));
    index->plan_dirty = true;
    return index->systems.size - 1;
}

ecs_system_t *ecs_system_index_get(ecs_system_id_t system) {
    ecs_system_index_t *index = &ecs_world.system_index;
    ecs_assert(ecs_system_id_valid(index, system), "invalid system id: %u\n", system);
    return sicore_vec_get_mut(&index->systems, system, ecs_system_t);
}

void ecs_system_index_build_plan() {
    ecs_system_index_t *index = &ecs_world.system_index;
    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        sicore_vec_clear(&index->phase_order[i]);
    }

    uint8_t *state = calloc(index->systems.size, sizeof(uint8_t));
    ecs_assert_not_null(state);

    for (uint32_t system = 1; system < index->systems.size; system++) {
        ecs_system_t *sys = ecs_system_index_get(system);
        ecs_assert(sys->phase < EcsPhaseCount, "invalid system phase: %u\n", sys->phase);

        if (sys->phase >= EcsPhaseCount) {
            continue;
        }

        ecs_system_index_plan_one(index, system, state, &index->phase_order[sys->phase]);
    }

    free(state);
    index->plan_dirty = false;
}

void ecs_system_index_fini() {
    ecs_system_index_t *index = &ecs_world.system_index;
    ecs_system_t *systems = sicore_vec_data(&index->systems, ecs_system_t);
    for (uint32_t i = 1; i < index->systems.size; i++) {
        if (systems[i].user_data_dtor) {
            systems[i].user_data_dtor(systems[i].user_data);
        }
    }

    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        sicore_vec_fini(&index->phase_order[i]);
    }

    sicore_vec_fini(&index->systems);
}

#define INITIAL_SLOT_SHIFT 12
#define LOAD_FACTOR 0.75
#define ECS_TABLE_SLOT_EMPTY UINT16_MAX

static inline uint32_t ecs_type_hash(ecs_type_t type) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < type.count; ++i) {
        h ^= (uint32_t)type.ids[i];
        h *= 16777619u;
    }
    h ^= (uint32_t)type.base;
    h *= 16777619u;
    h ^= (uint32_t)(type.base >> 32);
    h *= 16777619u;

    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

static inline uint16_t ecs_type_hash_fingerprint(uint32_t hash) {
    return (uint16_t)(hash ^ (hash >> 16));
}

static inline uint32_t ecs_table_index_slot_count(const ecs_table_index_t *map) {
    return 1u << map->slot_shift;
}

static inline void ecs_table_index_init_slots(ecs_table_index_t *map) {
    uint32_t slot_count = ecs_table_index_slot_count(map);
    map->slots = malloc(sizeof(ecs_type_slot_t) * slot_count);
    memset(map->slots, 0xFF, sizeof(ecs_type_slot_t) * slot_count);
}

static inline void
ecs_table_index_insert_slot(ecs_table_index_t *map, uint32_t hash, uint16_t table_index) {
    uint32_t slot_mask = ecs_table_index_slot_count(map) - 1;
    uint32_t slot_idx = hash & slot_mask;
    while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
        slot_idx = (slot_idx + 1) & slot_mask;
    }
    map->slots[slot_idx].hash = ecs_type_hash_fingerprint(hash);
    map->slots[slot_idx].table_index = table_index;
}

void ecs_table_index_init() {
    ecs_table_index_t *map = &ecs_world.table_index;
    map->table_count = 0;
    map->table_capacity = 1;
    map->tables = malloc(sizeof(ecs_table_t) * map->table_capacity);
    map->slot_shift = INITIAL_SLOT_SHIFT;
    ecs_table_index_init_slots(map);
}

void ecs_table_index_fini() {
    ecs_table_index_t *map = &ecs_world.table_index;
    for (uint16_t i = 0; i < map->table_count; i++) {
        ecs_table_fini(&map->tables[i]);
    }
    free(map->tables);
    free(map->slots);
}

static void ecs_table_index_resize(ecs_table_index_t *map) {
    ecs_type_slot_t *old_slots = map->slots;

    map->slot_shift += 1;
    ecs_table_index_init_slots(map);
    for (uint16_t i = 0; i < map->table_count; ++i) {
        // Table-owned types retain their full hash, so resize never scans their ids again.
        ecs_table_index_insert_slot(map, map->tables[i].type.hash, i);
    }
    free(old_slots);
}

static void ecs_table_index_grow_tables(ecs_table_index_t *map) {
    map->table_capacity *= 2;
    map->tables = realloc(map->tables, sizeof(ecs_table_t) * map->table_capacity);
}

static bool ecs_table_index_inherits_component_before(
    const ecs_table_t *table,
    ecs_entity_t stop_base,
    ecs_component_t component
) {
    ecs_entity_t base = table->type.base;
    while (base != 0 && base != stop_base) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);
        if (ecs_table_column_or_invalid(base_table, component) != UINT16_MAX) {
            return true;
        }
        base = base_table->type.base;
    }
    return false;
}

static void ecs_table_index_register_inherited_components(ecs_table_t *table, uint16_t table_id) {
    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);

        for (uint16_t i = 0; i < base_table->type.count; i++) {
            ecs_component_t component = base_table->type.ids[i];
            if (ecs_table_column_or_invalid(table, component) != UINT16_MAX ||
                ecs_table_index_inherits_component_before(table, base, component)) {
                continue;
            }

            table->bloom |= 1ull << (component % 64);
            ecs_component_record_t *record = ecs_component_index_get_mut(component);
            sicore_vec_push_u16(&record->tables, table_id);
        }

        base = base_table->type.base;
    }
}

uint16_t ecs_table_index_get_or_create(ecs_type_t type) {
    ecs_table_index_t *map = &ecs_world.table_index;
    uint32_t hash = ecs_type_hash(type);
    uint16_t hash_fingerprint = ecs_type_hash_fingerprint(hash);
    uint32_t slot_mask = ecs_table_index_slot_count(map) - 1;
    uint32_t slot_idx = hash & slot_mask;

    // Fast path: lookup
    while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
        if (ECS_LIKELY(map->slots[slot_idx].hash == hash_fingerprint)) {
            const ecs_table_t *table = ecs_table_index_at(map->slots[slot_idx].table_index);
            if (ECS_LIKELY(ecs_type_equals(&table->type, &type))) {
                ecs_type_fini(&type);
                return (uint16_t)map->slots[slot_idx].table_index;
            }
        }
        slot_idx = (slot_idx + 1) & slot_mask;
    }

    // Slow path: creation
    if (ECS_UNLIKELY(map->table_count >= ecs_table_index_slot_count(map) * LOAD_FACTOR)) {
        ecs_table_index_resize(map);
        // Recalculate and probe again: rehashing existing tables may occupy the
        // new ideal slot even though the old table had an empty slot there.
        slot_mask = ecs_table_index_slot_count(map) - 1;
        slot_idx = hash & slot_mask;
        while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
            slot_idx = (slot_idx + 1) & slot_mask;
        }
    }
    if (ECS_UNLIKELY(map->table_count >= map->table_capacity)) {
        ecs_table_index_grow_tables(map);
    }

    uint16_t table_idx = map->table_count++;
    ecs_table_t new_table;
    // Persist the hash while transferring ownership of the type to the table.
    type.hash = hash;
    ecs_table_init(&new_table, type, table_idx);
    map->tables[table_idx] = new_table;
    ecs_table_index_register_inherited_components(&map->tables[table_idx], table_idx);

    map->slots[slot_idx].hash = hash_fingerprint;
    map->slots[slot_idx].table_index = table_idx;

    ecs_query_index_add_table(ecs_table_index_at(table_idx), table_idx);
    ecs_observer_index_add_table(ecs_table_index_at(table_idx));
    return (uint16_t)table_idx;
}

