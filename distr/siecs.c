#include "siecs.h"

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
#if defined(_MSC_VER)
    v = _byteswap_uint64(v);
#else
    v = __builtin_bswap64(v);
#endif
#endif
    return v;
}

static inline uint64_t sicore_read32(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#if defined(_MSC_VER)
    v = _byteswap_ulong((unsigned long)v);
#else
    v = __builtin_bswap32(v);
#endif
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

#ifndef NDEBUG
#include <stdio.h>
#include <stdlib.h>

void sireflect_assert_fail(
    const char *condition,
    const char *message,
    const char *file,
    int line,
    const char *function
) {
    fprintf(stderr, "sireflect assertion failed: %s\n", message != NULL ? message : condition);
    fprintf(stderr, "  condition: %s\n", condition != NULL ? condition : "(unknown)");
    fprintf(stderr, "  location: %s:%d\n", file != NULL ? file : "(unknown)", line);
    fprintf(stderr, "  function: %s\n", function != NULL ? function : "(unknown)");
    abort();
}
#endif

#ifndef SIREFLECT_ERROR_H
#define SIREFLECT_ERROR_H

void sireflect_error_clear(void);
void sireflect_error_set(const char *message);

#endif

#include <stdlib.h>
#include <string.h>

static char *sireflect_current_error = NULL;

static char *sireflect_error_dup(const char *message) {
    sireflect_assert(message != NULL, "error message must not be NULL");

    const size_t len = strlen(message);
    char *copy = malloc(len + 1);
    sireflect_assert(copy != NULL, "failed to allocate error message");

    memcpy(copy, message, len + 1);
    return copy;
}

void sireflect_error_clear(void) {
    free(sireflect_current_error);
    sireflect_current_error = NULL;
}

void sireflect_error_set(const char *message) {
    sireflect_error_clear();

    if (message == NULL) {
        return;
    }

    sireflect_current_error = sireflect_error_dup(message);
}

const char *sireflect_error(void) {
    return sireflect_current_error;
}

const sireflect_field_info_t *
sireflect_field_info(sireflect_handle_t type, const char *field) {
    sireflect_error_clear();

    sireflect_assert(field != NULL, "field name must not be NULL");

    const sireflect_fields_t *fields = sireflect_type_fields(type);
    for (size_t i = 0; i < fields->field_count; i++) {
        if (strcmp(fields->fields[i].name, field) == 0) {
            return &fields->fields[i];
        }
    }

    return NULL;
}

sireflect_handle_t
sireflect_field_type(sireflect_handle_t type, const char *field) {
    sireflect_error_clear();

    const sireflect_field_info_t *info = sireflect_field_info(type, field);
    sireflect_assert(info != NULL, "field must exist");
    return info->type;
}

size_t
sireflect_field_size(sireflect_handle_t ref, const char *field) {
    sireflect_error_clear();

    const sireflect_field_info_t *info = sireflect_field_info(ref, field);
    sireflect_assert(info != NULL, "field must exist");
    return info->size;
}

const void *sireflect_field_ptr(
    sireflect_handle_t type,
    const void *obj,
    const char *field
) {
    sireflect_error_clear();

    sireflect_assert(obj != NULL, "object pointer must not be NULL");

    const sireflect_field_info_t *info = sireflect_field_info(type, field);
    sireflect_assert(info != NULL, "field must exist");

    return (const unsigned char *)obj + info->offset;
}

void *sireflect_field_mut_ptr(
    sireflect_handle_t type,
    void *obj,
    const char *field
) {
    sireflect_error_clear();

    sireflect_assert(obj != NULL, "object pointer must not be NULL");

    const sireflect_field_info_t *info = sireflect_field_info(type, field);
    sireflect_assert(info != NULL, "field must exist");

    return (unsigned char *)obj + info->offset;
}

int sireflect_field_copy(
    sireflect_handle_t type,
    void *obj,
    const char *field,
    const void *value
) {
    sireflect_error_clear();

    sireflect_assert(value != NULL, "source value pointer must not be NULL");

    const sireflect_field_info_t *info = sireflect_field_info(type, field);
    if (info == NULL) {
        return -1;
    }

    memcpy(sireflect_field_mut_ptr(type, obj, field), value, info->size);
    return 0;
}

#ifndef SIREFLECT_PARSER_H
#define SIREFLECT_PARSER_H

bool sireflect_parse_struct_fields(
    const char *struct_name,
    const char *fields_src,
    sireflect_field_info_t **out_fields,
    size_t *out_field_count,
    size_t struct_size,
    size_t struct_align,
    size_t *out_struct_size,
    size_t *out_struct_align,
    bool validate_layout,
    bool fail_fast
);

#endif

#ifndef SIREFLECT_REGISTRY_H
#define SIREFLECT_REGISTRY_H

typedef struct sireflect_registry_t sireflect_registry_t;

struct sireflect_registry_t {
    sireflect_type_info_t *types;
    size_t type_count;
    size_t type_cap;
};

sireflect_registry_t *sireflect_registry_current(void);
bool sireflect_registry_is_initialized(void);

sireflect_handle_t sireflect_registry_add_type(
    const char *name,
    sireflect_kind_t kind,
    size_t size,
    size_t align,
    sireflect_field_info_t *fields,
    size_t field_count
);

sireflect_handle_t sireflect_registry_get_or_add_array_type(
    sireflect_handle_t element_type,
    size_t element_count
);

sireflect_handle_t
sireflect_registry_get_or_add_pointer_type(sireflect_handle_t pointee_type);

sireflect_handle_t sireflect_registry_get_or_add_function_pointer_type(
    sireflect_handle_t return_type
);

sireflect_type_info_t *sireflect_registry_type_at(sireflect_handle_t handle);

const sireflect_type_info_t *sireflect_registry_const_type_at(sireflect_handle_t handle);

#endif

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#define SIREFLECT_MAX_ARRAY_DIMS 16

typedef enum {
    sireflect_token_ident,
    sireflect_token_integer,
    sireflect_token_lbrace,
    sireflect_token_rbrace,
    sireflect_token_lbracket,
    sireflect_token_rbracket,
    sireflect_token_lparen,
    sireflect_token_rparen,
    sireflect_token_star,
    sireflect_token_comma,
    sireflect_token_semicolon,
    sireflect_token_unknown,
    sireflect_token_end
} sireflect_token_kind_t;

typedef struct {
    sireflect_token_kind_t kind;
    const char *start;
    size_t len;
    size_t offset;
    size_t line;
    size_t column;
} sireflect_token_t;

typedef struct {
    const char *src;
    const char *struct_name;
    const char *field_start;
    size_t field_len;
    size_t pos;
    size_t line;
    size_t column;
    sireflect_token_t current;
    char message[512];
    bool failed;
    bool fail_fast;
} sireflect_parser_t;

typedef struct {
    const char *start;
    size_t len;
    char name[64];
    int has_name;
    size_t line;
    size_t column;
} sireflect_type_spec_t;

static inline int sireflect_is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }

static inline int sireflect_is_ident_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

static inline int sireflect_token_is_ident(sireflect_token_t token, const char *text) {
    return token.kind == sireflect_token_ident && strlen(text) == token.len &&
           strncmp(token.start, text, token.len) == 0;
}

static inline int sireflect_token_is_qualifier(sireflect_token_t token) {
    return sireflect_token_is_ident(token, "const") || sireflect_token_is_ident(token, "volatile");
}

static inline void
sireflect_type_spec_set(sireflect_type_spec_t *type, sireflect_token_t token) {
    type->start = token.start;
    type->len = token.len;
    type->name[0] = '\0';
    type->has_name = 0;
    type->line = token.line;
    type->column = token.column;
}

static inline void sireflect_type_spec_set2(
    sireflect_type_spec_t *type,
    sireflect_token_t first,
    sireflect_token_t second
) {
    const int len = snprintf(
        type->name,
        sizeof(type->name),
        "%.*s %.*s",
        (int)first.len,
        first.start,
        (int)second.len,
        second.start
    );
    (void)len;
    sireflect_indebug(
        sireflect_assert(len > 0 && (size_t)len < sizeof(type->name), "type specifier is too long");
    )
    type->start = NULL;
    type->len = 0;
    type->has_name = 1;
    type->line = first.line;
    type->column = first.column;
}

static inline void sireflect_type_spec_set3(
    sireflect_type_spec_t *type,
    sireflect_token_t first,
    sireflect_token_t second,
    sireflect_token_t third
) {
    const int len = snprintf(
        type->name,
        sizeof(type->name),
        "%.*s %.*s %.*s",
        (int)first.len,
        first.start,
        (int)second.len,
        second.start,
        (int)third.len,
        third.start
    );
    (void)len;
    sireflect_indebug(
        sireflect_assert(len > 0 && (size_t)len < sizeof(type->name), "type specifier is too long");
    );
    type->start = NULL;
    type->len = 0;
    type->has_name = 1;
    type->line = first.line;
    type->column = first.column;
}

static inline const char *sireflect_token_kind_name(sireflect_token_kind_t kind) {
    switch (kind) {
    case sireflect_token_ident:
        return "identifier";
    case sireflect_token_integer:
        return "integer";
    case sireflect_token_lbrace:
        return "'{'";
    case sireflect_token_rbrace:
        return "'}'";
    case sireflect_token_lbracket:
        return "'['";
    case sireflect_token_rbracket:
        return "']'";
    case sireflect_token_lparen:
        return "'('";
    case sireflect_token_rparen:
        return "')'";
    case sireflect_token_star:
        return "'*'";
    case sireflect_token_comma:
        return "','";
    case sireflect_token_semicolon:
        return "';'";
    case sireflect_token_unknown:
        return "unsupported token";
    case sireflect_token_end:
        return "end of input";
    }

    return "unknown token";
}

static inline void
sireflect_token_display(sireflect_token_t token, char *buffer, size_t buffer_size) {
    if (token.kind == sireflect_token_end) {
        snprintf(buffer, buffer_size, "end of input");
        return;
    }

    if (token.len == 0) {
        snprintf(buffer, buffer_size, "%s", sireflect_token_kind_name(token.kind));
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "%s '%.*s'",
        sireflect_token_kind_name(token.kind),
        (int)token.len,
        token.start
    );
}

static inline void
sireflect_parser_context(sireflect_parser_t *parser, char *buffer, size_t buffer_size) {
    if (parser->field_start != NULL) {
        snprintf(
            buffer,
            buffer_size,
            "struct '%s', field '%.*s'",
            parser->struct_name != NULL ? parser->struct_name : "<unknown>",
            (int)parser->field_len,
            parser->field_start
        );
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "struct '%s'",
        parser->struct_name != NULL ? parser->struct_name : "<unknown>"
    );
}

static inline void
sireflect_parser_fail_at(sireflect_parser_t *parser, sireflect_token_t token, const char *message) {
    char actual[96];
    char context[160];

    sireflect_token_display(token, actual, sizeof(actual));
    sireflect_parser_context(parser, context, sizeof(context));

    snprintf(
        parser->message,
        sizeof(parser->message),
        "%s in %s at line %zu, column %zu: actual %s",
        message,
        context,
        token.line,
        token.column,
        actual
    );

    parser->failed = true;
    if (parser->fail_fast) {
        sireflect_assert(false, parser->message);
    }
    sireflect_error_set(parser->message);
}

static inline void sireflect_parser_unexpected(
    sireflect_parser_t *parser,
    sireflect_token_kind_t expected,
    const char *context
) {
    char actual[96];
    char parser_context[160];

    sireflect_token_display(parser->current, actual, sizeof(actual));
    sireflect_parser_context(parser, parser_context, sizeof(parser_context));

    snprintf(
        parser->message,
        sizeof(parser->message),
        "unexpected token while parsing %s in %s at line %zu, column %zu: expected %s, actual %s",
        context,
        parser_context,
        parser->current.line,
        parser->current.column,
        sireflect_token_kind_name(expected),
        actual
    );

    parser->failed = true;
    if (parser->fail_fast) {
        sireflect_assert(false, parser->message);
    }
    sireflect_error_set(parser->message);
}

static inline void sireflect_parser_advance(sireflect_parser_t *parser) {
    if (parser->src[parser->pos] == '\n') {
        parser->line++;
        parser->column = 1;
    } else {
        parser->column++;
    }

    parser->pos++;
}

static inline void sireflect_parser_next(sireflect_parser_t *parser) {
    const char *src = parser->src;

    while (isspace((unsigned char)src[parser->pos])) {
        sireflect_parser_advance(parser);
    }

    const size_t start = parser->pos;
    const size_t line = parser->line;
    const size_t column = parser->column;
    const char c = src[start];

    if (c == '\0') {
        parser->current = (sireflect_token_t){ sireflect_token_end, &src[start], 0, start, line, column };
        return;
    }

    if (sireflect_is_ident_start(c)) {
        sireflect_parser_advance(parser);
        while (sireflect_is_ident_char(src[parser->pos])) {
            sireflect_parser_advance(parser);
        }

        parser->current = (sireflect_token_t){
            sireflect_token_ident,
            &src[start],
            parser->pos - start,
            start,
            line,
            column,
        };
        return;
    }

    if (isdigit((unsigned char)c)) {
        sireflect_parser_advance(parser);
        while (isdigit((unsigned char)src[parser->pos])) {
            sireflect_parser_advance(parser);
        }

        parser->current = (sireflect_token_t){
            sireflect_token_integer,
            &src[start],
            parser->pos - start,
            start,
            line,
            column,
        };
        return;
    }

    sireflect_parser_advance(parser);

    switch (c) {
    case '{':
        parser->current = (sireflect_token_t){ sireflect_token_lbrace, &src[start], 1, start, line, column };
        return;
    case '}':
        parser->current = (sireflect_token_t){ sireflect_token_rbrace, &src[start], 1, start, line, column };
        return;
    case '[':
        parser->current =
            (sireflect_token_t){ sireflect_token_lbracket, &src[start], 1, start, line, column };
        return;
    case ']':
        parser->current =
            (sireflect_token_t){ sireflect_token_rbracket, &src[start], 1, start, line, column };
        return;
    case '(':
        parser->current =
            (sireflect_token_t){ sireflect_token_lparen, &src[start], 1, start, line, column };
        return;
    case ')':
        parser->current =
            (sireflect_token_t){ sireflect_token_rparen, &src[start], 1, start, line, column };
        return;
    case '*':
        parser->current = (sireflect_token_t){ sireflect_token_star, &src[start], 1, start, line, column };
        return;
    case ',':
        parser->current = (sireflect_token_t){ sireflect_token_comma, &src[start], 1, start, line, column };
        return;
    case ';':
        parser->current =
            (sireflect_token_t){ sireflect_token_semicolon, &src[start], 1, start, line, column };
        return;
    default:
        parser->current = (sireflect_token_t){ sireflect_token_unknown, &src[start], 1, start, line, column };
        sireflect_parser_fail_at(
            parser,
            parser->current,
            "unsupported syntax in reflected struct; supported fields are '<type> <name>;', '<type> <name>, <name>;', '<type> *<name>;', '<type> (*<name>)();', '<type> <name>[N][M];', and '<type> *<name>[N];'"
        );
    }
}

static inline void sireflect_parser_init(
    sireflect_parser_t *parser,
    const char *struct_name,
    const char *src,
    bool fail_fast
) {
    sireflect_assert(parser != NULL, "parser must not be NULL");
    sireflect_assert(struct_name != NULL, "parser struct name must not be NULL");
    sireflect_assert(src != NULL, "parser source must not be NULL");

    parser->src = src;
    parser->struct_name = struct_name;
    parser->field_start = NULL;
    parser->field_len = 0;
    parser->pos = 0;
    parser->line = 1;
    parser->column = 1;
    parser->message[0] = '\0';
    parser->failed = false;
    parser->fail_fast = fail_fast;
    sireflect_parser_next(parser);
}

static inline sireflect_token_t
sireflect_expect(sireflect_parser_t *parser, sireflect_token_kind_t kind, const char *context) {
    sireflect_token_t token = parser->current;
    if (token.kind != kind) {
        sireflect_parser_unexpected(parser, kind, context);
        return token;
    }
    sireflect_parser_next(parser);
    return token;
}

static inline sireflect_token_t sireflect_expect_field_name(sireflect_parser_t *parser) {
    sireflect_token_t token = parser->current;
    if (token.kind != sireflect_token_ident || sireflect_token_is_qualifier(token)) {
        sireflect_parser_unexpected(parser, sireflect_token_ident, "field name");
        return token;
    }

    parser->field_start = token.start;
    parser->field_len = token.len;
    sireflect_parser_next(parser);
    return token;
}

static inline uint32_t sireflect_parse_qualifiers(sireflect_parser_t *parser) {
    uint32_t qualifiers = 0;

    for (;;) {
        if (sireflect_token_is_ident(parser->current, "const")) {
            qualifiers |= SIREFLECT_QUAL_CONST;
            sireflect_parser_next(parser);
            continue;
        }

        if (sireflect_token_is_ident(parser->current, "volatile")) {
            qualifiers |= SIREFLECT_QUAL_VOLATILE;
            sireflect_parser_next(parser);
            continue;
        }

        return qualifiers;
    }
}

static inline void
sireflect_fail_unsupported_type_specifier(sireflect_parser_t *parser, sireflect_token_t token) {
    sireflect_parser_fail_at(
        parser,
        token,
        "unsupported type specifier sequence; supported multi-token types are 'signed char', 'unsigned char', 'unsigned short', 'unsigned int', 'unsigned long', 'long long', and 'unsigned long long'"
    );
}

static inline sireflect_type_spec_t sireflect_parse_type_specifier(sireflect_parser_t *parser) {
    sireflect_token_t first = sireflect_expect(parser, sireflect_token_ident, "field type");
    sireflect_type_spec_t type;
    sireflect_type_spec_set(&type, first);
    if (parser->failed) {
        return type;
    }

    if (sireflect_token_is_ident(first, "signed")) {
        if (!sireflect_token_is_ident(parser->current, "char")) {
            sireflect_fail_unsupported_type_specifier(parser, parser->current);
            return type;
        }

        sireflect_token_t second = parser->current;
        sireflect_parser_next(parser);
        sireflect_type_spec_set2(&type, first, second);
        return type;
    }

    if (sireflect_token_is_ident(first, "long")) {
        if (!sireflect_token_is_ident(parser->current, "long")) {
            return type;
        }

        sireflect_token_t second = parser->current;
        sireflect_parser_next(parser);
        sireflect_type_spec_set2(&type, first, second);
        return type;
    }

    if (sireflect_token_is_ident(first, "unsigned")) {
        sireflect_token_t second = parser->current;

        if (sireflect_token_is_ident(second, "char") || sireflect_token_is_ident(second, "short") ||
            sireflect_token_is_ident(second, "int")) {
            sireflect_parser_next(parser);
            sireflect_type_spec_set2(&type, first, second);
            return type;
        }

        if (sireflect_token_is_ident(second, "long")) {
            sireflect_parser_next(parser);

            if (sireflect_token_is_ident(parser->current, "long")) {
                sireflect_token_t third = parser->current;
                sireflect_parser_next(parser);
                sireflect_type_spec_set3(&type, first, second, third);
                return type;
            }

            sireflect_type_spec_set2(&type, first, second);
            return type;
        }

        sireflect_fail_unsupported_type_specifier(parser, second);
        return type;
    }

    return type;
}

static inline char *sireflect_dup_range(const char *start, size_t len) {
    char *result = malloc(len + 1);
    sireflect_assert(result != NULL, "failed to allocate parser string");

    memcpy(result, start, len);
    result[len] = '\0';

    return result;
}

static inline size_t
sireflect_parse_array_count(sireflect_parser_t *parser, sireflect_token_t token) {
    size_t count = 0;

    for (size_t i = 0; i < token.len; i++) {
        const unsigned int digit = (unsigned int)(token.start[i] - '0');
        if (count > (SIZE_MAX - digit) / 10) {
            sireflect_parser_fail_at(parser, token, "array element count overflows size_t");
            return 0;
        }
        count = count * 10 + digit;
    }

    if (count == 0) {
        sireflect_parser_fail_at(parser, token, "array element count must be greater than zero");
        return 0;
    }

    return count;
}

static inline size_t
sireflect_parse_array_dimensions(sireflect_parser_t *parser, size_t *counts, size_t max_count) {
    size_t count = 0;

    while (parser->current.kind == sireflect_token_lbracket) {
        sireflect_parser_next(parser);

        if (parser->current.kind == sireflect_token_rbracket) {
            sireflect_parser_fail_at(parser, parser->current, "array element count is required");
            return count;
        }

        if (parser->current.kind != sireflect_token_integer) {
            sireflect_parser_fail_at(
                parser,
                parser->current,
                "array element count must be a positive integer literal"
            );
            return count;
        }

        sireflect_token_t count_token = parser->current;
        sireflect_parser_next(parser);

        if (parser->current.kind != sireflect_token_rbracket) {
            sireflect_parser_fail_at(parser, parser->current, "expected ']' after array element count");
            return count;
        }

        if (count >= max_count) {
            sireflect_parser_fail_at(parser, count_token, "too many array dimensions");
            return count;
        }

        counts[count++] = sireflect_parse_array_count(parser, count_token);
        if (parser->failed) {
            return count;
        }
        sireflect_parser_next(parser);
    }

    return count;
}

static inline void sireflect_parse_declarator_shape(sireflect_parser_t *parser) {
    size_t counts[SIREFLECT_MAX_ARRAY_DIMS];

    parser->field_start = NULL;
    parser->field_len = 0;

    if (parser->current.kind == sireflect_token_lparen) {
        sireflect_expect(parser, sireflect_token_lparen, "function pointer declarator");
        sireflect_expect(parser, sireflect_token_star, "function pointer declarator");
        (void)sireflect_expect_field_name(parser);
        sireflect_expect(parser, sireflect_token_rparen, "function pointer declarator");
        sireflect_expect(parser, sireflect_token_lparen, "function pointer parameters");
        sireflect_expect(parser, sireflect_token_rparen, "function pointer parameters");
    } else {
        if (parser->current.kind == sireflect_token_star) {
            sireflect_parser_next(parser);
        }

        sireflect_expect_field_name(parser);
    }

    (void)sireflect_parse_array_dimensions(parser, counts, SIREFLECT_MAX_ARRAY_DIMS);
}

static inline size_t sireflect_parse_declaration_shape(sireflect_parser_t *parser) {
    size_t count = 0;

    (void)sireflect_parse_qualifiers(parser);
    (void)sireflect_parse_type_specifier(parser);
    if (parser->failed) {
        return 0;
    }

    for (;;) {
        sireflect_parse_declarator_shape(parser);
        if (parser->failed) {
            return 0;
        }
        count++;

        if (parser->current.kind != sireflect_token_comma) {
            break;
        }

        sireflect_parser_next(parser);
    }

    sireflect_expect(parser, sireflect_token_semicolon, "field terminator");
    if (parser->failed) {
        return 0;
    }
    return count;
}

static inline bool sireflect_count_fields(
    const char *struct_name,
    const char *fields_src,
    bool fail_fast,
    size_t *out_count
) {
    sireflect_parser_t parser;
    size_t count = 0;

    sireflect_parser_init(&parser, struct_name, fields_src, fail_fast);
    sireflect_expect(&parser, sireflect_token_lbrace, "struct field list start");
    if (parser.failed) {
        return false;
    }

    while (parser.current.kind != sireflect_token_rbrace) {
        count += sireflect_parse_declaration_shape(&parser);
        if (parser.failed) {
            return false;
        }
    }

    sireflect_expect(&parser, sireflect_token_rbrace, "struct field list end");
    if (parser.failed) {
        return false;
    }
    sireflect_expect(&parser, sireflect_token_end, "trailing input after struct field list");
    if (parser.failed) {
        return false;
    }

    *out_count = count;
    return true;
}

static inline size_t sireflect_align_up(size_t value, size_t align) {
    sireflect_assert(align != 0, "alignment must not be zero");

    const size_t remainder = value % align;
    if (remainder == 0) {
        return value;
    }

    return value + align - remainder;
}

static inline sireflect_handle_t sireflect_resolve_field_type(
    sireflect_parser_t *parser,
    sireflect_type_spec_t type
) {
    char *owned_name = NULL;
    const char *type_name = type.name;

    if (!type.has_name) {
        owned_name = sireflect_dup_range(type.start, type.len);
        type_name = owned_name;
    }

    sireflect_handle_t field_type = sireflect_type_by_name(type_name);
    if (field_type == SIREFLECT_INVALID_HANDLE) {
        char context[160];

        sireflect_parser_context(parser, context, sizeof(context));
        snprintf(
            parser->message,
            sizeof(parser->message),
            "unknown field type '%s' in %s at line %zu, column %zu; register the type before this struct or use a supported primitive alias",
            type_name,
            context,
            type.line,
            type.column
        );
        free(owned_name);
        parser->failed = true;
        if (parser->fail_fast) {
            sireflect_assert(false, parser->message);
        }
        sireflect_error_set(parser->message);
        return SIREFLECT_INVALID_HANDLE;
    }

    free(owned_name);
    return field_type;
}

static inline void sireflect_parse_declarator(
    sireflect_parser_t *parser,
    sireflect_field_info_t *field,
    sireflect_type_spec_t type,
    uint32_t qualifiers,
    size_t *offset,
    size_t *max_align
) {
    parser->field_start = NULL;
    parser->field_len = 0;

    int is_pointer = 0;
    int is_function_pointer = 0;
    size_t array_counts[SIREFLECT_MAX_ARRAY_DIMS];
    size_t array_dim_count = 0;
    sireflect_token_t name_token;

    if (parser->current.kind == sireflect_token_lparen) {
        is_function_pointer = 1;
        sireflect_expect(parser, sireflect_token_lparen, "function pointer declarator");
        sireflect_expect(parser, sireflect_token_star, "function pointer declarator");
        name_token = sireflect_expect_field_name(parser);
        sireflect_expect(parser, sireflect_token_rparen, "function pointer declarator");
        sireflect_expect(parser, sireflect_token_lparen, "function pointer parameters");
        sireflect_expect(parser, sireflect_token_rparen, "function pointer parameters");
    } else {
        if (parser->current.kind == sireflect_token_star) {
            is_pointer = 1;
            sireflect_parser_next(parser);
        }

        name_token = sireflect_expect_field_name(parser);
    }
    if (parser->failed) {
        return;
    }

    array_dim_count =
        sireflect_parse_array_dimensions(parser, array_counts, SIREFLECT_MAX_ARRAY_DIMS);
    if (parser->failed) {
        return;
    }

    sireflect_handle_t field_type = sireflect_resolve_field_type(parser, type);
    if (parser->failed) {
        return;
    }

    if (is_function_pointer) {
        field_type = sireflect_registry_get_or_add_function_pointer_type(field_type);
    } else if (is_pointer) {
        field_type = sireflect_registry_get_or_add_pointer_type(field_type);
    }

    for (size_t i = array_dim_count; i > 0; i--) {
        field_type = sireflect_registry_get_or_add_array_type(field_type, array_counts[i - 1]);
    }

    const sireflect_type_info_t *type_info = sireflect_type_info(field_type);
    sireflect_assert(type_info != NULL, "field type metadata must exist");

    field->name = sireflect_dup_range(name_token.start, name_token.len);
    field->type = field_type;
    field->size = type_info->size;
    field->align = type_info->align;
    field->offset = sireflect_align_up(*offset, field->align);
    field->qualifiers = qualifiers;

    *offset = field->offset + field->size;
    if (field->align > *max_align) {
        *max_align = field->align;
    }
}

static inline size_t sireflect_parse_declaration(
    sireflect_parser_t *parser,
    sireflect_field_info_t *fields,
    size_t *offset,
    size_t *max_align
) {
    size_t count = 0;
    uint32_t qualifiers = sireflect_parse_qualifiers(parser);
    sireflect_type_spec_t type = sireflect_parse_type_specifier(parser);
    if (parser->failed) {
        return 0;
    }

    for (;;) {
        sireflect_parse_declarator(
            parser,
            &fields[count],
            type,
            qualifiers,
            offset,
            max_align
        );
        if (parser->failed) {
            return 0;
        }
        count++;

        if (parser->current.kind != sireflect_token_comma) {
            break;
        }

        sireflect_parser_next(parser);
    }

    sireflect_expect(parser, sireflect_token_semicolon, "field terminator");
    if (parser->failed) {
        return 0;
    }
    return count;
}

static inline void sireflect_free_parsed_fields(sireflect_field_info_t *fields, size_t field_count) {
    if (fields == NULL) {
        return;
    }

    for (size_t i = 0; i < field_count; i++) {
        free((char *)fields[i].name);
    }

    free(fields);
}

bool sireflect_parse_struct_fields(
    const char *struct_name,
    const char *fields_src,
    sireflect_field_info_t **out_fields,
    size_t *out_field_count,
    size_t struct_size,
    size_t struct_align,
    size_t *out_struct_size,
    size_t *out_struct_align,
    bool validate_layout,
    bool fail_fast
) {
    sireflect_assert(struct_name != NULL, "struct name must not be NULL");
    sireflect_assert(fields_src != NULL, "field source must not be NULL");
    sireflect_assert(out_fields != NULL, "output field pointer must not be NULL");
    sireflect_assert(out_field_count != NULL, "output field count pointer must not be NULL");
    sireflect_assert(out_struct_size != NULL, "output struct size must not be NULL");
    sireflect_assert(out_struct_align != NULL, "output struct alignment must not be NULL");

    size_t field_count = 0;
    if (!sireflect_count_fields(struct_name, fields_src, fail_fast, &field_count)) {
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

    sireflect_field_info_t *fields = NULL;

    if (field_count != 0) {
        fields = calloc(field_count, sizeof(*fields));
        sireflect_assert(fields != NULL, "failed to allocate field metadata");
    }

    sireflect_parser_t parser;
    sireflect_parser_init(&parser, struct_name, fields_src, fail_fast);
    sireflect_expect(&parser, sireflect_token_lbrace, "struct field list start");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

    size_t offset = 0;
    size_t max_align = 1;

    for (size_t i = 0; i < field_count;) {
        const size_t parsed_count =
            sireflect_parse_declaration(&parser, &fields[i], &offset, &max_align);
        if (parser.failed) {
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }
        i += parsed_count;
    }

    sireflect_expect(&parser, sireflect_token_rbrace, "struct field list end");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }
    sireflect_expect(&parser, sireflect_token_end, "trailing input after struct field list");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

#ifndef NDEBUG
    if (validate_layout) {
        const size_t computed_size = sireflect_align_up(offset, struct_align);
        if (computed_size != struct_size) {
            if (fail_fast) {
                sireflect_assert(false, "computed struct size does not match C layout");
            }
            sireflect_error_set("computed struct size does not match C layout");
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }

        if (max_align > struct_align) {
            if (fail_fast) {
                sireflect_assert(false, "computed field alignment exceeds struct alignment");
            }
            sireflect_error_set("computed field alignment exceeds struct alignment");
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }
    }
#else
    (void)struct_size;
    (void)struct_align;
    (void)validate_layout;
#endif

    *out_fields = fields;
    *out_field_count = field_count;
    *out_struct_align = max_align;
    *out_struct_size = sireflect_align_up(offset, max_align);
    return true;
}

static sireflect_registry_t sireflect_global_registry;
static size_t sireflect_global_references;

bool sireflect_registry_is_initialized(void) {
    return sireflect_global_references != 0;
}

sireflect_registry_t *sireflect_registry_current(void) {
    sireflect_assert(sireflect_registry_is_initialized(), "sireflect must be initialized");
    return &sireflect_global_registry;
}

static char *sireflect_dup_cstr(const char *str) {
    sireflect_assert(str != NULL, "string must not be NULL");

    const size_t len = strlen(str);
    char *result = malloc(len + 1);
    sireflect_assert(result != NULL, "failed to allocate string");

    memcpy(result, str, len + 1);
    return result;
}

static char *
sireflect_format_array_type_name(const sireflect_type_info_t *element, size_t element_count) {
    sireflect_assert(element != NULL, "array element metadata must exist");

    const char *suffix = strchr(element->name, '[');
    if (element->kind != sireflect_kind_array || suffix == NULL) {
        const int name_len = snprintf(NULL, 0, "%s[%zu]", element->name, element_count);
        sireflect_assert(name_len > 0, "failed to format array type name");

        char *name = malloc((size_t)name_len + 1);
        sireflect_assert(name != NULL, "failed to allocate array type name");
        snprintf(name, (size_t)name_len + 1, "%s[%zu]", element->name, element_count);
        return name;
    }

    const size_t prefix_len = (size_t)(suffix - element->name);
    const int count_len = snprintf(NULL, 0, "[%zu]", element_count);
    sireflect_assert(count_len > 0, "failed to format array dimension");

    const size_t suffix_len = strlen(suffix);
    char *name = malloc(prefix_len + (size_t)count_len + suffix_len + 1);
    sireflect_assert(name != NULL, "failed to allocate array type name");

    memcpy(name, element->name, prefix_len);
    snprintf(name + prefix_len, (size_t)count_len + 1, "[%zu]", element_count);
    memcpy(name + prefix_len + (size_t)count_len, suffix, suffix_len + 1);

    return name;
}

static sireflect_handle_t sireflect_handle_from_index(size_t index) {
    return (sireflect_handle_t)(index + 1);
}

static size_t sireflect_index_from_handle(sireflect_handle_t handle) {
    sireflect_assert(handle != SIREFLECT_INVALID_HANDLE, "type handle must be valid");
    return (size_t)(handle - 1);
}

static void sireflect_registry_reserve(size_t min_cap) {
    sireflect_registry_t *reg = sireflect_registry_current();

    if (reg->type_cap >= min_cap) {
        return;
    }

    size_t new_cap = reg->type_cap == 0 ? 16 : reg->type_cap * 2;
    while (new_cap < min_cap) {
        new_cap *= 2;
    }

    sireflect_type_info_t *types = realloc(reg->types, new_cap * sizeof(*types));
    sireflect_assert(types != NULL, "failed to allocate type metadata");

    reg->types = types;
    reg->type_cap = new_cap;
}

sireflect_handle_t sireflect_registry_add_type(
    const char *name,
    sireflect_kind_t kind,
    size_t size,
    size_t align,
    sireflect_field_info_t *fields,
    size_t field_count
) {
    sireflect_registry_t *reg = sireflect_registry_current();

    sireflect_assert(name != NULL, "type name must not be NULL");
    sireflect_assert(size != 0 || kind == sireflect_kind_struct, "non-struct type size must not be zero");
    sireflect_assert(align != 0, "type alignment must not be zero");

    sireflect_registry_reserve(reg->type_count + 1);

    const size_t index = reg->type_count++;
    reg->types[index] = (sireflect_type_info_t){
        .name = sireflect_dup_cstr(name),
        .kind = kind,
        .size = size,
        .align = align,
        .fields =
            {
                .fields = fields,
                .field_count = field_count,
            },
        .element_type = SIREFLECT_INVALID_HANDLE,
        .element_count = 0,
    };

    return sireflect_handle_from_index(index);
}

sireflect_handle_t
sireflect_registry_get_or_add_pointer_type(sireflect_handle_t pointee_type) {
    sireflect_registry_t *reg = sireflect_registry_current();

    sireflect_assert(pointee_type != SIREFLECT_INVALID_HANDLE, "pointer pointee type must be valid");

    for (size_t i = 0; i < reg->type_count; i++) {
        const sireflect_type_info_t *type = &reg->types[i];
        if (type->kind == sireflect_kind_pointer && type->element_type == pointee_type) {
            return sireflect_handle_from_index(i);
        }
    }

    const sireflect_type_info_t *pointee = sireflect_registry_const_type_at(pointee_type);
    sireflect_assert(pointee != NULL, "pointer pointee metadata must exist");

    const int name_len = snprintf(NULL, 0, "%s*", pointee->name);
    sireflect_assert(name_len > 0, "failed to format pointer type name");

    char *name = malloc((size_t)name_len + 1);
    sireflect_assert(name != NULL, "failed to allocate pointer type name");
    snprintf(name, (size_t)name_len + 1, "%s*", pointee->name);

    sireflect_handle_t pointer_type = sireflect_registry_add_type(
        name,
        sireflect_kind_pointer,
        sizeof(ptr),
        _Alignof(ptr),
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *pointer_info = sireflect_registry_type_at(pointer_type);
    pointer_info->element_type = pointee_type;

    return pointer_type;
}

sireflect_handle_t sireflect_registry_get_or_add_function_pointer_type(
    sireflect_handle_t return_type
) {
    sireflect_registry_t *reg = sireflect_registry_current();

    sireflect_assert(return_type != SIREFLECT_INVALID_HANDLE, "function return type must be valid");

    for (size_t i = 0; i < reg->type_count; i++) {
        const sireflect_type_info_t *type = &reg->types[i];
        if (type->kind == sireflect_kind_function_pointer && type->element_type == return_type) {
            return sireflect_handle_from_index(i);
        }
    }

    const sireflect_type_info_t *return_info = sireflect_registry_const_type_at(return_type);
    sireflect_assert(return_info != NULL, "function return type metadata must exist");

    const int name_len = snprintf(NULL, 0, "%s(*)()", return_info->name);
    sireflect_assert(name_len > 0, "failed to format function pointer type name");

    char *name = malloc((size_t)name_len + 1);
    sireflect_assert(name != NULL, "failed to allocate function pointer type name");
    snprintf(name, (size_t)name_len + 1, "%s(*)()", return_info->name);

    sireflect_handle_t function_pointer_type = sireflect_registry_add_type(
        name,
        sireflect_kind_function_pointer,
        sizeof(ptr),
        _Alignof(ptr),
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *function_pointer_info =
        sireflect_registry_type_at(function_pointer_type);
    function_pointer_info->element_type = return_type;

    return function_pointer_type;
}

sireflect_handle_t sireflect_registry_get_or_add_array_type(
    sireflect_handle_t element_type,
    size_t element_count
) {
    sireflect_registry_t *reg = sireflect_registry_current();

    sireflect_assert(element_type != SIREFLECT_INVALID_HANDLE, "array element type must be valid");
    sireflect_assert(element_count != 0, "array element count must not be zero");

    for (size_t i = 0; i < reg->type_count; i++) {
        const sireflect_type_info_t *type = &reg->types[i];
        if (type->kind == sireflect_kind_array && type->element_type == element_type &&
            type->element_count == element_count) {
            return sireflect_handle_from_index(i);
        }
    }

    const sireflect_type_info_t *element = sireflect_registry_const_type_at(element_type);
    sireflect_assert(element != NULL, "array element metadata must exist");
    sireflect_assert(element->size <= SIZE_MAX / element_count, "array type size overflows size_t");

    char *name = sireflect_format_array_type_name(element, element_count);

    sireflect_handle_t array_type = sireflect_registry_add_type(
        name,
        sireflect_kind_array,
        element->size * element_count,
        element->align,
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *array_info = sireflect_registry_type_at(array_type);
    array_info->element_type = element_type;
    array_info->element_count = element_count;

    return array_type;
}

#define add_type(name, kind) \
    sireflect_registry_add_type(#name, kind, sizeof(name), _Alignof(name), NULL, 0)

#define add_named_type(c_type, reflected_name, kind) \
    sireflect_registry_add_type(reflected_name, kind, sizeof(c_type), _Alignof(c_type), NULL, 0)

static inline void sireflect_register_builtin_types(void) {
    add_type(u8, sireflect_kind_u8);
    add_type(u16, sireflect_kind_u16);
    add_type(u32, sireflect_kind_u32);
    add_type(u64, sireflect_kind_u64);
    add_type(i8, sireflect_kind_i8);
    add_type(i16, sireflect_kind_i16);
    add_type(i32, sireflect_kind_i32);
    add_type(i64, sireflect_kind_i64);
    add_type(f32, sireflect_kind_f32);
    add_type(f64, sireflect_kind_f64);
    add_type(bool, sireflect_kind_bool);
    add_type(char, sireflect_kind_char);
    add_type(ptr, sireflect_kind_ptr);

    add_type(uint8_t, sireflect_kind_u8);
    add_type(uint16_t, sireflect_kind_u16);
    add_type(uint32_t, sireflect_kind_u32);
    add_type(uint64_t, sireflect_kind_u64);
    add_type(int8_t, sireflect_kind_i8);
    add_type(int16_t, sireflect_kind_i16);
    add_type(int32_t, sireflect_kind_i32);
    add_type(int64_t, sireflect_kind_i64);

    add_type(float, sireflect_kind_f32);
    add_type(double, sireflect_kind_f64);
    add_type(short, sireflect_kind_short);
    add_type(int, sireflect_kind_int);
    add_type(long, sireflect_kind_long);

    add_named_type(signed char, "signed char", sireflect_kind_signed_char);
    add_named_type(unsigned char, "unsigned char", sireflect_kind_unsigned_char);
    add_named_type(unsigned short, "unsigned short", sireflect_kind_unsigned_short);
    add_named_type(unsigned int, "unsigned int", sireflect_kind_unsigned_int);
    add_named_type(unsigned long, "unsigned long", sireflect_kind_unsigned_long);
    add_named_type(long long, "long long", sireflect_kind_long_long);
    add_named_type(unsigned long long, "unsigned long long", sireflect_kind_unsigned_long_long);
}

void sireflect_init(void) {
    sireflect_error_clear();

    if (sireflect_global_references == 0) {
        sireflect_global_references = 1;
        sireflect_register_builtin_types();
        return;
    }

    sireflect_assert(sireflect_global_references != SIZE_MAX, "sireflect reference count overflow");
    sireflect_global_references++;
}

static void sireflect_registry_clear(void) {
    sireflect_registry_t *reg = &sireflect_global_registry;

    for (size_t i = 0; i < reg->type_count; i++) {
        sireflect_type_info_t *type = &reg->types[i];

        free((char *)type->name);

        for (size_t f = 0; f < type->fields.field_count; f++) {
            free((char *)type->fields.fields[f].name);
        }

        free(type->fields.fields);
    }

    free(reg->types);
    memset(reg, 0, sizeof(*reg));
}

void sireflect_fini(void) {
    sireflect_error_clear();

    sireflect_assert(sireflect_global_references != 0, "sireflect is not initialized");
    if (sireflect_global_references == 0) {
        return;
    }

    sireflect_global_references--;
    if (sireflect_global_references == 0) {
        sireflect_registry_clear();
    }
}

sireflect_handle_t sireflect_type_by_name(const char *name) {
    sireflect_error_clear();

    sireflect_registry_t *reg = sireflect_registry_current();
    sireflect_assert(name != NULL, "type name must not be NULL");

    for (size_t i = 0; i < reg->type_count; i++) {
        if (strcmp(reg->types[i].name, name) == 0) {
            return sireflect_handle_from_index(i);
        }
    }

    return SIREFLECT_INVALID_HANDLE;
}

const sireflect_type_info_t *sireflect_registry_const_type_at(sireflect_handle_t handle) {
    const sireflect_registry_t *reg = sireflect_registry_current();

    const size_t index = sireflect_index_from_handle(handle);
    sireflect_assert(index < reg->type_count, "type handle is out of range");

    return &reg->types[index];
}

sireflect_type_info_t *sireflect_registry_type_at(sireflect_handle_t handle) {
    return (sireflect_type_info_t *)sireflect_registry_const_type_at(handle);
}

sireflect_handle_t
sireflect_try_register_struct(const sireflect_struct_desc_t *desc) {
    sireflect_error_clear();

    if (!sireflect_registry_is_initialized() || desc == NULL || desc->name == NULL || desc->fields == NULL ||
        desc->align == 0) {
        sireflect_error_set(
            sireflect_registry_is_initialized() ? "invalid struct descriptor"
                                                 : "sireflect is not initialized"
        );
        return SIREFLECT_INVALID_HANDLE;
    }

    sireflect_handle_t existing = sireflect_type_by_name(desc->name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        const sireflect_type_info_t *type = sireflect_type_info(existing);
        if (type->kind != sireflect_kind_struct || type->size != desc->size ||
            type->align != desc->align) {
            sireflect_error_set("existing type is incompatible with struct descriptor");
            return SIREFLECT_INVALID_HANDLE;
        }
        return existing;
    }

    sireflect_field_info_t *parsed_fields = NULL;
    size_t field_count = 0;
    size_t parsed_size = 0;
    size_t parsed_align = 0;

    if (!sireflect_parse_struct_fields(
        desc->name,
        desc->fields,
        &parsed_fields,
        &field_count,
        desc->size,
        desc->align,
        &parsed_size,
        &parsed_align,
        true,
        false
    )) {
        return SIREFLECT_INVALID_HANDLE;
    }

    return sireflect_registry_add_type(
        desc->name,
        sireflect_kind_struct,
        desc->size,
        desc->align,
        parsed_fields,
        field_count
    );
}

sireflect_handle_t
sireflect_register_struct(const sireflect_struct_desc_t *desc) {
    sireflect_error_clear();

    sireflect_assert(desc != NULL, "struct descriptor must not be NULL");
    sireflect_assert(desc->name != NULL, "struct descriptor name must not be NULL");
    sireflect_assert(desc->fields != NULL, "struct descriptor fields must not be NULL");
    sireflect_assert(desc->align != 0, "struct descriptor alignment must not be zero");

    sireflect_handle_t handle = SIREFLECT_INVALID_HANDLE;

    if (sireflect_registry_is_initialized() && desc != NULL && desc->name != NULL && desc->fields != NULL &&
        desc->align != 0) {
        sireflect_handle_t existing = sireflect_type_by_name(desc->name);
        if (existing != SIREFLECT_INVALID_HANDLE) {
            const sireflect_type_info_t *type = sireflect_type_info(existing);
            if (type->kind != sireflect_kind_struct || type->size != desc->size ||
                type->align != desc->align) {
                sireflect_assert(type->kind == sireflect_kind_struct, "existing type must be a struct");
                sireflect_assert(
                    type->size == desc->size,
                    "existing struct size must match descriptor"
                );
                sireflect_assert(
                    type->align == desc->align,
                    "existing struct alignment must match descriptor"
                );
                return SIREFLECT_INVALID_HANDLE;
            }
            return existing;
        }

        sireflect_field_info_t *parsed_fields = NULL;
        size_t field_count = 0;
        size_t parsed_size = 0;
        size_t parsed_align = 0;

        if (sireflect_parse_struct_fields(
                desc->name,
                desc->fields,
                &parsed_fields,
                &field_count,
                desc->size,
                desc->align,
                &parsed_size,
                &parsed_align,
                true,
                true
            )) {
            handle = sireflect_registry_add_type(
                desc->name,
                sireflect_kind_struct,
                desc->size,
                desc->align,
                parsed_fields,
                field_count
            );
        }
    }

    sireflect_assert(handle != SIREFLECT_INVALID_HANDLE, "failed to register struct");
    return handle;
}

sireflect_handle_t sireflect_try_register_dynamic_struct(
    const char *name,
    const char *fields
) {
    sireflect_error_clear();

    if (!sireflect_registry_is_initialized() || name == NULL || fields == NULL) {
        sireflect_error_set(
            sireflect_registry_is_initialized() ? "invalid dynamic struct descriptor"
                                                 : "sireflect is not initialized"
        );
        return SIREFLECT_INVALID_HANDLE;
    }

    sireflect_handle_t existing = sireflect_type_by_name(name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        if (!sireflect_type_is_struct(sireflect_type_info(existing))) {
            sireflect_error_set("existing type is not a struct");
            return SIREFLECT_INVALID_HANDLE;
        }
        return existing;
    }

    sireflect_field_info_t *parsed_fields = NULL;
    size_t field_count = 0;
    size_t size = 0;
    size_t align = 0;

    if (!sireflect_parse_struct_fields(
            name,
            fields,
            &parsed_fields,
            &field_count,
            0,
            1,
            &size,
            &align,
            false,
            false
        )) {
        return SIREFLECT_INVALID_HANDLE;
    }

    return sireflect_registry_add_type(
        name,
        sireflect_kind_struct,
        size,
        align,
        parsed_fields,
        field_count
    );
}

const char *sireflect_kind_name(sireflect_kind_t kind) {
    sireflect_error_clear();

    switch (kind) {
    case sireflect_kind_u8:
        return "u8";
    case sireflect_kind_u16:
        return "u16";
    case sireflect_kind_u32:
        return "u32";
    case sireflect_kind_u64:
        return "u64";
    case sireflect_kind_i8:
        return "i8";
    case sireflect_kind_i16:
        return "i16";
    case sireflect_kind_i32:
        return "i32";
    case sireflect_kind_i64:
        return "i64";
    case sireflect_kind_f32:
        return "f32";
    case sireflect_kind_f64:
        return "f64";
    case sireflect_kind_bool:
        return "bool";
    case sireflect_kind_char:
        return "char";
    case sireflect_kind_short:
        return "short";
    case sireflect_kind_int:
        return "int";
    case sireflect_kind_long:
        return "long";
    case sireflect_kind_ptr:
        return "ptr";
    case sireflect_kind_pointer:
        return "pointer";
    case sireflect_kind_struct:
        return "struct";
    case sireflect_kind_array:
        return "array";
    case sireflect_kind_signed_char:
        return "signed char";
    case sireflect_kind_unsigned_char:
        return "unsigned char";
    case sireflect_kind_unsigned_short:
        return "unsigned short";
    case sireflect_kind_unsigned_int:
        return "unsigned int";
    case sireflect_kind_unsigned_long:
        return "unsigned long";
    case sireflect_kind_long_long:
        return "long long";
    case sireflect_kind_unsigned_long_long:
        return "unsigned long long";
    case sireflect_kind_function_pointer:
        return "function pointer";
    }

    return "unknown";
}

bool sireflect_is_numeric(sireflect_kind_t kind) {
    sireflect_error_clear();

    switch (kind) {
    case sireflect_kind_u8:
    case sireflect_kind_u16:
    case sireflect_kind_u32:
    case sireflect_kind_u64:
    case sireflect_kind_i8:
    case sireflect_kind_i16:
    case sireflect_kind_i32:
    case sireflect_kind_i64:
    case sireflect_kind_f32:
    case sireflect_kind_f64:
    case sireflect_kind_char:
    case sireflect_kind_short:
    case sireflect_kind_int:
    case sireflect_kind_long:
    case sireflect_kind_signed_char:
    case sireflect_kind_unsigned_char:
    case sireflect_kind_unsigned_short:
    case sireflect_kind_unsigned_int:
    case sireflect_kind_unsigned_long:
    case sireflect_kind_long_long:
    case sireflect_kind_unsigned_long_long:
        return true;
    case sireflect_kind_bool:
    case sireflect_kind_ptr:
    case sireflect_kind_pointer:
    case sireflect_kind_struct:
    case sireflect_kind_array:
    case sireflect_kind_function_pointer:
        return false;
    }

    return false;
}

const sireflect_type_info_t *
sireflect_type_info(sireflect_handle_t ref) {
    sireflect_error_clear();

    return sireflect_registry_const_type_at(ref);
}

const sireflect_fields_t *
sireflect_type_fields(sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(ref);
    return &type->fields;
}

size_t sireflect_type_size(sireflect_handle_t ref) {
    sireflect_error_clear();

    return sireflect_type_info(ref)->size;
}

const char *sireflect_type_name(sireflect_handle_t ref) {
    sireflect_error_clear();

    return sireflect_type_info(ref)->name;
}

bool sireflect_type_is_struct(const sireflect_type_info_t *info) {
    sireflect_error_clear();

    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_struct;
}

bool sireflect_type_is_array(const sireflect_type_info_t *info) {
    sireflect_error_clear();

    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_array;
}

bool sireflect_type_is_pointer(const sireflect_type_info_t *info) {
    sireflect_error_clear();

    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_pointer || info->kind == sireflect_kind_function_pointer;
}

sireflect_handle_t
sireflect_type_element(sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(ref);
    sireflect_assert(type->kind == sireflect_kind_array, "type must be an array");
    return type->element_type;
}

size_t
sireflect_type_element_count(sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(ref);
    sireflect_assert(type->kind == sireflect_kind_array, "type must be an array");
    return type->element_count;
}

sireflect_handle_t
sireflect_type_pointee(sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(ref);
    sireflect_assert(
        type->kind == sireflect_kind_pointer || type->kind == sireflect_kind_function_pointer,
        "type must be a typed pointer"
    );
    return type->element_type;
}

#ifndef SIJSON_INTERNAL_H
#define SIJSON_INTERNAL_H

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
typedef union sijson_max_align {
    long double long_double;
    void *pointer;
    long long integer;
} sijson_max_align_t;
#else
typedef max_align_t sijson_max_align_t;
#endif

#if defined(__GNUC__) || defined(__clang__)
#define SIJSON_INTERNAL_API __attribute__((visibility("hidden")))
#else
#define SIJSON_INTERNAL_API
#endif

typedef struct sijson_member {
    char *key;
    sijson_value_t value;
} sijson_member_t;

typedef struct sijson_array {
    sijson_value_t *items;
    size_t len;
    size_t cap;
} sijson_array_t;

typedef struct sijson_object {
    sijson_member_t *items;
    size_t len;
    size_t cap;
} sijson_object_t;

struct sijson_value {
    sijson_type_t type;
    union {
        bool boolean;
        double number;
        char *string;
        sijson_array_t array;
        sijson_object_t object;
    } as;
};

typedef struct sijson_writer {
    char *data;
    size_t len;
    size_t cap;
} sijson_writer_t;

typedef struct sijson_parser {
    const char *cur;
} sijson_parser_t;

SIJSON_INTERNAL_API void sijson_clear_error(void);
SIJSON_INTERNAL_API bool sijson_set_error(const char *message);
SIJSON_INTERNAL_API bool sijson_set_error_at(const char *message, const char *at);

SIJSON_INTERNAL_API char *sijson_dup_range(const char *start, size_t len);
SIJSON_INTERNAL_API char *sijson_dup_cstr(const char *str);

SIJSON_INTERNAL_API void *sijson_arena_alloc(size_t size, size_t align);
SIJSON_INTERNAL_API char *sijson_arena_dup_range(const char *start, size_t len);
SIJSON_INTERNAL_API char *sijson_arena_dup_cstr(const char *str);
SIJSON_INTERNAL_API size_t sijson_arena_mark(void);
SIJSON_INTERNAL_API void sijson_arena_rewind(size_t mark);

SIJSON_INTERNAL_API bool sijson_reserve_array(sijson_array_t *array, size_t need);
SIJSON_INTERNAL_API bool sijson_reserve_object(sijson_object_t *object, size_t need);
SIJSON_INTERNAL_API sijson_value_t sijson_new_value(sijson_type_t type);

SIJSON_INTERNAL_API bool sijson_writer_reserve(sijson_writer_t *writer, size_t extra);
SIJSON_INTERNAL_API bool sijson_writer_putc(sijson_writer_t *writer, char c);
SIJSON_INTERNAL_API bool sijson_writer_write(sijson_writer_t *writer, const char *data, size_t len);
SIJSON_INTERNAL_API bool sijson_writer_cstr(sijson_writer_t *writer, const char *str);
SIJSON_INTERNAL_API bool sijson_writer_string(sijson_writer_t *writer, const char *str);
SIJSON_INTERNAL_API bool sijson_write_value(sijson_writer_t *writer, sijson_value_t value);

#endif

static char g_error[256];

void sijson_clear_error(void) { g_error[0] = '\0'; }

bool sijson_set_error(const char *message) {
    if (message == NULL) {
        message = "unknown sijson error";
    }

    snprintf(g_error, sizeof(g_error), "%s", message);
    return false;
}

bool sijson_set_error_at(const char *message, const char *at) {
    if (at == NULL) {
        return sijson_set_error(message);
    }

    snprintf(g_error, sizeof(g_error), "%s near '%.24s'", message, at);
    return false;
}

const char *sijson_error(void) { return g_error[0] != '\0' ? g_error : NULL; }

static void sijson_skip_ws(sijson_parser_t *parser) {
    while (isspace((unsigned char)*parser->cur)) {
        parser->cur++;
    }
}

static bool sijson_take(sijson_parser_t *parser, char c) {
    sijson_skip_ws(parser);
    if (*parser->cur != c) {
        return false;
    }
    parser->cur++;
    return true;
}

static int sijson_hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + c - 'a';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + c - 'A';
    }
    return -1;
}

static bool sijson_writer_utf8(sijson_writer_t *writer, unsigned codepoint) {
    char out[4];
    size_t len = 0;

    if (codepoint <= 0x7f) {
        out[len++] = (char)codepoint;
    } else if (codepoint <= 0x7ff) {
        out[len++] = (char)(0xc0 | (codepoint >> 6));
        out[len++] = (char)(0x80 | (codepoint & 0x3f));
    } else {
        out[len++] = (char)(0xe0 | (codepoint >> 12));
        out[len++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        out[len++] = (char)(0x80 | (codepoint & 0x3f));
    }

    return sijson_writer_write(writer, out, len);
}

static char *sijson_parse_string_raw(sijson_parser_t *parser) {
    sijson_skip_ws(parser);
    if (*parser->cur != '"') {
        sijson_set_error_at("expected JSON string", parser->cur);
        return NULL;
    }
    parser->cur++;

    sijson_writer_t writer = { 0 };
    const char *chunk = parser->cur;
    while (*parser->cur != '\0') {
        unsigned char c = (unsigned char)*parser->cur;
        if (c == '"') {
            if (!sijson_writer_write(&writer, chunk, (size_t)(parser->cur - chunk))) {
                free(writer.data);
                return NULL;
            }
            parser->cur++;
            if (writer.data == NULL) {
                return sijson_arena_dup_cstr("");
            }
            char *result = sijson_arena_dup_cstr(writer.data);
            free(writer.data);
            return result;
        }

        if (c < 0x20) {
            free(writer.data);
            sijson_set_error_at("control character in JSON string", parser->cur);
            return NULL;
        }

        if (c != '\\') {
            parser->cur++;
            continue;
        }

        if (!sijson_writer_write(&writer, chunk, (size_t)(parser->cur - chunk))) {
            free(writer.data);
            return NULL;
        }

        parser->cur++;
        char escaped = *parser->cur++;
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
            if (!sijson_writer_putc(&writer, escaped)) {
                free(writer.data);
                return NULL;
            }
            break;
        case 'b':
            if (!sijson_writer_putc(&writer, '\b')) {
                free(writer.data);
                return NULL;
            }
            break;
        case 'f':
            if (!sijson_writer_putc(&writer, '\f')) {
                free(writer.data);
                return NULL;
            }
            break;
        case 'n':
            if (!sijson_writer_putc(&writer, '\n')) {
                free(writer.data);
                return NULL;
            }
            break;
        case 'r':
            if (!sijson_writer_putc(&writer, '\r')) {
                free(writer.data);
                return NULL;
            }
            break;
        case 't':
            if (!sijson_writer_putc(&writer, '\t')) {
                free(writer.data);
                return NULL;
            }
            break;
        case 'u': {
            unsigned codepoint = 0;
            for (size_t i = 0; i < 4; i++) {
                int digit = sijson_hex_digit(parser->cur[i]);
                if (digit < 0) {
                    free(writer.data);
                    sijson_set_error_at("invalid unicode escape", parser->cur);
                    return NULL;
                }
                codepoint = (codepoint << 4) | (unsigned)digit;
            }
            parser->cur += 4;
            if (!sijson_writer_utf8(&writer, codepoint)) {
                free(writer.data);
                return NULL;
            }
            break;
        }
        default:
            free(writer.data);
            sijson_set_error_at("invalid JSON string escape", parser->cur - 1);
            return NULL;
        }
        chunk = parser->cur;
    }

    free(writer.data);
    sijson_set_error("unterminated JSON string");
    return NULL;
}

static sijson_value_t sijson_parse_value(sijson_parser_t *parser);

static sijson_value_t sijson_parse_array_value(sijson_parser_t *parser) {
    if (!sijson_take(parser, '[')) {
        return NULL;
    }

    sijson_value_t array = sijson_new_value(SIJSON_ARRAY);
    if (array == NULL) {
        return NULL;
    }

    sijson_skip_ws(parser);
    if (*parser->cur == ']') {
        parser->cur++;
        return array;
    }

    for (;;) {
        sijson_value_t item = sijson_parse_value(parser);
        if (item == NULL) {
            return NULL;
        }
        if (!sijson_reserve_array(&array->as.array, array->as.array.len + 1)) {
            return NULL;
        }
        array->as.array.items[array->as.array.len++] = item;

        sijson_skip_ws(parser);
        if (*parser->cur == ']') {
            parser->cur++;
            return array;
        }
        if (*parser->cur != ',') {
            sijson_set_error_at("expected ',' or ']'", parser->cur);
            return NULL;
        }
        parser->cur++;
    }
}

static sijson_value_t sijson_parse_object_value(sijson_parser_t *parser) {
    if (!sijson_take(parser, '{')) {
        return NULL;
    }

    sijson_value_t object = sijson_new_value(SIJSON_OBJECT);
    if (object == NULL) {
        return NULL;
    }

    sijson_skip_ws(parser);
    if (*parser->cur == '}') {
        parser->cur++;
        return object;
    }

    for (;;) {
        char *key = sijson_parse_string_raw(parser);
        if (key == NULL) {
            return NULL;
        }
        if (!sijson_take(parser, ':')) {
            sijson_set_error_at("expected ':'", parser->cur);
            return NULL;
        }

        sijson_value_t item = sijson_parse_value(parser);
        if (item == NULL) {
            return NULL;
        }

        if (!sijson_reserve_object(&object->as.object, object->as.object.len + 1)) {
            return NULL;
        }
        object->as.object.items[object->as.object.len++] = (sijson_member_t){
            .key = key,
            .value = item,
        };

        sijson_skip_ws(parser);
        if (*parser->cur == '}') {
            parser->cur++;
            return object;
        }
        if (*parser->cur != ',') {
            sijson_set_error_at("expected ',' or '}'", parser->cur);
            return NULL;
        }
        parser->cur++;
    }
}

static sijson_value_t sijson_parse_number_value(sijson_parser_t *parser) {
    sijson_skip_ws(parser);
    const char *start = parser->cur;
    const char *scan = start;

    if (*scan == '-') {
        scan++;
    }

    if (*scan == '0') {
        scan++;
    } else if (*scan >= '1' && *scan <= '9') {
        do {
            scan++;
        } while (isdigit((unsigned char)*scan));
    } else {
        sijson_set_error_at("invalid JSON number", start);
        return NULL;
    }

    if (*scan == '.') {
        scan++;
        if (!isdigit((unsigned char)*scan)) {
            sijson_set_error_at("invalid JSON number", start);
            return NULL;
        }
        do {
            scan++;
        } while (isdigit((unsigned char)*scan));
    }

    if (*scan == 'e' || *scan == 'E') {
        scan++;
        if (*scan == '+' || *scan == '-') {
            scan++;
        }
        if (!isdigit((unsigned char)*scan)) {
            sijson_set_error_at("invalid JSON number", start);
            return NULL;
        }
        do {
            scan++;
        } while (isdigit((unsigned char)*scan));
    }

    size_t len = (size_t)(scan - start);
    char stack[128];
    char *number_text = stack;
    if (len >= sizeof(stack)) {
        number_text = malloc(len + 1);
        if (number_text == NULL) {
            sijson_set_error("out of memory");
            return NULL;
        }
    }
    memcpy(number_text, start, len);
    number_text[len] = '\0';

    errno = 0;
    char *end = NULL;
    double number = strtod(number_text, &end);
    if (end == number_text || *end != '\0' || errno == ERANGE || !isfinite(number)) {
        if (number_text != stack) {
            free(number_text);
        }
        sijson_set_error_at("invalid JSON number", start);
        return NULL;
    }
    if (number_text != stack) {
        free(number_text);
    }

    parser->cur = scan;
    sijson_value_t value = sijson_new_value(SIJSON_NUMBER);
    if (value != NULL) {
        value->as.number = number;
    }
    return value;
}

static sijson_value_t
sijson_parse_literal(sijson_parser_t *parser, const char *literal, sijson_value_t value) {
    size_t len = strlen(literal);
    if (strncmp(parser->cur, literal, len) != 0) {
        sijson_set_error_at("invalid JSON literal", parser->cur);
        return NULL;
    }
    parser->cur += len;
    return value;
}

static sijson_value_t sijson_parse_value(sijson_parser_t *parser) {
    sijson_skip_ws(parser);
    switch (*parser->cur) {
    case 'n':
        return sijson_parse_literal(parser, "null", sijson_new_value(SIJSON_NULL));
    case 't': {
        sijson_value_t value = sijson_new_value(SIJSON_BOOL);
        if (value != NULL) {
            value->as.boolean = true;
        }
        return sijson_parse_literal(parser, "true", value);
    }
    case 'f': {
        sijson_value_t value = sijson_new_value(SIJSON_BOOL);
        if (value != NULL) {
            value->as.boolean = false;
        }
        return sijson_parse_literal(parser, "false", value);
    }
    case '"': {
        char *string = sijson_parse_string_raw(parser);
        if (string == NULL) {
            return NULL;
        }
        sijson_value_t value = sijson_new_value(SIJSON_STRING);
        if (value == NULL) {
            return NULL;
        }
        value->as.string = string;
        return value;
    }
    case '[':
        return sijson_parse_array_value(parser);
    case '{':
        return sijson_parse_object_value(parser);
    default:
        if (*parser->cur == '-' || isdigit((unsigned char)*parser->cur)) {
            return sijson_parse_number_value(parser);
        }
        sijson_set_error_at("expected JSON value", parser->cur);
        return NULL;
    }
}

sijson_value_t sijson_parse(const char *json) {
    sijson_clear_error();
    if (json == NULL) {
        sijson_set_error("sijson_parse expects JSON text");
        return NULL;
    }

    size_t mark = sijson_arena_mark();
    sijson_parser_t parser = { .cur = json };
    sijson_value_t value = sijson_parse_value(&parser);
    if (value == NULL) {
        sijson_arena_rewind(mark);
        return NULL;
    }

    sijson_skip_ws(&parser);
    if (*parser.cur != '\0') {
        sijson_set_error_at("trailing characters after JSON value", parser.cur);
        sijson_arena_rewind(mark);
        return NULL;
    }

    return value;
}

#include <limits.h>

static void *g_from_json_buffer;
static size_t g_from_json_capacity;

static sireflect_handle_t
sijson_register_type(sireflect_handle_t *ref, const sireflect_struct_desc_t *desc) {
    if (ref == NULL || desc == NULL) {
        sijson_set_error("missing reflection descriptor");
        return SIREFLECT_INVALID_HANDLE;
    }
    static const sireflect_struct_desc_t value_desc = {
        .name = "sijson_value_t",
        .fields = "{ ptr value; }",
        .size = sizeof(sijson_value_t),
        .align = _Alignof(sijson_value_t),
    };
    sireflect_register_struct(&value_desc);
    *ref = sireflect_register_struct(desc);
    return *ref;
}

static bool sijson_is_value_type(const sireflect_type_info_t *type) {
    return type != NULL && strcmp(type->name, "sijson_value_t") == 0 &&
           type->size == sizeof(sijson_value_t);
}

static bool sijson_is_char_pointer_type(const sireflect_type_info_t *type) {
    if (type == NULL) {
        return false;
    }
    if (type->kind == sireflect_kind_ptr) {
        return true;
    }
    if (type->kind != sireflect_kind_pointer) {
        return false;
    }

    const sireflect_type_info_t *pointee = sireflect_type_info(type->element_type);
    return pointee != NULL && pointee->kind == sireflect_kind_char;
}

static bool
sijson_write_reflected(sijson_writer_t *writer, sireflect_handle_t type, const void *ptr);

static bool sijson_write_reflected_field(
    sijson_writer_t *writer,
    const sireflect_type_info_t *field_type,
    const void *field_ptr
);

static bool sijson_write_reflected_array(
    sijson_writer_t *writer,
    const sireflect_type_info_t *array_type,
    const void *array_ptr
) {
    if (array_type == NULL || array_type->kind != sireflect_kind_array) {
        return sijson_set_error("expected reflected array type");
    }

    const sireflect_type_info_t *element_type =
        sireflect_type_info(array_type->element_type);
    if (element_type == NULL || element_type->size == 0) {
        return sijson_set_error("missing reflected array element type");
    }

    if (!sijson_writer_putc(writer, '[')) {
        return false;
    }

    for (size_t i = 0; i < array_type->element_count; i++) {
        const void *element_ptr = (const unsigned char *)array_ptr + i * element_type->size;

        if (i != 0 && !sijson_writer_putc(writer, ',')) {
            return false;
        }
        if (!sijson_write_reflected_field(writer, element_type, element_ptr)) {
            return false;
        }
    }

    return sijson_writer_putc(writer, ']');
}

static bool sijson_write_reflected_field(
    sijson_writer_t *writer,
    const sireflect_type_info_t *field_type,
    const void *field_ptr
) {
    if (field_type == NULL) {
        return sijson_set_error("missing reflected field type");
    }

    switch (field_type->kind) {
    case sireflect_kind_bool:
        return sijson_writer_cstr(writer, *(const bool *)field_ptr ? "true" : "false");
    default:
        break;
    }

    char number[64];
    switch (field_type->kind) {
    case sireflect_kind_signed_char:
        snprintf(number, sizeof(number), "%d", (int)*(const signed char *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_unsigned_char:
        snprintf(number, sizeof(number), "%u", (unsigned)*(const unsigned char *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_u8:
        snprintf(number, sizeof(number), "%u", (unsigned)*(const u8 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_u16:
        snprintf(number, sizeof(number), "%u", (unsigned)*(const u16 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_unsigned_short:
        snprintf(number, sizeof(number), "%u", (unsigned)*(const unsigned short *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_u32:
        snprintf(number, sizeof(number), "%u", *(const u32 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_unsigned_int:
        snprintf(number, sizeof(number), "%u", *(const unsigned int *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_u64:
        snprintf(number, sizeof(number), "%llu", (unsigned long long)*(const u64 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_i8:
        snprintf(number, sizeof(number), "%d", (int)*(const i8 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_i16:
        snprintf(number, sizeof(number), "%d", (int)*(const i16 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_i32:
        snprintf(number, sizeof(number), "%d", *(const i32 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_i64:
        snprintf(number, sizeof(number), "%lld", (long long)*(const i64 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_short:
        snprintf(number, sizeof(number), "%d", (int)*(const short *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_int:
        snprintf(number, sizeof(number), "%d", *(const int *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_long:
        snprintf(number, sizeof(number), "%ld", *(const long *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_unsigned_long:
        snprintf(number, sizeof(number), "%lu", *(const unsigned long *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_long_long:
        snprintf(number, sizeof(number), "%lld", *(const long long *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_unsigned_long_long:
        snprintf(number, sizeof(number), "%llu", *(const unsigned long long *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_f32:
        snprintf(number, sizeof(number), "%.9g", (double)*(const f32 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_f64:
        snprintf(number, sizeof(number), "%.17g", *(const f64 *)field_ptr);
        return sijson_writer_cstr(writer, number);
    case sireflect_kind_char:
        return sijson_writer_string(writer, (char[2]){ *(const char *)field_ptr, '\0' });
    case sireflect_kind_ptr:
        return sijson_writer_string(writer, *(char *const *)field_ptr);
    case sireflect_kind_pointer:
        if (sijson_is_char_pointer_type(field_type)) {
            return sijson_writer_string(writer, *(char *const *)field_ptr);
        }
        break;
    case sireflect_kind_struct:
        if (sijson_is_value_type(field_type)) {
            return sijson_write_value(writer, *(const sijson_value_t *)field_ptr);
        }
        return sijson_write_reflected(
            writer,
            sireflect_type_by_name(field_type->name),
            field_ptr
        );
    case sireflect_kind_array:
        return sijson_write_reflected_array(writer, field_type, field_ptr);
    case sireflect_kind_bool:
        break;
    default:
        break;
    }

    return sijson_set_error("unsupported field type for serialization");
}

static bool
sijson_write_reflected(sijson_writer_t *writer, sireflect_handle_t type, const void *ptr) {
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (info == NULL || info->kind != sireflect_kind_struct) {
        return sijson_set_error("expected reflected struct");
    }

    if (!sijson_writer_putc(writer, '{')) {
        return false;
    }

    const sireflect_fields_t *fields = &info->fields;
    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        const sireflect_type_info_t *field_type = sireflect_type_info(field->type);
        const void *field_ptr = (const unsigned char *)ptr + field->offset;

        if (i != 0 && !sijson_writer_putc(writer, ',')) {
            return false;
        }
        if (!sijson_writer_string(writer, field->name) || !sijson_writer_putc(writer, ':')) {
            return false;
        }
        if (!sijson_write_reflected_field(writer, field_type, field_ptr)) {
            return false;
        }
    }

    return sijson_writer_putc(writer, '}');
}

char *
sijson_to_json_impl(sireflect_handle_t *ref, const sireflect_struct_desc_t *desc, const void *ptr) {
    sijson_clear_error();
    if (ptr == NULL) {
        sijson_set_error("sijson_to_json expects a value pointer");
        return NULL;
    }

    sireflect_handle_t type = sijson_register_type(ref, desc);
    if (type == SIREFLECT_INVALID_HANDLE) {
        return NULL;
    }

    sijson_writer_t writer = { 0 };
    if (!sijson_write_reflected(&writer, type, ptr)) {
        free(writer.data);
        return NULL;
    }
    return writer.data;
}

static bool sijson_number_is_integer(double value) {
    if (!isfinite(value) || value < -9007199254740991.0 || value > 9007199254740991.0) {
        return false;
    }

    int64_t integer = (int64_t)value;
    return (double)integer == value;
}

static bool sijson_assign_number(
    const sireflect_type_info_t *field_type,
    void *field_ptr,
    sijson_value_t value
) {
    if (value == NULL || value->type != SIJSON_NUMBER) {
        return sijson_set_error("expected JSON number");
    }

    double number = value->as.number;
    switch (field_type->kind) {
    case sireflect_kind_signed_char:
        if (!sijson_number_is_integer(number) || number < SCHAR_MIN || number > SCHAR_MAX) {
            return sijson_set_error("number out of range for signed char");
        }
        *(signed char *)field_ptr = (signed char)number;
        return true;
    case sireflect_kind_unsigned_char:
        if (!sijson_number_is_integer(number) || number < 0 || number > UCHAR_MAX) {
            return sijson_set_error("number out of range for unsigned char");
        }
        *(unsigned char *)field_ptr = (unsigned char)number;
        return true;
    case sireflect_kind_u8:
        if (!sijson_number_is_integer(number) || number < 0 || number > UINT8_MAX) {
            return sijson_set_error("number out of range for u8");
        }
        *(u8 *)field_ptr = (u8)number;
        return true;
    case sireflect_kind_u16:
        if (!sijson_number_is_integer(number) || number < 0 || number > UINT16_MAX) {
            return sijson_set_error("number out of range for u16");
        }
        *(u16 *)field_ptr = (u16)number;
        return true;
    case sireflect_kind_unsigned_short:
        if (!sijson_number_is_integer(number) || number < 0 || number > USHRT_MAX) {
            return sijson_set_error("number out of range for unsigned short");
        }
        *(unsigned short *)field_ptr = (unsigned short)number;
        return true;
    case sireflect_kind_u32:
        if (!sijson_number_is_integer(number) || number < 0 || number > UINT32_MAX) {
            return sijson_set_error("number out of range for u32");
        }
        *(u32 *)field_ptr = (u32)number;
        return true;
    case sireflect_kind_unsigned_int:
        if (!sijson_number_is_integer(number) || number < 0 || number > UINT_MAX) {
            return sijson_set_error("number out of range for unsigned int");
        }
        *(unsigned int *)field_ptr = (unsigned int)number;
        return true;
    case sireflect_kind_u64:
        if (!sijson_number_is_integer(number) || number < 0) {
            return sijson_set_error("number out of range for u64");
        }
        *(u64 *)field_ptr = (u64)number;
        return true;
    case sireflect_kind_i8:
        if (!sijson_number_is_integer(number) || number < INT8_MIN || number > INT8_MAX) {
            return sijson_set_error("number out of range for i8");
        }
        *(i8 *)field_ptr = (i8)number;
        return true;
    case sireflect_kind_i16:
        if (!sijson_number_is_integer(number) || number < INT16_MIN || number > INT16_MAX) {
            return sijson_set_error("number out of range for i16");
        }
        *(i16 *)field_ptr = (i16)number;
        return true;
    case sireflect_kind_i32:
        if (!sijson_number_is_integer(number) || number < INT32_MIN || number > INT32_MAX) {
            return sijson_set_error("number out of range for i32");
        }
        *(i32 *)field_ptr = (i32)number;
        return true;
    case sireflect_kind_i64:
        if (!sijson_number_is_integer(number)) {
            return sijson_set_error("number out of range for i64");
        }
        *(i64 *)field_ptr = (i64)number;
        return true;
    case sireflect_kind_short:
        if (!sijson_number_is_integer(number)) {
            return sijson_set_error("expected integer for short");
        }
        *(short *)field_ptr = (short)number;
        return true;
    case sireflect_kind_int:
        if (!sijson_number_is_integer(number)) {
            return sijson_set_error("expected integer for int");
        }
        *(int *)field_ptr = (int)number;
        return true;
    case sireflect_kind_long:
        if (!sijson_number_is_integer(number)) {
            return sijson_set_error("expected integer for long");
        }
        *(long *)field_ptr = (long)number;
        return true;
    case sireflect_kind_unsigned_long:
        if (!sijson_number_is_integer(number) || number < 0) {
            return sijson_set_error("number out of range for unsigned long");
        }
        *(unsigned long *)field_ptr = (unsigned long)number;
        return true;
    case sireflect_kind_long_long:
        if (!sijson_number_is_integer(number)) {
            return sijson_set_error("number out of range for long long");
        }
        *(long long *)field_ptr = (long long)number;
        return true;
    case sireflect_kind_unsigned_long_long:
        if (!sijson_number_is_integer(number) || number < 0) {
            return sijson_set_error("number out of range for unsigned long long");
        }
        *(unsigned long long *)field_ptr = (unsigned long long)number;
        return true;
    case sireflect_kind_f32:
        *(f32 *)field_ptr = (f32)number;
        return true;
    case sireflect_kind_f64:
        *(f64 *)field_ptr = number;
        return true;
    default:
        return sijson_set_error("field is not numeric");
    }
}

static bool sijson_assign_reflected(sireflect_handle_t type, void *ptr, sijson_value_t value);

static bool
sijson_assign_field(const sireflect_type_info_t *field_type, void *field_ptr, sijson_value_t value);

static bool sijson_assign_array(
    const sireflect_type_info_t *array_type,
    void *array_ptr,
    sijson_value_t value
) {
    if (array_type == NULL || array_type->kind != sireflect_kind_array) {
        return sijson_set_error("expected reflected array type");
    }
    if (value == NULL || value->type != SIJSON_ARRAY) {
        return sijson_set_error("expected JSON array");
    }
    if (value->as.array.len != array_type->element_count) {
        return sijson_set_error("JSON array length does not match reflected array");
    }

    const sireflect_type_info_t *element_type =
        sireflect_type_info(array_type->element_type);
    if (element_type == NULL || element_type->size == 0) {
        return sijson_set_error("missing reflected array element type");
    }

    for (size_t i = 0; i < array_type->element_count; i++) {
        void *element_ptr = (unsigned char *)array_ptr + i * element_type->size;
        if (!sijson_assign_field(element_type, element_ptr, value->as.array.items[i])) {
            return false;
        }
    }

    return true;
}

static bool sijson_assign_field(
    const sireflect_type_info_t *field_type,
    void *field_ptr,
    sijson_value_t value
) {
    if (field_type == NULL) {
        return sijson_set_error("missing reflected field type");
    }

    switch (field_type->kind) {
    case sireflect_kind_bool:
        if (value == NULL || value->type != SIJSON_BOOL) {
            return sijson_set_error("expected JSON bool");
        }
        *(bool *)field_ptr = value->as.boolean;
        return true;
    case sireflect_kind_signed_char:
    case sireflect_kind_unsigned_char:
    case sireflect_kind_u8:
    case sireflect_kind_u16:
    case sireflect_kind_u32:
    case sireflect_kind_u64:
    case sireflect_kind_i8:
    case sireflect_kind_i16:
    case sireflect_kind_i32:
    case sireflect_kind_i64:
    case sireflect_kind_unsigned_short:
    case sireflect_kind_short:
    case sireflect_kind_unsigned_int:
    case sireflect_kind_int:
    case sireflect_kind_unsigned_long:
    case sireflect_kind_long:
    case sireflect_kind_long_long:
    case sireflect_kind_unsigned_long_long:
    case sireflect_kind_f32:
    case sireflect_kind_f64:
        return sijson_assign_number(field_type, field_ptr, value);
    case sireflect_kind_char:
        if (value == NULL || value->type != SIJSON_STRING || value->as.string[0] == '\0') {
            return sijson_set_error("expected non-empty JSON string for char");
        }
        *(char *)field_ptr = value->as.string[0];
        return true;
    case sireflect_kind_ptr:
    case sireflect_kind_pointer:
        if (!sijson_is_char_pointer_type(field_type)) {
            break;
        }
        if (value == NULL || value->type == SIJSON_NULL) {
            *(char **)field_ptr = NULL;
            return true;
        }
        if (value->type != SIJSON_STRING) {
            return sijson_set_error("expected JSON string for pointer field");
        }
        *(char **)field_ptr = sijson_dup_cstr(value->as.string);
        return *(char **)field_ptr != NULL;
    case sireflect_kind_struct:
        if (sijson_is_value_type(field_type)) {
            *(sijson_value_t *)field_ptr = value;
            return true;
        }
        return sijson_assign_reflected(
            sireflect_type_by_name(field_type->name),
            field_ptr,
            value
        );
    case sireflect_kind_array:
        return sijson_assign_array(field_type, field_ptr, value);
    default:
        break;
    }

    return sijson_set_error("unsupported field type for deserialization");
}

static bool sijson_assign_reflected(sireflect_handle_t type, void *ptr, sijson_value_t value) {
    if (value == NULL || value->type != SIJSON_OBJECT) {
        return sijson_set_error("expected JSON object for reflected struct");
    }

    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (info == NULL || info->kind != sireflect_kind_struct) {
        return sijson_set_error("expected reflected struct type");
    }

    memset(ptr, 0, info->size);
    const sireflect_fields_t *fields = &info->fields;
    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        sijson_value_t member = sijson_object_get(value, field->name);
        if (member == NULL) {
            continue;
        }

        const sireflect_type_info_t *field_type = sireflect_type_info(field->type);
        void *field_ptr = (unsigned char *)ptr + field->offset;
        if (!sijson_assign_field(field_type, field_ptr, member)) {
            return false;
        }
    }

    return true;
}

void *sijson_from_json_impl(
    sireflect_handle_t *ref,
    const sireflect_struct_desc_t *desc,
    const char *json
) {
    sijson_clear_error();
    sireflect_handle_t type = sijson_register_type(ref, desc);
    if (type == SIREFLECT_INVALID_HANDLE) {
        return NULL;
    }

    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (info == NULL) {
        sijson_set_error("missing reflected type info");
        return NULL;
    }

    if (g_from_json_capacity < info->size) {
        void *buffer = realloc(g_from_json_buffer, info->size);
        if (buffer == NULL) {
            sijson_set_error("out of memory");
            return NULL;
        }
        g_from_json_buffer = buffer;
        g_from_json_capacity = info->size;
    }

    memset(g_from_json_buffer, 0, info->size);
    sijson_value_t root = sijson_parse(json);
    if (root == NULL) {
        memset(g_from_json_buffer, 0, info->size);
        return g_from_json_buffer;
    }

    if (!sijson_assign_reflected(type, g_from_json_buffer, root)) {
        memset(g_from_json_buffer, 0, info->size);
        return g_from_json_buffer;
    }

    return g_from_json_buffer;
}

static void sijson_free_reflected_field(const sireflect_type_info_t *field_type, void *field_ptr);

static void sijson_free_reflected(sireflect_handle_t type, void *ptr) {
    if (ptr == NULL) {
        return;
    }

    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (info == NULL || info->kind != sireflect_kind_struct || sijson_is_value_type(info)) {
        return;
    }

    const sireflect_fields_t *fields = &info->fields;
    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        const sireflect_type_info_t *field_type = sireflect_type_info(field->type);
        void *field_ptr = (unsigned char *)ptr + field->offset;

        sijson_free_reflected_field(field_type, field_ptr);
    }
}

static void sijson_free_reflected_field(const sireflect_type_info_t *field_type, void *field_ptr) {
    if (field_type == NULL || field_ptr == NULL) {
        return;
    }

    switch (field_type->kind) {
    case sireflect_kind_ptr:
        free(*(void **)field_ptr);
        *(void **)field_ptr = NULL;
        return;
    case sireflect_kind_pointer:
        if (sijson_is_char_pointer_type(field_type)) {
            free(*(void **)field_ptr);
            *(void **)field_ptr = NULL;
        }
        return;
    case sireflect_kind_struct:
        if (!sijson_is_value_type(field_type)) {
            sijson_free_reflected(sireflect_type_by_name(field_type->name), field_ptr);
        }
        return;
    case sireflect_kind_array: {
        const sireflect_type_info_t *element_type =
            sireflect_type_info(field_type->element_type);
        if (element_type == NULL || element_type->size == 0) {
            return;
        }
        for (size_t i = 0; i < field_type->element_count; i++) {
            void *element_ptr = (unsigned char *)field_ptr + i * element_type->size;
            sijson_free_reflected_field(element_type, element_ptr);
        }
        return;
    }
    case sireflect_kind_u8:
    case sireflect_kind_u16:
    case sireflect_kind_u32:
    case sireflect_kind_u64:
    case sireflect_kind_i8:
    case sireflect_kind_i16:
    case sireflect_kind_i32:
    case sireflect_kind_i64:
    case sireflect_kind_signed_char:
    case sireflect_kind_unsigned_char:
    case sireflect_kind_unsigned_short:
    case sireflect_kind_unsigned_int:
    case sireflect_kind_unsigned_long:
    case sireflect_kind_long_long:
    case sireflect_kind_unsigned_long_long:
    case sireflect_kind_f32:
    case sireflect_kind_f64:
    case sireflect_kind_bool:
    case sireflect_kind_char:
    case sireflect_kind_short:
    case sireflect_kind_int:
    case sireflect_kind_long:
    case sireflect_kind_function_pointer:
        return;
    }
}

void sijson_free_impl(sireflect_handle_t *ref, const sireflect_struct_desc_t *desc, void *ptr) {
    sijson_clear_error();
    sireflect_handle_t type = sijson_register_type(ref, desc);
    if (type == SIREFLECT_INVALID_HANDLE) {
        return;
    }
    sijson_free_reflected(type, ptr);
}

#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifndef SIJSON_ARENA_RESERVE
#define SIJSON_ARENA_RESERVE ((size_t)1 << 30)
#endif

#ifndef SIJSON_ARENA_FALLBACK_RESERVE
#define SIJSON_ARENA_FALLBACK_RESERVE ((size_t)1 << 20)
#endif

typedef struct sijson_arena {
    unsigned char *data;
    size_t used;
    size_t cap;
    size_t reserve;
    bool mmap_backed;
} sijson_arena_t;

static sijson_arena_t g_arena;

static size_t sijson_align_forward(size_t value, size_t align) {
    size_t mask = align - 1;
    return (value + mask) & ~mask;
}

static size_t sijson_page_size(void) {
#if defined(__unix__) || defined(__APPLE__)
    long page = sysconf(_SC_PAGESIZE);
    if (page > 0) {
        return (size_t)page;
    }
#endif
    return 4096;
}

static bool sijson_arena_init(size_t need) {
    if (g_arena.data != NULL) {
        return true;
    }

    size_t page_size = sijson_page_size();
    size_t reserve = SIJSON_ARENA_RESERVE;
    if (reserve < need) {
        reserve = sijson_align_forward(need, page_size);
    }

#if defined(__unix__) || defined(__APPLE__)
    void *data = mmap(NULL, reserve, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (data != MAP_FAILED) {
        g_arena.data = data;
        g_arena.reserve = reserve;
        g_arena.mmap_backed = true;
        return true;
    }
#endif

    size_t fallback_reserve = reserve;
    if (need <= SIJSON_ARENA_FALLBACK_RESERVE && fallback_reserve > SIJSON_ARENA_FALLBACK_RESERVE) {
        fallback_reserve = SIJSON_ARENA_FALLBACK_RESERVE;
    }

    g_arena.data = malloc(fallback_reserve);
    if (g_arena.data == NULL) {
        sijson_set_error("out of memory");
        return false;
    }
    g_arena.cap = fallback_reserve;
    g_arena.reserve = fallback_reserve;
    return true;
}

static bool sijson_arena_commit(size_t need) {
    if (!sijson_arena_init(need)) {
        return false;
    }
    if (need <= g_arena.cap) {
        return true;
    }
    if (need > g_arena.reserve) {
        return sijson_set_error("sijson arena capacity exceeded");
    }

    size_t page_size = sijson_page_size();
    size_t cap = sijson_align_forward(need, page_size);

    if (g_arena.mmap_backed) {
#if defined(__unix__) || defined(__APPLE__)
        if (mprotect(g_arena.data + g_arena.cap, cap - g_arena.cap, PROT_READ | PROT_WRITE) != 0) {
            return sijson_set_error("out of memory");
        }
#else
        return sijson_set_error("sijson arena backend unavailable");
#endif
    }

    g_arena.cap = cap;
    return true;
}

void *sijson_arena_alloc(size_t size, size_t align) {
    if (align == 0) {
        align = _Alignof(sijson_max_align_t);
    }

    size_t offset = sijson_align_forward(g_arena.used, align);
    if (offset < g_arena.used || size > SIZE_MAX - offset) {
        sijson_set_error("out of memory");
        return NULL;
    }

    size_t need = offset + size;
    if (!sijson_arena_commit(need)) {
        return NULL;
    }

    void *ptr = g_arena.data + offset;
    g_arena.used = need;
    return ptr;
}

char *sijson_arena_dup_range(const char *start, size_t len) {
    char *result = sijson_arena_alloc(len + 1, _Alignof(char));
    if (result == NULL) {
        return NULL;
    }

    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

char *sijson_arena_dup_cstr(const char *str) {
    if (str == NULL) {
        return NULL;
    }

    return sijson_arena_dup_range(str, strlen(str));
}

size_t sijson_arena_mark(void) {
    return g_arena.used;
}

void sijson_arena_rewind(size_t mark) {
    if (mark <= g_arena.used) {
        g_arena.used = mark;
    }
}

void sijson_clean(void) {
    sijson_clear_error();
    g_arena.used = 0;
}

void sijson_release(void) {
    sijson_clear_error();
    if (g_arena.mmap_backed) {
#if defined(__unix__) || defined(__APPLE__)
        munmap(g_arena.data, g_arena.reserve);
#endif
    } else {
        free(g_arena.data);
    }
    g_arena = (sijson_arena_t){ 0 };
}

char *sijson_dup_range(const char *start, size_t len) {
    char *result = malloc(len + 1);
    if (result == NULL) {
        sijson_set_error("out of memory");
        return NULL;
    }

    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

char *sijson_dup_cstr(const char *str) {
    if (str == NULL) {
        return NULL;
    }

    return sijson_dup_range(str, strlen(str));
}

bool sijson_reserve_array(sijson_array_t *array, size_t need) {
    if (array->cap >= need) {
        return true;
    }

    size_t cap = array->cap != 0 ? array->cap * 2 : 8;
    while (cap < need) {
        cap *= 2;
    }

    sijson_value_t *items = sijson_arena_alloc(cap * sizeof(*items), _Alignof(sijson_value_t));
    if (items == NULL) {
        return false;
    }
    if (array->items != NULL) {
        memcpy(items, array->items, array->len * sizeof(*items));
    }

    array->items = items;
    array->cap = cap;
    return true;
}

bool sijson_reserve_object(sijson_object_t *object, size_t need) {
    if (object->cap >= need) {
        return true;
    }

    size_t cap = object->cap != 0 ? object->cap * 2 : 8;
    while (cap < need) {
        cap *= 2;
    }

    sijson_member_t *items = sijson_arena_alloc(cap * sizeof(*items), _Alignof(sijson_member_t));
    if (items == NULL) {
        return false;
    }
    if (object->items != NULL) {
        memcpy(items, object->items, object->len * sizeof(*items));
    }

    object->items = items;
    object->cap = cap;
    return true;
}

sijson_value_t sijson_new_value(sijson_type_t type) {
    sijson_value_t value = sijson_arena_alloc(sizeof(*value), _Alignof(struct sijson_value));
    if (value == NULL) {
        return NULL;
    }

    memset(value, 0, sizeof(*value));
    value->type = type;
    return value;
}

sijson_value_t sijson_make_null(void) {
    sijson_clear_error();
    return sijson_new_value(SIJSON_NULL);
}

sijson_value_t sijson_make_bool(bool value) {
    sijson_clear_error();
    sijson_value_t result = sijson_new_value(SIJSON_BOOL);
    if (result != NULL) {
        result->as.boolean = value;
    }
    return result;
}

sijson_value_t sijson_make_number(double value) {
    sijson_clear_error();
    sijson_value_t result = sijson_new_value(SIJSON_NUMBER);
    if (result != NULL) {
        result->as.number = value;
    }
    return result;
}

sijson_value_t sijson_make_string(const char *value) {
    sijson_clear_error();
    sijson_value_t result = sijson_new_value(SIJSON_STRING);
    if (result == NULL) {
        return NULL;
    }

    result->as.string = sijson_arena_dup_cstr(value != NULL ? value : "");
    if (result->as.string == NULL) {
        return NULL;
    }

    return result;
}

sijson_value_t sijson_make_array(void) {
    sijson_clear_error();
    return sijson_new_value(SIJSON_ARRAY);
}

sijson_value_t sijson_make_object(void) {
    sijson_clear_error();
    return sijson_new_value(SIJSON_OBJECT);
}

sijson_type_t sijson_type(sijson_value_t value) {
    return value != NULL ? value->type : SIJSON_NULL;
}

bool sijson_bool(sijson_value_t value) {
    return value != NULL && value->type == SIJSON_BOOL ? value->as.boolean : false;
}

double sijson_number(sijson_value_t value) {
    return value != NULL && value->type == SIJSON_NUMBER ? value->as.number : 0.0;
}

const char *sijson_string(sijson_value_t value) {
    return value != NULL && value->type == SIJSON_STRING ? value->as.string : NULL;
}

size_t sijson_array_len(sijson_value_t value) {
    return value != NULL && value->type == SIJSON_ARRAY ? value->as.array.len : 0;
}

sijson_value_t sijson_array_get(sijson_value_t value, size_t index) {
    if (value == NULL || value->type != SIJSON_ARRAY || index >= value->as.array.len) {
        return NULL;
    }

    return value->as.array.items[index];
}

size_t sijson_object_len(sijson_value_t value) {
    return value != NULL && value->type == SIJSON_OBJECT ? value->as.object.len : 0;
}

const char *sijson_object_key(sijson_value_t value, size_t index) {
    if (value == NULL || value->type != SIJSON_OBJECT || index >= value->as.object.len) {
        return NULL;
    }

    return value->as.object.items[index].key;
}

sijson_value_t sijson_object_get(sijson_value_t value, const char *key) {
    if (value == NULL || value->type != SIJSON_OBJECT || key == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < value->as.object.len; i++) {
        if (strcmp(value->as.object.items[i].key, key) == 0) {
            return value->as.object.items[i].value;
        }
    }

    return NULL;
}

bool sijson_array_push(sijson_value_t array, sijson_value_t value) {
    sijson_clear_error();
    if (array == NULL || array->type != SIJSON_ARRAY) {
        return sijson_set_error("sijson_array_push expects an array");
    }

    if (!sijson_reserve_array(&array->as.array, array->as.array.len + 1)) {
        return false;
    }

    array->as.array.items[array->as.array.len++] = value;
    return true;
}

bool sijson_object_set(sijson_value_t object, const char *key, sijson_value_t value) {
    sijson_clear_error();
    if (object == NULL || object->type != SIJSON_OBJECT) {
        return sijson_set_error("sijson_object_set expects an object");
    }
    if (key == NULL) {
        return sijson_set_error("sijson_object_set expects a key");
    }

    for (size_t i = 0; i < object->as.object.len; i++) {
        if (strcmp(object->as.object.items[i].key, key) == 0) {
            object->as.object.items[i].value = value;
            return true;
        }
    }

    if (!sijson_reserve_object(&object->as.object, object->as.object.len + 1)) {
        return false;
    }

    char *owned_key = sijson_arena_dup_cstr(key);
    if (owned_key == NULL) {
        return false;
    }

    object->as.object.items[object->as.object.len++] = (sijson_member_t){
        .key = owned_key,
        .value = value,
    };
    return true;
}

bool sijson_writer_reserve(sijson_writer_t *writer, size_t extra) {
    if (writer->len + extra + 1 <= writer->cap) {
        return true;
    }

    size_t cap = writer->cap != 0 ? writer->cap * 2 : 128;
    while (cap < writer->len + extra + 1) {
        cap *= 2;
    }

    char *data = realloc(writer->data, cap);
    if (data == NULL) {
        return sijson_set_error("out of memory");
    }

    writer->data = data;
    writer->cap = cap;
    return true;
}

bool sijson_writer_putc(sijson_writer_t *writer, char c) {
    if (!sijson_writer_reserve(writer, 1)) {
        return false;
    }

    writer->data[writer->len++] = c;
    writer->data[writer->len] = '\0';
    return true;
}

bool sijson_writer_write(sijson_writer_t *writer, const char *data, size_t len) {
    if (!sijson_writer_reserve(writer, len)) {
        return false;
    }

    memcpy(writer->data + writer->len, data, len);
    writer->len += len;
    writer->data[writer->len] = '\0';
    return true;
}

bool sijson_writer_cstr(sijson_writer_t *writer, const char *str) {
    return sijson_writer_write(writer, str, strlen(str));
}

bool sijson_writer_string(sijson_writer_t *writer, const char *str) {
    if (str == NULL) {
        return sijson_writer_cstr(writer, "null");
    }

    if (!sijson_writer_putc(writer, '"')) {
        return false;
    }

    const unsigned char *cur = (const unsigned char *)str;
    const unsigned char *chunk = cur;
    while (*cur != '\0') {
        unsigned char c = *cur;
        if (c == '"' || c == '\\' || c < 0x20) {
            if (!sijson_writer_write(writer, (const char *)chunk, (size_t)(cur - chunk))) {
                return false;
            }

            switch (c) {
            case '"':
                if (!sijson_writer_cstr(writer, "\\\"")) {
                    return false;
                }
                break;
            case '\\':
                if (!sijson_writer_cstr(writer, "\\\\")) {
                    return false;
                }
                break;
            case '\b':
                if (!sijson_writer_cstr(writer, "\\b")) {
                    return false;
                }
                break;
            case '\f':
                if (!sijson_writer_cstr(writer, "\\f")) {
                    return false;
                }
                break;
            case '\n':
                if (!sijson_writer_cstr(writer, "\\n")) {
                    return false;
                }
                break;
            case '\r':
                if (!sijson_writer_cstr(writer, "\\r")) {
                    return false;
                }
                break;
            case '\t':
                if (!sijson_writer_cstr(writer, "\\t")) {
                    return false;
                }
                break;
            default: {
                char escape[7];
                snprintf(escape, sizeof(escape), "\\u%04x", c);
                if (!sijson_writer_cstr(writer, escape)) {
                    return false;
                }
                break;
            }
            }

            cur++;
            chunk = cur;
            continue;
        }
        cur++;
    }

    if (!sijson_writer_write(writer, (const char *)chunk, (size_t)(cur - chunk))) {
        return false;
    }

    return sijson_writer_putc(writer, '"');
}

bool sijson_write_value(sijson_writer_t *writer, sijson_value_t value);

static bool sijson_write_array(sijson_writer_t *writer, sijson_value_t value) {
    if (!sijson_writer_putc(writer, '[')) {
        return false;
    }

    for (size_t i = 0; i < value->as.array.len; i++) {
        if (i != 0 && !sijson_writer_putc(writer, ',')) {
            return false;
        }
        if (!sijson_write_value(writer, value->as.array.items[i])) {
            return false;
        }
    }

    return sijson_writer_putc(writer, ']');
}

static bool sijson_write_object(sijson_writer_t *writer, sijson_value_t value) {
    if (!sijson_writer_putc(writer, '{')) {
        return false;
    }

    for (size_t i = 0; i < value->as.object.len; i++) {
        if (i != 0 && !sijson_writer_putc(writer, ',')) {
            return false;
        }
        if (!sijson_writer_string(writer, value->as.object.items[i].key)) {
            return false;
        }
        if (!sijson_writer_putc(writer, ':')) {
            return false;
        }
        if (!sijson_write_value(writer, value->as.object.items[i].value)) {
            return false;
        }
    }

    return sijson_writer_putc(writer, '}');
}

bool sijson_write_value(sijson_writer_t *writer, sijson_value_t value) {
    if (value == NULL) {
        return sijson_writer_cstr(writer, "null");
    }

    switch (value->type) {
    case SIJSON_NULL:
        return sijson_writer_cstr(writer, "null");
    case SIJSON_BOOL:
        return sijson_writer_cstr(writer, value->as.boolean ? "true" : "false");
    case SIJSON_NUMBER: {
        if (!isfinite(value->as.number)) {
            return sijson_set_error("cannot write non-finite JSON number");
        }
        char number[64];
        int len = snprintf(number, sizeof(number), "%.17g", value->as.number);
        if (len < 0 || (size_t)len >= sizeof(number)) {
            return sijson_set_error("failed to format JSON number");
        }
        return sijson_writer_write(writer, number, (size_t)len);
    }
    case SIJSON_STRING:
        return sijson_writer_string(writer, value->as.string);
    case SIJSON_ARRAY:
        return sijson_write_array(writer, value);
    case SIJSON_OBJECT:
        return sijson_write_object(writer, value);
    }

    return sijson_set_error("unknown JSON value type");
}

char *sijson_stringify(sijson_value_t value) {
    sijson_clear_error();
    sijson_writer_t writer = { 0 };
    if (!sijson_write_value(&writer, value)) {
        free(writer.data);
        return NULL;
    }
    if (writer.data == NULL) {
        return sijson_dup_cstr("");
    }
    return writer.data;
}

#ifndef SIECS_HELPER_H
#define SIECS_HELPER_H

#if defined(_MSC_VER)
#include <intrin.h>

#define ECS_LIKELY(x) (!!(x))
#define ECS_UNLIKELY(x) (!!(x))

static inline unsigned ecs_ctz(unsigned value) {
    unsigned long index;
    _BitScanForward(&index, value);
    return (unsigned)index;
}

#define ECS_CTZ(x) ecs_ctz((unsigned)(x))
#else
#define ECS_LIKELY(x) __builtin_expect(!!(x), 1)
#define ECS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ECS_CTZ(x) __builtin_ctz((unsigned)(x))
#endif

#define ecs_entity(index, generation) (((uint64_t)(index) << 32) | (generation & 0xffffffff))

#define ecs_first(id) ((uint32_t)((id) >> 32))
#define ecs_second(id) ((uint32_t)((id) & 0xffffffff))

#endif

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
    uint16_t aux; /* free metadata stored in the pointer-alignment gap */
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
    uint16_t key;
    uint64_t value;
} ecs_type_pair_t;

typedef struct {
    uint16_t *ids;
    uint16_t component_count;
    uint16_t pair_count;
    uint32_t hash;
    ecs_entity_t base;
} ecs_type_t;

ecs_type_t ecs_type_with(
    const ecs_type_t *type,
    ecs_component_t component,
    ecs_type_pair_t pair
);
ecs_type_t ecs_type_without(
    const ecs_type_t *type,
    uint16_t component_index,
    uint16_t pair_key
);
ecs_type_t ecs_type_with_ids(const ecs_type_t *type, const uint16_t *ids, uint16_t count);
ecs_type_t ecs_type_with_added_ids(
    const ecs_type_t *type,
    const ecs_component_t *ids,
    uint16_t count
);
ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base);

static inline ecs_type_pair_t *ecs_type_pairs(const ecs_type_t *type) {
    uintptr_t end = (uintptr_t)type->ids +
                    (uintptr_t)type->component_count * sizeof(uint16_t);
    return (ecs_type_pair_t *)((end + _Alignof(ecs_type_pair_t) - 1) &
                               ~(uintptr_t)(_Alignof(ecs_type_pair_t) - 1));
}

static inline uint16_t ecs_type_pair_index(const ecs_type_t *type, uint16_t key) {
    const ecs_type_pair_t *pairs = ecs_type_pairs(type);
    for (uint16_t i = 0; i < type->pair_count; i++) {
        if (pairs[i].key >= key) {
            return pairs[i].key == key ? i : UINT16_MAX;
        }
    }
    return UINT16_MAX;
}

static inline uint64_t ecs_type_pair_get(const ecs_type_t *type, uint16_t key) {
    uint16_t index = ecs_type_pair_index(type, key);
    return index == UINT16_MAX ? 0 : ecs_type_pairs(type)[index].value;
}

uint64_t ecs_type_bloom(const ecs_type_t *type);
void ecs_type_fini(ecs_type_t *type);

static inline int ecs_type_equals(const ecs_type_t *a, const ecs_type_t *b) {
    if (a->base != b->base || a->component_count != b->component_count ||
        a->pair_count != b->pair_count) {
        return 0;
    }
    if (a->component_count &&
        memcmp(a->ids, b->ids, (size_t)a->component_count * sizeof(uint16_t)) != 0) {
        return 0;
    }
    const ecs_type_pair_t *ap = ecs_type_pairs(a);
    const ecs_type_pair_t *bp = ecs_type_pairs(b);
    for (uint16_t i = 0; i < a->pair_count; i++) {
        if (ap[i].key != bp[i].key || ap[i].value != bp[i].value) {
            return 0;
        }
    }
    return 1;
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

struct ecs_table_s {
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
};

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
    if (column_index < table->type.component_count &&
        table->type.ids[column_index] == component_id) {
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
    uint64_t value;
    uint16_t *tables;
    uint16_t key;
    uint16_t table_count;
    uint16_t table_capacity;
    uint16_t first_table;
} ecs_pair_table_slot_t;

typedef struct {
    const uint16_t *ids;
    uint16_t count;
} ecs_pair_tables_t;

typedef struct {
    ecs_table_t *tables;
    ecs_type_slot_t *slots;
    uint16_t table_count;
    uint16_t table_capacity;
    uint8_t slot_shift; // slot_count = 1 << slot_shift
    ecs_pair_table_slot_t *pair_slots;
    uint32_t pair_slot_count;
    uint8_t pair_slot_shift;
} ecs_table_index_t;

extern ecs_table_index_t table_index;

void ecs_table_index_init();
void ecs_table_index_fini();

#define ecs_table_index_at(index) (&table_index.tables[index])

uint16_t ecs_table_index_get_or_create(
    ecs_type_t type
);
ecs_pair_tables_t ecs_table_index_pair_tables(uint16_t key, uint64_t value);

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
#define ecs_assert_entity_alive(entity) \
    do { ecs_assert_entity_valid(entity); ecs_assert_is_alive(entity); } while (0)
#define ecs_assert_component_access(entity, id) \
    do { ecs_assert_id_valid(id); ecs_assert_entity_alive(entity); } while (0)

#else
#define ecs_assert(condition, ...)
#define ecs_assert_id_valid(id)
#define ecs_assert_not_null(ptr)
#define ecs_assert_entity_valid(entity)
#define ecs_assert_is_alive(entity)
#define ecs_assert_entity_alive(entity)
#define ecs_assert_component_access(entity, id)
#endif

#endif

#ifndef SIECS_WORLD_INTERNAL_H
#define SIECS_WORLD_INTERNAL_H
#ifndef SIECS_COMMAND_BUFFER_H
#define SIECS_COMMAND_BUFFER_H

#ifndef ECS_ARENA_H
#define ECS_ARENA_H

#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER)
/* MSVC's C headers do not expose max_align_t in the C language mode. */
typedef __declspec(align(16)) struct {
    unsigned char value[16];
} ecs_arena_max_align_t;
#else
typedef max_align_t ecs_arena_max_align_t;
#endif

typedef struct ecs_arena_block_s {
    struct ecs_arena_block_s *next;
    uint32_t capacity;
    uint32_t cursor;
    ecs_arena_max_align_t data[];
} ecs_arena_block_t;

typedef struct {
    ecs_arena_block_t *first;
    ecs_arena_block_t *current;
    ecs_arena_block_t *last;
} ecs_arena_t;

void ecs_arena_init(ecs_arena_t *allocator);
void ecs_arena_fini(ecs_arena_t *allocator);
void *ecs_arena_alloc_slow(ecs_arena_t *allocator, uint32_t size);

static inline void *ecs_arena_alloc(ecs_arena_t *allocator, uint32_t size) {
    ecs_arena_block_t *block = allocator->current;
    const uint32_t alignment = (uint32_t)_Alignof(ecs_arena_max_align_t);
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

#include <stdbool.h>
#include <stdint.h>

typedef struct ecs_world_s ecs_world_t;

typedef enum {
    EcsDeferredAdd,
    EcsDeferredRemove,
    EcsDeferredCopy,
    EcsDeferredMove,
} ecs_deferred_op_t;

typedef struct {
    ecs_component_t id;
    ecs_deferred_op_t op;
    void *data;
} ecs_deferred_change_t;

typedef struct {
    ecs_entity_t target; /* 0 removes the relation */
    uint32_t next;
    ecs_relation_id_t id;
} ecs_deferred_relation_t;

typedef struct {
    ecs_entity_t entity;
    uint32_t relation_head;
    bool kill;
    bool has_base;
    ecs_entity_t base;
    sicore_vec_t changes;
} ecs_entity_command_t;

typedef struct ecs_command_buffer_s {
    sicore_vec_t commands;
    sicore_vec_t relations;
    uint32_t *entity_to_command;
    uint32_t entity_capacity;
    ecs_arena_t *arena;
} ecs_command_buffer_t;

typedef struct ecs_execution_context_s {
    ecs_command_buffer_t commands;
    ecs_arena_t arena;
    uint32_t defer_depth;
    bool flushing_commands;
    bool scheduler_parallel;
} ecs_execution_context_t;

void ecs_execution_context_init(ecs_execution_context_t *context);
void ecs_execution_context_fini(ecs_execution_context_t *context);
ecs_execution_context_t *ecs_execution_context_current(void);
void ecs_execution_context_set(ecs_execution_context_t *context);

void ecs_command_buffer_init(ecs_command_buffer_t *buffer, ecs_arena_t *arena);
void ecs_command_buffer_fini(ecs_command_buffer_t *buffer);

void ecs_command_buffer_add(ecs_entity_t entity, ecs_component_t id);
void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id);
void ecs_command_buffer_set(ecs_entity_t entity, ecs_component_t id, const void *data);
void ecs_command_buffer_move(ecs_entity_t entity, ecs_component_t id, void *data);
void ecs_command_buffer_kill(ecs_entity_t entity);
void ecs_command_buffer_set_base(ecs_entity_t entity, ecs_entity_t target);
void ecs_command_buffer_relate(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_entity_t target
);
void ecs_command_buffer_flush();
void ecs_command_buffer_flush_buffer(ecs_command_buffer_t *buffer);

void ecs_add_cid_now(ecs_entity_t entity, ecs_component_t id);
void ecs_remove_cid_now(ecs_entity_t entity, ecs_component_t id);
void ecs_set_cid_now(ecs_entity_t entity, ecs_component_t id, const void *data);
void ecs_move_cid_now(ecs_entity_t entity, ecs_component_t id, void *data);
void ecs_kill_now(ecs_entity_t entity);
void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target);

#endif

#ifndef SIECS_STORAGE_COMPONENT_INDEX_H
#define SIECS_STORAGE_COMPONENT_INDEX_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define ECS_COMPONENT_REQUIRE_CAPACITY 32

typedef enum {
    EcsComponentRelationTarget = 1 << 0,
    EcsComponentRelationSource = 1 << 1,
} ecs_component_internal_flags_t;

typedef struct {
    ecs_relation_id_t relation;
    ecs_entity_t target;
} ecs_component_required_relation_t;

#define ECS_COMPONENT_RELATION_ID(flags) ((ecs_relation_id_t)((flags) >> 16))
#define ECS_COMPONENT_RELATION_FLAGS(id, flags) ((uint32_t)(flags) | ((uint32_t)(id) << 16))

typedef struct {
    ecs_component_info_t *info;
    uint16_t *required;
    uint32_t required_count;
    ecs_component_required_relation_t *default_relations;
    uint16_t default_relation_count;
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

extern ecs_component_index_t component_index;

void ecs_component_index_register(
    ecs_component_t id,
    const char *name,
    uint64_t size,
    ecs_type_ops_t ops,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    ecs_component_inheritance_t inheritance,
    uint32_t relation_flags,
    sireflect_handle_t type,
    const sireflect_struct_desc_t *reflection_desc
);

void ecs_component_index_init();
void ecs_component_index_fini();

ecs_component_record_t *ecs_component_index_get(ecs_component_t cid);

static inline ecs_component_required_relation_t *ecs_component_default_relations(
    ecs_component_t component
) {
    return ecs_component_index_get(component)->default_relations;
}

static inline void ecs_component_value_copy(
    const ecs_component_record_t *record,
    void *dst, const void *src, uint32_t count
) {
    if (!record->info->size) return;
    if (record->ops.copy) record->ops.copy(dst, src, count);
    else memcpy(dst, src, (size_t)record->info->size * count);
}

static inline void ecs_component_value_copy_ctor(
    const ecs_component_record_t *record,
    void *dst, const void *src, uint32_t count
) {
    if (!record->info->size) return;
    if (record->ops.copy_ctor) record->ops.copy_ctor(dst, src, count);
    else memcpy(dst, src, (size_t)record->info->size * count);
}

static inline void ecs_component_value_move(
    const ecs_component_record_t *record,
    void *dst, void *src, uint32_t count
) {
    if (!record->info->size) return;
    if (record->ops.move) record->ops.move(dst, src, count);
    else if (record->ops.copy) {
        record->ops.copy(dst, src, count);
        if (record->ops.dtor) record->ops.dtor(src, count);
    } else memcpy(dst, src, (size_t)record->info->size * count);
}

static inline void ecs_component_value_move_ctor(
    const ecs_component_record_t *record,
    void *dst, void *src, uint32_t count
) {
    if (!record->info->size) return;
    if (record->ops.move_ctor) record->ops.move_ctor(dst, src, count);
    else if (record->ops.copy_ctor) {
        record->ops.copy_ctor(dst, src, count);
        if (record->ops.dtor) record->ops.dtor(src, count);
    } else memcpy(dst, src, (size_t)record->info->size * count);
}

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

extern ecs_entity_index_t entity_index;

#define ecs_entity_index_get_record(entity_id)                                                     \
    sicore_vec_get_mut(&entity_index.entities, entity_id, ecs_entity_record_t)

static inline bool ecs_entity_index_is_alive(
    ecs_entity_t entity
) {
    return ecs_entity_index_get_record(
        ecs_first(entity)
    )->generation == ecs_second(entity);
}

#endif

#ifndef SIECS_MODULE_H
#define SIECS_MODULE_H

typedef struct {
    ecs_module_id_t *id;
    const char *name;
    ecs_observer_id_t observer;
    ecs_system_id_t system;
    bool enabled;
} ecs_module_t;

void ecs_module_record_system(ecs_system_id_t system);
void ecs_module_record_observer(ecs_observer_id_t observer);

#endif

#ifndef SIECS_STORAGE_OBSERVER_INDEX_H
#define SIECS_STORAGE_OBSERVER_INDEX_H
#ifndef SIECS_STORAGE_QUERY_INDEX_H
#define SIECS_STORAGE_QUERY_INDEX_H
#include <stdint.h>

#define ECS_QUERY_RETAIN_TABLE_CAPACITY 8
#define ECS_QUERY_COMPILED_TERM_CAPACITY \
    (ECS_QUERY_TERM_CAPACITY + ECS_QUERY_RELATION_CAPACITY + 2)

typedef enum {
    EcsQueryFilterRequired,
    EcsQueryFilterExcluded,
    EcsQueryFilterExact,
} ecs_query_filter_op_t;

typedef struct {
    uint64_t value;
    uint16_t id;
    uint8_t op;
} ecs_query_type_filter_t;

typedef struct {
    uint64_t bloom;
    ecs_entity_t is_a;
    ecs_query_order_t order_by;
    uint16_t term_count;
    uint16_t field_count;
    uint16_t field_mask;
    uint16_t up_mask;
    uint16_t filter_count;
    uint16_t access_count;
} ecs_query_t;

static inline ecs_term_access_t ecs_query_term_access(ecs_query_term_t term) {
    return (ecs_term_access_t)((uint32_t)term.access & UINT32_C(0xff));
}

static inline ecs_relation_id_t ecs_query_term_source_relation(ecs_query_term_t term) {
    return (ecs_relation_id_t)((uint32_t)term.access >> 8);
}

typedef struct ecs_query_cache_s {
    ecs_query_t query;
    sicore_vec_t table_ids; // uint16_t
    void **fields_ptr;
    uint32_t *field_kind_bits;
    uint16_t field_table_capacity;
    uint32_t active_index;
    ecs_observer_id_t observer;
    uint16_t next_free;
    bool alive;
    ecs_query_term_t terms[ECS_QUERY_COMPILED_TERM_CAPACITY];
    ecs_query_type_filter_t filters[ECS_QUERY_RELATION_CAPACITY];
    uint32_t accesses[ECS_QUERY_TERM_CAPACITY];
} ecs_query_cache_t;

typedef struct {
    sicore_vec_t queries;
    sicore_vec_t active_ids; // ecs_query_id_t
    uint16_t first_free;
} ecs_query_index_t;

extern ecs_query_index_t query_index;

void ecs_query_index_init();
void ecs_query_index_fini();
uint16_t ecs_query_index_create(const ecs_query_desc_t *desc);
void ecs_query_index_add_table(const ecs_table_t *table, uint16_t table_id);
void ecs_query_index_refresh_table_fields(const ecs_table_t *table, uint16_t table_id);

static inline bool ecs_query_term_requires_owned(ecs_query_term_t term) {
    return term.access == EcsOut || term.access == EcsInOut || term.access == EcsInOutOptional;
}

static inline bool ecs_query_match_table(const ecs_query_cache_t *cache, const ecs_table_t *table) {
    const ecs_query_t *query = &cache->query;
    if (ECS_LIKELY((query->bloom & table->bloom) != query->bloom)) {
        return false;
    }
    if (query->is_a && !ecs_table_is_a(table, query->is_a)) {
        return false;
    }
    for (uint16_t i = 0; i < query->term_count; i++) {
        ecs_query_term_t term = cache->terms[i];
        ecs_term_access_t access = ecs_query_term_access(term);
        if (access == EcsInOptional || access == EcsInOutOptional || access == EcsInUp ||
            access == EcsInUpOptional) {
            continue;
        }
        if (access == EcsNot) {
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
    for (uint16_t i = 0; i < query->filter_count; i++) {
        ecs_query_type_filter_t filter = cache->filters[i];
        uint16_t pair_index = ecs_type_pair_index(&table->type, filter.id);
        bool present = pair_index != UINT16_MAX;
        if ((filter.op == EcsQueryFilterRequired && !present) ||
            (filter.op == EcsQueryFilterExcluded && present) ||
            (filter.op == EcsQueryFilterExact &&
             (!present || ecs_type_pairs(&table->type)[pair_index].value != filter.value))) {
            return false;
        }
    }
    return true;
}
bool ecs_query_resolve_up_fields(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_index
);

#endif

#include <stdint.h>

typedef struct {
    ecs_event_t event;
    ecs_query_id_t query;
    ecs_observer_callback_t callback;
    uintptr_t user_data;
    ecs_observer_id_t next_module;
    bool enabled;
} ecs_observer_t;

typedef struct {
    sicore_vec_t observers; // ecs_observer_t
    uint16_t event_count;   // next free event id; starts past the builtin events
} ecs_observer_index_t;

extern ecs_observer_index_t observer_index;

void ecs_observer_index_init();
void ecs_observer_index_fini();

#endif

#ifndef SIECS_STORAGE_SYSTEM_INDEX_H
#define SIECS_STORAGE_SYSTEM_INDEX_H
#include <stdint.h>

typedef struct {
    const char *name;
    ecs_query_id_t qid;
    void (*callback)(ecs_iter_t *);
    uintptr_t user_data;
    void (*user_data_dtor)(uintptr_t user_data);
    ecs_phase_t phase;
    ecs_system_id_t after;
    uint32_t resource_accesses[ECS_SYSTEM_RESOURCE_CAPACITY * 2];
    uint16_t resource_access_count;
    ecs_system_id_t next_module;
    bool enabled;
    bool main_thread_only;
} ecs_system_t;

typedef struct {
    ecs_phase_t id;
    const char *name;
    sicore_vec_t systems;
    uint32_t plan_first;
    uint32_t plan_count;
} ecs_phase_info_t;

typedef struct {
    sicore_vec_t systems;
    sicore_vec_t phases;
    sicore_vec_t phase_order;
    sicore_vec_t execution_order;
    uint32_t start_phase_count;
    bool plan_dirty;
} ecs_system_index_t;

extern ecs_system_index_t system_index;

void ecs_system_index_init(void);
void ecs_system_index_fini(void);

ecs_phase_t ecs_phase_register(const ecs_phase_desc_t *desc);
ecs_phase_info_t *ecs_system_index_get_phase(ecs_phase_t phase);

ecs_system_id_t ecs_system_index_create(const ecs_system_desc_t *desc, ecs_query_id_t qid);
ecs_system_t *ecs_system_index_get(ecs_system_id_t system);
void ecs_system_index_build_plan(void);

#endif

#ifndef SIECS_RELATION_H
#define SIECS_RELATION_H

typedef struct {
    ecs_component_t component;
    ecs_relation_info_t info;
} ecs_relation_record_t;

typedef struct {
    sicore_vec_t records; /* ecs_relation_record_t */
} ecs_relation_index_t;

extern ecs_relation_index_t relation_index;

void ecs_relation_index_init(void);
void ecs_relation_index_fini(void);
void ecs_relation_target_on_remove(ecs_entity_t target, ecs_component_t component, void *ptr);
void ecs_relate_id_now(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target);
void ecs_unrelate_id_now(ecs_entity_t entity, ecs_relation_id_t relation);

ecs_component_t ecs_component_register_relation_internal(
    const char *name,
    ecs_relation_id_t relation,
    bool by_target
);

#define ecs_relation_record(id)                                                                    \
    sicore_vec_get(&relation_index.records, id, ecs_relation_record_t)

ecs_entity_t
ecs_relation_target_at_table(const ecs_table_t *table, ecs_relation_id_t relation, uint32_t row);

#endif

#ifndef SIECS_WORKER_POOL_H
#define SIECS_WORKER_POOL_H

#ifndef SIECS_PLATFORM_H
#define SIECS_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

double ecs_platform_time_now_sec(void);
void ecs_platform_time_sleep_sec(double seconds);

#ifdef _WIN32
#include <windows.h>
typedef struct {
    HANDLE handle;
} ecs_platform_thread_t;
typedef CRITICAL_SECTION ecs_platform_mutex_t;
typedef CONDITION_VARIABLE ecs_platform_condition_t;
#define ECS_PLATFORM_THREAD_CALL WINAPI
typedef DWORD (ECS_PLATFORM_THREAD_CALL *ecs_platform_thread_func_t)(void *);
#define ecs_platform_mutex_init(m) InitializeCriticalSection(m)
#define ecs_platform_mutex_fini(m) DeleteCriticalSection(m)
#define ecs_platform_mutex_lock(m) EnterCriticalSection(m)
#define ecs_platform_mutex_unlock(m) LeaveCriticalSection(m)
#define ecs_platform_condition_init(c) InitializeConditionVariable(c)
#define ecs_platform_condition_fini(c) ((void)(c))
#define ecs_platform_condition_wait(c, m) SleepConditionVariableCS(c, m, INFINITE)
#define ecs_platform_condition_signal(c) WakeConditionVariable(c)
#define ecs_platform_condition_broadcast(c) WakeAllConditionVariable(c)
#else
#include <pthread.h>
typedef struct {
    pthread_t handle;
} ecs_platform_thread_t;
typedef pthread_mutex_t ecs_platform_mutex_t;
typedef pthread_cond_t ecs_platform_condition_t;
typedef void *(*ecs_platform_thread_func_t)(void *);
#define ECS_PLATFORM_THREAD_CALL
#define ecs_platform_thread_create(t, f, a) (pthread_create(&(t)->handle, NULL, f, a) == 0)
#define ecs_platform_thread_join(t) pthread_join((t)->handle, NULL)
#define ecs_platform_mutex_init(m) pthread_mutex_init(m, NULL)
#define ecs_platform_mutex_fini(m) pthread_mutex_destroy(m)
#define ecs_platform_mutex_lock(m) pthread_mutex_lock(m)
#define ecs_platform_mutex_unlock(m) pthread_mutex_unlock(m)
#define ecs_platform_condition_init(c) pthread_cond_init(c, NULL)
#define ecs_platform_condition_fini(c) pthread_cond_destroy(c)
#define ecs_platform_condition_wait(c, m) pthread_cond_wait(c, m)
#define ecs_platform_condition_signal(c) pthread_cond_signal(c)
#define ecs_platform_condition_broadcast(c) pthread_cond_broadcast(c)
#endif

#ifdef _WIN32
bool ecs_platform_thread_create(
    ecs_platform_thread_t *thread,
    ecs_platform_thread_func_t function,
    void *argument
);
void ecs_platform_thread_join(ecs_platform_thread_t *thread);
#endif

uint32_t ecs_platform_hardware_thread_count(void);

#endif

#include <stdatomic.h>
#include <stdint.h>

typedef struct {
    ecs_system_id_t system;
} ecs_worker_job_t;

typedef struct ecs_worker_pool_s ecs_worker_pool_t;

typedef struct {
    ecs_platform_thread_t thread;
    ecs_execution_context_t context;
    ecs_worker_pool_t *pool;
    uint16_t index;
    _Alignas(64) atomic_uint completed;
} ecs_worker_t;

struct ecs_worker_pool_s {
    uint16_t worker_count;
    ecs_worker_t *workers;
    ecs_worker_job_t *jobs;
    uint32_t job_capacity;
    uint32_t job_count;
    _Alignas(64) atomic_uint next_job;
    _Alignas(64) atomic_uint completed_jobs;
    atomic_uint epoch;
    atomic_bool stop;
    ecs_platform_mutex_t mutex;
    ecs_platform_condition_t condition;
};

void ecs_worker_pool_init(ecs_worker_pool_t *pool, uint16_t requested_workers);
void ecs_worker_pool_fini(ecs_worker_pool_t *pool);
bool ecs_worker_pool_enabled(const ecs_worker_pool_t *pool);
void ecs_worker_pool_run_systems(
    ecs_worker_pool_t *pool,
    const ecs_system_id_t *systems,
    uint32_t system_count
);
void ecs_worker_pool_flush(ecs_worker_pool_t *pool);

#endif

typedef struct ecs_world_s ecs_world_t;

struct ecs_world_s {
    ecs_module_id_t active_module;
    ecs_world_feat_desc_t features;
    ecs_execution_context_t main_context;
    ecs_worker_pool_t worker_pool;
    bool did_start;
    bool exit;
    double delta_time;
    double last_time;
};

extern ecs_world_t ecs_world;

void ecs_resource_storage_init(void);
void ecs_resource_storage_fini(void);
void ecs_module_storage_init(void);
void ecs_module_storage_fini(void);

typedef ecs_relation_target_t RelationTarget;

typedef struct {
    sicore_vec_t entities;
} RelationSource;

#define ecs_get_record(entity)                                                                     \
    sicore_vec_get_mut(&entity_index.entities, ecs_first(entity), ecs_entity_record_t)
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
        ecs_observer_t *obs = sicore_vec_get_mut(&observer_index.observers, oid, ecs_observer_t);
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
    ecs_execution_context_t *context = ecs_execution_context_current();
    return context->defer_depth != 0 || context->flushing_commands ||
           context->scheduler_parallel;
}

static inline void ecs_assert_not_scheduler_parallel(const char *operation) {
    ecs_assert(
        !ecs_execution_context_current()->scheduler_parallel,
        "%s is not allowed from a parallel system wave\n",
        operation
    );
}

void ecs_bootstrap(void);

extern sicore_map_t name_map;

#endif

ECS_RELATION_DEFINE(
    ChildOf,
    {
        .storage = EcsRelationByDepth,
        .on_delete_target = EcsDeleteSources,
        .acyclic = true,
    }
);
sicore_map_t name_map;

static char *name_copy_string(const char *value) {
    char *copy = value ? strdup(value) : NULL;
    if (value) ecs_assert_not_null(copy);
    return copy;
}

static void name_ctor(void *ptr, uint32_t count) {
    memset(ptr, 0, (size_t)count * sizeof(Name));
}

static void name_dtor(void *ptr, uint32_t count) {
    Name *names = ptr;
    for (uint32_t i = 0; i < count; i++) {
        free(names[i].value);
        names[i].value = NULL;
    }
}

static void name_copy_ctor(void *dst, const void *src, uint32_t count) {
    Name *out = dst;
    const Name *in = src;
    for (uint32_t i = 0; i < count; i++) {
        out[i].value = name_copy_string(in[i].value);
    }
}

static void name_copy(void *dst, const void *src, uint32_t count) {
    Name *out = dst;
    const Name *in = src;
    for (uint32_t i = 0; i < count; i++) {
        if (out[i].value && in[i].value && strcmp(out[i].value, in[i].value) == 0) {
            continue;
        }
        char *copy = name_copy_string(in[i].value);
        free(out[i].value);
        out[i].value = copy;
    }
}

static void name_move_ctor(void *dst, void *src, uint32_t count) {
    memcpy(dst, src, (size_t)count * sizeof(Name));
    memset(src, 0, (size_t)count * sizeof(Name));
}

static void name_move(void *dst, void *src, uint32_t count) {
    Name *out = dst;
    Name *in = src;
    for (uint32_t i = 0; i < count; i++) {
        if (out == in) {
            continue;
        }
        if (out[i].value && in[i].value && strcmp(out[i].value, in[i].value) == 0) {
            free(in[i].value);
            in[i].value = NULL;
            continue;
        }
        free(out[i].value);
        out[i].value = in[i].value;
        in[i].value = NULL;
    }
}

void name_on_add(ecs_entity_t entity, ecs_component_t component, void *data) {
    Name *name = data;
    if (name->value) {
        sicore_map_set(&name_map, name->value, ecs_first(entity));
    }
}

void name_on_set(
    ecs_entity_t entity,
    ecs_component_t component,
    const void *new_value,
    void *current_value
) {
    Name *name = current_value;
    const Name *new_name = new_value;
    if (name != new_name) {
        char *value = name_copy_string(new_name->value);
        if (name->value) sicore_map_unset(&name_map, name->value);
        free(name->value);
        name->value = value;
    }
    if (name->value) sicore_map_set(&name_map, name->value, ecs_first(entity));
}

void name_on_remove(ecs_entity_t entity, ecs_component_t component, void *data) {
    Name *name = data;
    if (name->value) {
        sicore_map_unset(&name_map, name->value);
    }
}

ECS_COMPONENT_DEFINE(
    Name,
    .ops = {
        .ctor = name_ctor,
        .dtor = name_dtor,
        .copy_ctor = name_copy_ctor,
        .copy = name_copy,
        .move_ctor = name_move_ctor,
        .move = name_move,
    },
    .on_add = name_on_add,
    .on_remove = name_on_remove,
    .on_set = name_on_set
);

ECS_RESOURCE_DEFINE(DeltaTime);

ECS_TAG_DEFINE(Disabled);
ECS_TAG_DEFINE(Abstract);

void ecs_bootstrap() {
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create((ecs_type_t){ 0 });
    sicore_vec_push_u64(&entity_index.entities, 0);
    ecs_component({ .name = "Invalid" });

    // Register the ecs_entity_t struct reflection.
    sireflect_register_struct(
        &(sireflect_struct_desc_t){
            .name = "ecs_entity_t",
            .fields = "{ uint32_t id; uint32_t generation; }",
            .size = sizeof(ecs_entity_t),
            .align = _Alignof(ecs_entity_t),
        }
    );

    ECS_RELATION_REGISTER(ChildOf);
    ECS_COMPONENT_REGISTER(Name);
    ECS_RESOURCE_REGISTER(DeltaTime);
    ecs_set_resource(DeltaTime, { .value = 0.0f });
    sicore_map_init(&name_map);
    ECS_COMPONENT_REGISTER(Disabled);
    ECS_COMPONENT_REGISTER(Abstract);

}

#ifndef SIECS_EVENT_OPS_H
#define SIECS_EVENT_OPS_H

typedef struct {
    ecs_type_t known;
    ecs_type_t candidate;
    uint16_t known_i;
    uint16_t candidate_i;
} ecs_type_diff_t;

static inline bool ecs_type_diff_next(ecs_type_diff_t *diff, uint16_t *index) {
    while (diff->candidate_i < diff->candidate.component_count) {
        ecs_component_t id = diff->candidate.ids[diff->candidate_i];
        while (diff->known_i < diff->known.component_count &&
               diff->known.ids[diff->known_i] < id) diff->known_i++;
        if (diff->known_i < diff->known.component_count &&
            diff->known.ids[diff->known_i] == id) { diff->candidate_i++; continue; }
        *index = diff->candidate_i++;
        return true;
    }
    return false;
}

static inline void ecs_apply_component_default_relations(
    ecs_entity_t entity,
    ecs_component_t component
) {
    const ecs_component_record_t *record = ecs_component_index_get(component);
    for (uint16_t i = 0; i < record->default_relation_count; i++) {
        const ecs_component_required_relation_t *required = &record->default_relations[i];
        if (!ecs_has_relation_id(entity, required->relation)) {
            ecs_relate_id_now(entity, required->relation, required->target);
        }
    }
}

static inline bool ecs_emit_component_event(
    ecs_table_t *table, ecs_entity_t entity, uint32_t row, uint16_t column, bool add
) {
    ecs_component_t id = table->type.ids[column];
    void *data = ecs_table_component_at_column(table, column, row);
    const ecs_component_record_t *record = ecs_component_index_get(id);
    ecs_component_on_add_t hook = add ? record->on_add : record->on_remove;
    bool has_default_relations = record->default_relation_count != 0;
    if (hook) hook(entity, id, data);
    ecs_emit(table, entity, add ? EcsOnAdd : EcsOnRemove, data);
    return has_default_relations;
}

static inline bool ecs_emit_added_components(
    const ecs_table_t *from_table,
    ecs_table_t *to_table,
    ecs_entity_t entity,
    uint32_t row
) {
    bool has_default_relations = false;
    ecs_type_diff_t diff = { .known = from_table->type, .candidate = to_table->type };
    uint16_t column;
    while (ecs_type_diff_next(&diff, &column)) {
        has_default_relations |= ecs_emit_component_event(to_table, entity, row, column, true);
    }
    return has_default_relations;
}

static inline void ecs_apply_added_component_default_relations(
    const ecs_table_t *from_table,
    const ecs_table_t *to_table,
    ecs_entity_t entity
) {
    ecs_type_diff_t diff = { .known = from_table->type, .candidate = to_table->type };
    uint16_t column;
    while (ecs_type_diff_next(&diff, &column))
        ecs_apply_component_default_relations(entity, diff.candidate.ids[column]);
}

static inline void ecs_emit_removed_components(
    ecs_table_t *from_table,
    const ecs_type_t *to_type,
    ecs_entity_t entity,
    uint32_t row
) {
    ecs_type_diff_t diff = { .known = *to_type, .candidate = from_table->type };
    uint16_t column;
    while (ecs_type_diff_next(&diff, &column))
        ecs_emit_component_event(from_table, entity, row, column, false);
}

#endif

#ifndef SIECS_INHERITANCE_H
#define SIECS_INHERITANCE_H

#include <stdint.h>

typedef struct {
    ecs_component_t *ids;
    uint16_t count;
} ecs_inheritance_plan_t;

/* Collect components that must become owned when a type inherits from base. */
void ecs_inheritance_plan_build(
    const ecs_type_t *child_type,
    ecs_entity_t base,
    ecs_inheritance_plan_t *plan
);

void ecs_inheritance_plan_fini(ecs_inheritance_plan_t *plan);

/* Copy the effective values from base into newly materialized child columns. */
void ecs_inheritance_plan_copy(
    const ecs_inheritance_plan_t *plan,
    ecs_entity_t base,
    ecs_table_t *child_table,
    uint32_t child_row
);

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
    if (record->ops.move_ctor) {
        record->ops.move_ctor(dst, src, 1);
        return;
    }

    record->ops.copy_ctor(dst, src, 1);
    if (record->ops.dtor) {
        record->ops.dtor(src, 1);
    }
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
    record->ops.ctor(dst, 1);
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
    record->ops.dtor(ptr, 1);
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

static inline void ecs_migrate_same_layout(
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);
    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    for (uint16_t i = 0; i < from_table->add_edge.aux; i++) {
        uint16_t col = from_table->data_columns[i];
        ecs_table_move_column(from_table, col, old_row, to_table, col, new_row);
    }

    ecs_table_finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
}

void *ecs_migrate(
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id,
    ecs_component_t requested_id
);

#endif

#define ECS_COMMAND_NONE UINT32_MAX

#if defined(_MSC_VER)
#define ECS_THREAD_LOCAL __declspec(thread)
#else
#define ECS_THREAD_LOCAL _Thread_local
#endif
static ECS_THREAD_LOCAL ecs_execution_context_t *ecs_tls_context;

ecs_execution_context_t *ecs_execution_context_current(void) {
    return ecs_tls_context ? ecs_tls_context : &ecs_world.main_context;
}

void ecs_execution_context_set(ecs_execution_context_t *context) {
    ecs_tls_context = context;
}

static inline void deferred_change_fini(ecs_deferred_change_t *change) {
    if (!change->data) {
        return;
    }
    const ecs_component_record_t *record = ecs_component_index_get(change->id);
    if (record->info->size && record->ops.dtor) {
        record->ops.dtor(change->data, 1);
    }
    change->data = NULL;
}

static uint32_t change_lower_bound(const sicore_vec_t *changes, ecs_component_t id) {
    const ecs_deferred_change_t *items = changes->data;
    uint32_t first = 0, count = changes->size;
    while (count) {
        uint32_t step = count / 2, middle = first + step;
        if (items[middle].id < id) { first = middle + 1; count -= step + 1; }
        else count = step;
    }
    return first;
}

static inline ecs_deferred_change_t *change_get(
    ecs_entity_command_t *command,
    ecs_component_t id,
    ecs_deferred_op_t op,
    bool *inserted
) {
    uint32_t at = change_lower_bound(&command->changes, id);
    ecs_deferred_change_t *items = command->changes.data;
    *inserted = at == command->changes.size || items[at].id != id;
    if (!*inserted) return &items[at];
    sicore_vec_push_empty(&command->changes, sizeof(ecs_deferred_change_t));
    items = command->changes.data;
    memmove(items + at + 1, items + at,
            (command->changes.size - at - 1) * sizeof *items);
    items[at] = (ecs_deferred_change_t){ .id = id, .op = op };
    return &items[at];
}

static inline void command_init(ecs_entity_command_t *command, ecs_entity_t entity) {
    *command = (ecs_entity_command_t){ .entity = entity, .relation_head = ECS_COMMAND_NONE };
    sicore_vec_init(&command->changes, sizeof(ecs_deferred_change_t));
}

static inline void command_fini(ecs_entity_command_t *command) {
    ecs_deferred_change_t *changes =
        sicore_vec_data(&command->changes, ecs_deferred_change_t);
    for (uint32_t i = 0; i < command->changes.size; i++) {
        deferred_change_fini(&changes[i]);
    }
    sicore_vec_fini(&command->changes);
}

void ecs_command_buffer_init(ecs_command_buffer_t *buffer, ecs_arena_t *arena) {
    sicore_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));
    sicore_vec_init(&buffer->relations, sizeof(ecs_deferred_relation_t));
    buffer->entity_to_command = NULL;
    buffer->entity_capacity = 0;
    buffer->arena = arena;
}

void ecs_command_buffer_fini(ecs_command_buffer_t *buffer) {
    ecs_entity_command_t *commands = sicore_vec_data(&buffer->commands, ecs_entity_command_t);
    for (uint32_t i = 0; i < buffer->commands.size; i++) {
        command_fini(&commands[i]);
    }
    sicore_vec_fini(&buffer->commands);
    sicore_vec_fini(&buffer->relations);
    free(buffer->entity_to_command);
}

void ecs_execution_context_init(ecs_execution_context_t *context) {
    *context = (ecs_execution_context_t){ 0 };
    ecs_arena_init(&context->arena);
    ecs_command_buffer_init(&context->commands, &context->arena);
}

void ecs_execution_context_fini(ecs_execution_context_t *context) {
    ecs_command_buffer_fini(&context->commands);
    ecs_arena_fini(&context->arena);
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

static ecs_entity_command_t *command_for_entity(
    ecs_command_buffer_t *buffer,
    ecs_entity_t entity
) {
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

static inline void command_buffer_change(
    ecs_entity_t entity,
    ecs_component_t id,
    void *data,
    ecs_deferred_op_t op
) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    bool inserted;
    ecs_deferred_change_t *change = change_get(command, id, op, &inserted);
    if (!inserted && op == EcsDeferredAdd) {
        if (change->op == EcsDeferredRemove) change->op = op;
        return;
    } else if (!inserted) {
        deferred_change_fini(change);
        change->op = op;
    }
    if (op == EcsDeferredRemove || op == EcsDeferredAdd) return;
    const ecs_component_record_t *record = ecs_component_index_get(id);
    change->data = ecs_arena_alloc(buffer->arena, record->info->size ? record->info->size : 1);
    if (op == EcsDeferredMove) {
        ecs_component_value_move_ctor(record, change->data, data, 1);
    } else {
        ecs_component_value_copy_ctor(record, change->data, data, 1);
    }
}

void ecs_command_buffer_add(ecs_entity_t entity, ecs_component_t id) {
    command_buffer_change(entity, id, NULL, EcsDeferredAdd);
}

void ecs_command_buffer_remove(ecs_entity_t entity, ecs_component_t id) {
    command_buffer_change(entity, id, NULL, EcsDeferredRemove);
}

void ecs_command_buffer_set(ecs_entity_t entity, ecs_component_t id, const void *data) {
    command_buffer_change(entity, id, (void *)data, EcsDeferredCopy);
}

void ecs_command_buffer_move(ecs_entity_t entity, ecs_component_t id, void *data) {
    command_buffer_change(entity, id, data, EcsDeferredMove);
}

void ecs_command_buffer_kill(ecs_entity_t entity) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    command->kill = true;
    command->has_base = false;
    ecs_deferred_change_t *changes =
        sicore_vec_data(&command->changes, ecs_deferred_change_t);
    for (uint32_t i = 0; i < command->changes.size; i++) {
        deferred_change_fini(&changes[i]);
    }
    sicore_vec_clear(&command->changes);
    command->relation_head = ECS_COMMAND_NONE;
}

void ecs_command_buffer_set_base(ecs_entity_t entity, ecs_entity_t target) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    command->has_base = true;
    command->base = target;
}

void ecs_command_buffer_relate(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_entity_t target
) {
    ecs_command_buffer_t *buffer = &ecs_execution_context_current()->commands;
    ecs_entity_command_t *command = command_for_entity(buffer, entity);
    uint32_t index = command->relation_head;
    while (index != ECS_COMMAND_NONE) {
        ecs_deferred_relation_t *entry =
            sicore_vec_get_mut(&buffer->relations, index, ecs_deferred_relation_t);
        if (entry->id == relation) {
            entry->target = target;
            return;
        }
        index = entry->next;
    }
    ecs_deferred_relation_t value = {
        .target = target,
        .next = command->relation_head,
        .id = relation,
    };
    command->relation_head = buffer->relations.size;
    sicore_vec_push(&buffer->relations, &value, sizeof value);
}

static ecs_type_t command_build_type(
    ecs_command_buffer_t *buffer,
    const ecs_table_t *table,
    ecs_entity_command_t *command
) {
    for (uint32_t i = 0; i < command->changes.size; i++) {
        ecs_deferred_change_t *change = sicore_vec_get_mut(
            &command->changes, i, ecs_deferred_change_t);
        if (change->op == EcsDeferredRemove) continue;
        const ecs_component_record_t *record = ecs_component_index_get(change->id);
        for (uint32_t r = 0; r < record->required_count; r++) {
            bool inserted;
            ecs_deferred_change_t *required =
                change_get(command, record->required[r], EcsDeferredAdd, &inserted);
            if (!inserted && required->op == EcsDeferredRemove) required->op = EcsDeferredAdd;
        }
    }

    const ecs_deferred_change_t *changes = command->changes.data;
    ecs_component_t *ids = ecs_arena_alloc(
        buffer->arena,
        sizeof *ids * (table->type.component_count + command->changes.size)
    );
    uint16_t ti = 0, ci = 0, count = 0;
    while (ti < table->type.component_count || ci < command->changes.size) {
        ecs_component_t table_id = ti < table->type.component_count
                                       ? table->type.ids[ti] : UINT16_MAX;
        ecs_component_t change_id = ci < command->changes.size
                                        ? changes[ci].id : UINT16_MAX;
        if (table_id < change_id) ids[count++] = table->type.ids[ti++];
        else if (change_id < table_id) {
            if (changes[ci].op != EcsDeferredRemove) ids[count++] = change_id;
            ci++;
        } else {
            if (changes[ci].op != EcsDeferredRemove) ids[count++] = table_id;
            ti++; ci++;
        }
    }
    ecs_type_t type = ecs_type_with_ids(&table->type, ids, count);
    type.base = command->has_base ? command->base : table->type.base;
    return type;
}

static bool command_type_unchanged(const ecs_table_t *table, const ecs_entity_command_t *command) {
    const ecs_deferred_change_t *changes =
        sicore_vec_data(&command->changes, ecs_deferred_change_t);
    if (command->has_base && command->base != table->type.base) {
        return false;
    }

    for (uint32_t i = 0; i < command->changes.size; i++) {
        if (changes[i].op == EcsDeferredRemove ||
            !ecs_table_has_owned(table, changes[i].id)) {
            return false;
        }
    }
    return true;
}

static void command_apply_changes(ecs_entity_command_t *command) {
    ecs_deferred_change_t *changes =
        sicore_vec_data(&command->changes, ecs_deferred_change_t);
    for (uint32_t i = 0; i < command->changes.size && ecs_is_alive(command->entity); i++) {
        if (changes[i].op != EcsDeferredCopy && changes[i].op != EcsDeferredMove) {
            continue;
        }
        ecs_component_t id = changes[i].id;
        const ecs_component_record_t *record = ecs_component_index_get(id);
        ecs_entity_record_t *entity_record = ecs_get_record(command->entity);
        ecs_table_t *table = ecs_get_table(entity_record->table_id);
        uint16_t column = ecs_table_get_column_index(table, id);
        void *dst = ecs_table_component_at_column(table, column, entity_record->table_row);

        if (record->on_set) {
            record->on_set(command->entity, id, changes[i].data, dst);
            if (!ecs_is_alive(command->entity)) {
                return;
            }
            entity_record = ecs_get_record(command->entity);
            table = ecs_get_table(entity_record->table_id);
            column = ecs_table_get_column_index(table, id);
            dst = ecs_table_component_at_column(table, column, entity_record->table_row);
        }
        ecs_emit(table, command->entity, EcsOnSet, changes[i].data);
        ecs_component_value_move(record, dst, changes[i].data, 1);
        changes[i].data = NULL;
    }
}

static void command_apply_relations(
    ecs_entity_command_t *command,
    const sicore_vec_t *relations
) {
    uint32_t index = command->relation_head;
    while (index != ECS_COMMAND_NONE && ecs_is_alive(command->entity)) {
        const ecs_deferred_relation_t *entry =
            sicore_vec_get(relations, index, ecs_deferred_relation_t);
        if (entry->target) {
            ecs_relate_id_now(command->entity, entry->id, entry->target);
        } else {
            ecs_unrelate_id_now(command->entity, entry->id);
        }
        index = entry->next;
    }
}

static void command_apply(
    ecs_command_buffer_t *buffer,
    ecs_entity_command_t *command,
    const sicore_vec_t *relations
) {
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
        command_apply_changes(command);
        command_apply_relations(command, relations);
        return;
    }

    ecs_type_t final_type = command_build_type(buffer, old_table, command);
    ecs_inheritance_plan_t inheritance_plan = { 0 };
    bool base_changed = command->has_base && command->base != old_table->type.base;
    if (base_changed) {
        ecs_inheritance_plan_build(&final_type, command->base, &inheritance_plan);
        if (inheritance_plan.count != 0) {
            ecs_type_t materialized = ecs_type_with_added_ids(
                &final_type,
                inheritance_plan.ids,
                inheritance_plan.count
            );
            materialized.base = final_type.base;
            ecs_type_fini(&final_type);
            final_type = materialized;
        }
    }

    if (!ecs_type_equals(&old_table->type, &final_type)) {
        uint32_t old_row = record->table_row;
        ecs_emit_removed_components(old_table, &final_type, command->entity, old_row);
        if (!ecs_is_alive(command->entity)) {
            ecs_type_fini(&final_type);
            ecs_inheritance_plan_fini(&inheritance_plan);
            return;
        }

        uint16_t new_table_id = ecs_table_index_get_or_create(final_type);
        record = ecs_get_record(command->entity);
        old_table = ecs_get_table(old_table_id);
        ecs_migrate(record, command->entity, old_table, new_table_id, 0);
        record = ecs_get_record(command->entity);
        ecs_table_t *new_table = ecs_get_table(record->table_id);
        if (base_changed) {
            ecs_inheritance_plan_copy(
                &inheritance_plan,
                command->base,
                new_table,
                record->table_row
            );
        }
        if (ecs_emit_added_components(old_table, new_table, command->entity, record->table_row)) {
            ecs_apply_added_component_default_relations(
                old_table,
                new_table,
                command->entity
            );
        }
    } else {
        ecs_type_fini(&final_type);
    }

    ecs_inheritance_plan_fini(&inheritance_plan);

    command_apply_changes(command);
    command_apply_relations(command, relations);
}

void ecs_command_buffer_flush_buffer(ecs_command_buffer_t *buffer) {
    if (buffer->commands.size == 0) {
        ecs_arena_reset(buffer->arena);
        return;
    }

    while (buffer->commands.size != 0) {
        sicore_vec_t commands = buffer->commands;
        sicore_vec_t relations = buffer->relations;
        sicore_vec_init(&buffer->commands, sizeof(ecs_entity_command_t));
        sicore_vec_init(&buffer->relations, sizeof(ecs_deferred_relation_t));

        ecs_entity_command_t *items = sicore_vec_data(&commands, ecs_entity_command_t);
        for (uint32_t i = 0; i < commands.size; i++) {
            uint32_t entity_id = ecs_first(items[i].entity);
            buffer->entity_to_command[entity_id] = ECS_COMMAND_NONE;
        }

        for (uint32_t i = 0; i < commands.size; i++) {
            command_apply(buffer, &items[i], &relations);
            command_fini(&items[i]);
        }
        sicore_vec_fini(&commands);
        sicore_vec_fini(&relations);
    }
    ecs_arena_reset(buffer->arena);
}

void ecs_command_buffer_flush() {
    ecs_execution_context_t *context = ecs_execution_context_current();
    if (context->flushing_commands) {
        ecs_command_buffer_flush_buffer(&context->commands);
        return;
    }
    context->flushing_commands = true;
    ecs_command_buffer_flush_buffer(&context->commands);
    context->flushing_commands = false;
}

void ecs_defer_begin(void) {
    ecs_execution_context_current()->defer_depth++;
}

void ecs_defer_end(void) {
    ecs_execution_context_t *context = ecs_execution_context_current();
    ecs_assert(context->defer_depth > 0, "ecs_defer_end called without ecs_defer_begin\n");
    context->defer_depth--;
    if (context->defer_depth == 0 && !context->scheduler_parallel) {
        ecs_command_buffer_flush();
    }
}

static ecs_component_t ecs_component_alloc_ids(uint16_t count) {
    uint32_t id = component_index.components.size;
    ecs_assert(id + count <= UINT16_MAX, "component id overflow\n");
    return id;
}

static inline void ecs_dense_relation_remove_source(
    ecs_entity_t entity,
    ecs_entity_t target,
    ecs_component_t target_component,
    RelationSource *source,
    uint32_t index
) {
    ecs_assert(
        index < source->entities.size &&
            *sicore_vec_get(&source->entities, index, ecs_entity_t) == entity,
        "relation source index is invalid\n"
    );

    uint32_t last = source->entities.size - 1;

    if (index != last) {
        ecs_entity_t moved =
            *sicore_vec_get(&source->entities, last, ecs_entity_t);

        *sicore_vec_get_mut(
            &source->entities,
            index,
            ecs_entity_t
        ) = moved;

        RelationTarget *moved_data =
            ecs_get_cid(moved, target_component);

        moved_data->source_index = index;
    }

    sicore_vec_remove_last(&source->entities);

    if (source->entities.size == 0) {
        ecs_remove_cid(target, target_component + 1);
    }
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

    ecs_assert_entity_valid(target_data->entity);
    ecs_assert_is_alive(target_data->entity);

    if (old_target_data->entity == target_data->entity) {
        return;
    }

    if (old_target_data->entity) {
        RelationSource *source =
            ecs_get_cid(old_target_data->entity, source_component);

        ecs_dense_relation_remove_source(
            entity,
            old_target_data->entity,
            target_component,
            source,
            old_target_data->source_index
        );
    }

    uint32_t source_index;
    if (ecs_has_cid(target_data->entity, source_component)) {
        RelationSource *source_data = ecs_get_cid(target_data->entity, source_component);
        source_index = source_data->entities.size;
        sicore_vec_push_u64(&source_data->entities, entity);
    } else {
        RelationSource source_data = { 0 };
        sicore_vec_init(&source_data.entities, sizeof(ecs_entity_t));
        sicore_vec_push_u64(&source_data.entities, entity);
        ecs_set_cid(target_data->entity, source_component, &source_data);
        source_index = 0;
    }

    ((RelationTarget *)current_value)->source_index = source_index;
}

void RelationOnRemove(
    ecs_entity_t entity,
    ecs_component_t component,
    void *ptr
) {
    const RelationTarget *target_data = ptr;
    ecs_component_t source_component = component + 1;
    RelationSource *target_source_data =
        ecs_get_cid(target_data->entity, source_component);

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    if (target_source_data->entities.size == UINT32_MAX) {
        return;
    }

    ecs_dense_relation_remove_source(
        entity,
        target_data->entity,
        component,
        target_source_data,
        target_data->source_index
    );
}

static void RelationSourceDtor(void *ptr, uint32_t count) {
    RelationSource *source_data = ptr;
    for (uint32_t i = 0; i < count; i++) {
        sicore_vec_fini(&source_data[i].entities);
    }
}

void RelationSourceOnRemove(ecs_entity_t entity, ecs_component_t component, void *ptr) {
    (void)entity;
    RelationSource *source_data = ptr;

    const ecs_entity_t *entities = source_data->entities.data;
    const uint32_t count = source_data->entities.size;
    ecs_relation_id_t relation =
        ECS_COMPONENT_RELATION_ID(ecs_component_index_get(component)->relation_flags);
    const ecs_relation_record_t *relation_record = ecs_relation_record(relation);

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    source_data->entities.size = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
    if (relation_record->info.desc.on_delete_target == EcsDeleteSources) {
            ecs_kill(entities[i]);
        } else {
            ecs_unrelate_id(entities[i], relation);
        }
    }
}

static ecs_component_t ecs_component_register_type(
    ecs_component_t *id,
    const ecs_component_desc_t *desc,
    sireflect_handle_t type
) {
    ecs_assert_not_null(id);
    ecs_assert_not_null(desc);

    if (*id != 0 && *id < component_index.components.size) {
        const ecs_component_record_t *existing = ecs_component_index_get(*id);
        if (existing->tables.data) {
            return *id;
        }
    }

    if (*id == 0) {
        *id = ecs_component_alloc_ids(1);
    }

    ecs_component_t component = *id;
    ecs_component_index_register(
        component,
        desc->name,

        desc->size,
        desc->ops,
        desc->on_set,
        desc->on_remove,
        desc->on_add,
        desc->inheritance,
        0,
        type,
        desc->struct_desc
    );
    return component;
}

ecs_component_t ecs_component_register_relation_internal(
    const char *name,
    ecs_relation_id_t relation,
    bool by_target
) {
    ecs_component_t component = ecs_component_alloc_ids(by_target ? 1 : 2);
    uint32_t target_flags = ECS_COMPONENT_RELATION_FLAGS(relation, EcsComponentRelationTarget);
    ecs_component_index_register(
        component,
        name,

        by_target ? 0 : sizeof(RelationTarget),
        (ecs_type_ops_t){ 0 },
        by_target ? NULL : RelationOnSet,
        by_target ? ecs_relation_target_on_remove : RelationOnRemove,
        NULL,
        EcsInheritShared,
        target_flags,
        SIREFLECT_INVALID_HANDLE,
        NULL
    );
    if (by_target) {
        return component;
    }
    ecs_component_index_register(
        component + 1,
        NULL,

        sizeof(RelationSource),
        (ecs_type_ops_t){ .dtor = RelationSourceDtor },
        NULL,
        RelationSourceOnRemove,
        NULL,
        EcsInheritShared,
        ECS_COMPONENT_RELATION_FLAGS(relation, EcsComponentRelationSource),
        SIREFLECT_INVALID_HANDLE,
        NULL
    );
    return component;
}

ecs_component_t ecs_component_register(ecs_component_t *id, const ecs_component_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("component registration");
    sireflect_handle_t type = SIREFLECT_INVALID_HANDLE;
    if (ECS_LIKELY(desc && desc->struct_desc)) {
        type = sireflect_try_register_struct(desc->struct_desc);
        if (ECS_UNLIKELY(type == SIREFLECT_INVALID_HANDLE)) {
            puts(sireflect_error());
        }
    }
    return ecs_component_register_type(id, desc, type);
}

ecs_component_t ecs_component_init(const ecs_component_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("component registration");
    ecs_component_t id = 0;
    return ecs_component_register(&id, desc);
}

const ecs_component_info_t *ecs_component_info(ecs_component_t component) {
    if (component == 0 || component >= component_index.components.size) {
        return NULL;
    }
    return ecs_component_index_get(component)->info;
}

uint32_t ecs_component_count(void) { return component_index.components.size; }

ecs_component_t ecs_component_dynamic_init(const ecs_dynamic_component_desc_t *desc) {
    sireflect_handle_t type =
        sireflect_try_register_dynamic_struct(desc->name, desc->fields);
    if (type == SIREFLECT_INVALID_HANDLE) {
        return 0;
    }

    for (uint32_t i = 1; i < component_index.components.size; i++) {
        const ecs_component_info_t *info = ecs_component_index_get((ecs_component_t)i)->info;
        if (info && info->type == type) {
            return (ecs_component_t)i;
        }
    }

    const sireflect_type_info_t *info = sireflect_type_info(type);
    sireflect_struct_desc_t reflection = {
        .name = desc->name,
        .fields = desc->fields,
        .size = info->size,
        .align = info->align,
    };
    ecs_component_desc_t component = {
        .size = info->size,
        .struct_desc = &reflection,
        .inheritance = desc->inheritance,
    };

    component.name = desc->name;

    ecs_component_t id = 0;
    return ecs_component_register_type(&id, &component, type);
}

ecs_component_t ecs_tag_init(const char *name) {
    return ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = name,
        .fields = "{}",
    });
}

const char *ecs_component_name(ecs_component_t component) {
    ecs_assert(
        component != 0 && component < component_index.components.size,
        "invalid component id: %u\n",
        component
    );
    return ecs_component_index_get(component)->info->name;
}

#include <stdarg.h>

#ifdef ecs_with
#undef ecs_with
#endif

#define ecs_assert_can_be_updated(entity) \
    ecs_assert(!ecs_has_cid_owned(entity, ecs_id(Abstract)), \
               "An abstract entity cannot be updated.")

#define entity_edit(entity, table, record)                                                         \
    ecs_entity_record_t *record = ecs_get_record(entity);                                          \
    ecs_table_t *table = ecs_get_table(record->table_id);

void ecs_add_cid_now(ecs_entity_t entity, ecs_component_t cid) {
    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(from_id);
    uint16_t edge = ecs_table_get_add_edge(table, cid);

    if (ECS_UNLIKELY(edge < table->type.component_count && table->type.ids[edge] == cid)) {
        return;
    }

    const ecs_component_record_t *crec = ecs_component_index_get(cid);

    if (crec->required_count == 0) {
        if (edge == UINT16_MAX) {
            ecs_type_t new_type = ecs_type_with(&table->type, cid, (ecs_type_pair_t){ 0 });
            edge = ecs_table_index_get_or_create(new_type);

            table = ecs_get_table(from_id);
            ecs_id_map_set(&table->add_edge, cid, edge);
        }

        ecs_table_t *new_table = ecs_get_table(edge);
        void *component_data = ecs_migrate(record, entity, table, edge, cid);

        if (crec->on_add) {
            crec->on_add(entity, cid, component_data);
        }
        ecs_emit(new_table, entity, EcsOnAdd, component_data);
        if (ECS_UNLIKELY(ecs_component_default_relations(cid))) {
            ecs_apply_component_default_relations(entity, cid);
        }
        return;
    }

    if (edge == UINT16_MAX) {
        ecs_component_t added[ECS_COMPONENT_REQUIRE_CAPACITY];
        uint16_t count = 0, required = 0;
        bool component_pending = true;
        while (required < crec->required_count || component_pending) {
            ecs_component_t next = required < crec->required_count
                                       ? crec->required[required]
                                       : UINT16_MAX;
            if (component_pending && cid < next) { next = cid; component_pending = false; }
            else if (component_pending && cid == next) component_pending = false;
            else required++;
            if (!ecs_table_has_owned(table, next)) added[count++] = next;
        }
        ecs_type_t new_type = ecs_type_with_added_ids(&table->type, added, count);
        edge = ecs_table_index_get_or_create(new_type);

        table = ecs_get_table(from_id);
        ecs_id_map_set(&table->add_edge, cid, edge);
    }

    ecs_table_t *new_table = ecs_get_table(edge);
    void *component_data = ecs_migrate(record, entity, table, edge, cid);

    if (new_table->type.component_count > table->type.component_count + 1) {
        if (ecs_emit_added_components(table, new_table, entity, record->table_row)) {
            ecs_apply_added_component_default_relations(table, new_table, entity);
        }
        return;
    }
    if (crec->on_add) {
        crec->on_add(entity, cid, component_data);
    }
    ecs_emit(new_table, entity, EcsOnAdd, component_data);
    if (ECS_UNLIKELY(ecs_component_default_relations(cid))) {
        ecs_apply_component_default_relations(entity, cid);
    }
}

void ecs_add_cid(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_component_access(entity, cid);
    ecs_assert_can_be_updated(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_add(entity, cid);
        return;
    }

    ecs_add_cid_now(entity, cid);
}

void ecs_remove_cid_now(ecs_entity_t entity, ecs_component_t cid) {
    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(from_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);

    if (ECS_UNLIKELY(col_idx == UINT16_MAX)) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_without(&table->type, col_idx, 0);
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

    ecs_migrate(record, entity, table, new_table_id, 0);
}

void ecs_remove_cid(ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_component_access(entity, cid);

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
    ecs_assert_component_access(entity, cid);

    return ecs_component_get_from_record(ecs_get_record(entity), cid);
}

void *ecs_try_get_cid(ecs_entity_t entity, ecs_component_t cid) {
    return ecs_get_cid(entity, cid);
}

static inline void ecs_store_cid_now(
    ecs_entity_t entity, ecs_component_t cid, void *data, bool move
) {
    bool had_value = move && ecs_has_cid_owned(entity, cid);
    ecs_add_cid_now(entity, cid);
    if (!move) ecs_defer_begin();
    const ecs_component_record_t *crec = ecs_component_index_get(cid);
    entity_edit(entity, table, record);
    void *dst = ecs_table_get_component(table, cid, record->table_row);

    if (crec->on_set) {
        crec->on_set(entity, cid, data, dst);
    }
    ecs_emit(table, entity, EcsOnSet, data);
    if (crec->relation_flags & EcsComponentRelationTarget) {
        ((RelationTarget *)dst)->entity = ((const RelationTarget *)data)->entity;
    } else if (!move) {
        ecs_component_value_copy(crec, dst, data, 1);
    } else if (had_value || crec->ops.ctor) ecs_component_value_move(crec, dst, data, 1);
    else ecs_component_value_move_ctor(crec, dst, data, 1);
    if (!move) ecs_defer_end();
}

void ecs_set_cid_now(ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_store_cid_now(entity, cid, (void *)data, false);
}

static inline void ecs_store_cid(ecs_entity_t entity, ecs_component_t cid, void *data, bool move) {
    ecs_assert_component_access(entity, cid);
    if (ecs_is_deferred()) {
        if (move) ecs_command_buffer_move(entity, cid, data);
        else ecs_command_buffer_set(entity, cid, data);
        return;
    }
    ecs_store_cid_now(entity, cid, data, move);
}

void ecs_set_cid(ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_store_cid(entity, cid, (void *)data, false);
}

void ecs_move_cid_now(ecs_entity_t entity, ecs_component_t cid, void *data) {
    ecs_store_cid_now(entity, cid, data, true);
}

void ecs_move_cid(ecs_entity_t entity, ecs_component_t cid, void *data) {
    ecs_store_cid(entity, cid, data, true);
}

bool ecs_has_cid(const ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_entity_alive(entity);

    uint16_t tid = ecs_get_record(entity)->table_id;
    return ecs_table_has(ecs_get_table(tid), id);
}

bool ecs_has_cid_owned(const ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_entity_alive(entity);

    uint16_t tid = ecs_get_record(entity)->table_id;
    return ecs_table_has_owned(ecs_get_table(tid), id);
}

static uint32_t ecs_required_lower_bound(
    const ecs_component_t *ids, uint32_t count, ecs_component_t id
) {
    uint32_t first = 0;
    while (first < count) {
        uint32_t middle = first + (count - first) / 2;
        if (ids[middle] < id) first = middle + 1;
        else count = middle;
    }
    return first;
}

static void ecs_required_add(ecs_component_record_t *record, ecs_component_t id) {
    uint32_t at = ecs_required_lower_bound(record->required, record->required_count, id);
    if (at < record->required_count && record->required[at] == id) return;
    ecs_assert(record->required_count < ECS_COMPONENT_REQUIRE_CAPACITY - 1,
               "component requirement capacity exceeded\n");
    record->required = realloc(record->required,
                               sizeof *record->required * (record->required_count + 1));
    memmove(record->required + at + 1, record->required + at,
            (record->required_count - at) * sizeof *record->required);
    record->required[at] = id;
    record->required_count++;
}

static inline void ecs_with_impl(ecs_component_t component, ecs_component_t require) {
    ecs_assert_id_valid(component);
    ecs_assert_id_valid(require);
    ecs_assert(component != require, "component cannot require itself: %d\n", component);
    const ecs_component_record_t *required_record = ecs_component_index_get(require);
    uint32_t cycle = ecs_required_lower_bound(
        required_record->required, required_record->required_count, component);
    ecs_assert(
        cycle == required_record->required_count || required_record->required[cycle] != component,
        "cyclic component requirement: %d requires %d\n",
        component,
        require
    ); (void)cycle;

    ecs_component_record_t *records = component_index.components.data;
    for (uint32_t i = 1; i < component_index.components.size; i++) {
        ecs_component_record_t *record = &records[i];
        uint32_t at = ecs_required_lower_bound(record->required, record->required_count, component);
        if (i != component &&
            (at == record->required_count || record->required[at] != component)) continue;
        ecs_assert(record->tables.size == 0, "component already used cannot register requirement");
        ecs_required_add(record, require);
        for (uint32_t r = 0; r < required_record->required_count; r++)
            ecs_required_add(record, required_record->required[r]);
    }
}

void ecs_with_relation_id(ecs_component_t cid, ecs_relation_id_t relation, ecs_entity_t target) {
    ecs_assert_id_valid(cid);
    ecs_assert(
        relation != 0 && relation < ecs_relation_count() && ecs_relation_info(relation),
        "relation must be registered: %u\n",
        relation
    );
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(target);

    ecs_component_record_t *record = ecs_component_index_get(cid);
    ecs_assert(record->tables.size == 0, "component already used cannot register relation default");

    for (uint16_t i = 0; i < record->default_relation_count; i++) {
        const ecs_component_required_relation_t *current = &record->default_relations[i];
        if (current->relation != relation) {
            continue;
        }
        ecs_assert(
            current->target == target,
            "component already has a different default target for relation: %u\n",
            relation
        );
        return;
    }

    record->default_relations = realloc(
        record->default_relations,
        sizeof *record->default_relations * (record->default_relation_count + 1)
    );
    record->default_relations[record->default_relation_count++] =
        (ecs_component_required_relation_t){ .relation = relation, .target = target };
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvarargs"
#endif
void ecs_with_many(ecs_component_t component, ...) {
    va_list args;
    va_start(args, component);

    ecs_component_t require;
    while ((require = (ecs_component_t)va_arg(args, int)) != 0) {
        ecs_with_impl(component, require);
    }

    va_end(args);
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

static inline ecs_entity_t ecs_entity_index_create(uint32_t row, bool reuse) {
    ecs_entity_index_t *index = &entity_index;
    uint32_t entity_id;
    uint32_t generation;
    if (reuse && index->first_available != UINT32_MAX) {
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

    ecs_entity_t entity = ecs_entity_index_create(table->entity_count, true);
    ecs_table_add_entity(table, entity);

    return entity;
}

ecs_entity_t ecs_new_no_reuse(void) {
    ecs_table_t *table = ecs_get_table(0);

    ecs_entity_t entity = ecs_entity_index_create(table->entity_count, false);
    ecs_table_add_entity(table, entity);

    return entity;
}

bool ecs_is_alive(const ecs_entity_t entity) { return ecs_entity_index_is_alive(entity); }

ecs_entity_t ecs_entity_from_index(uint32_t index) {
    if (index == 0 || index >= entity_index.entities.size) {
        return 0;
    }

    const ecs_entity_record_t *record =
        sicore_vec_get(&entity_index.entities, index, ecs_entity_record_t);
    if (record->table_id == UINT16_MAX) {
        return 0;
    }

    return ecs_entity(index, record->generation);
}

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

ecs_entity_t ecs_entity_base(ecs_entity_t entity) {
    ecs_assert_is_alive(entity);
    return ecs_get_table(ecs_get_record(entity)->table_id)->type.base;
}

ecs_entity_t ecs_lookup(const char *key) {
    uint32_t index = sicore_map_get(&name_map, key);
    if (index == UINT32_MAX) {
        return 0;
    }
    return ecs_entity(index, ecs_entity_index_get_record(index)->generation);
}

void ecs_is_a_now(ecs_entity_t entity, ecs_entity_t target) {
    if (target) {
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
    }

    ecs_entity_record_t *record = ecs_get_record(entity);
    uint16_t from_table_id = record->table_id;
    ecs_table_t *from_table = ecs_get_table(from_table_id);
    if (from_table->type.base == target) {
        return;
    }

    ecs_inheritance_plan_t plan;
    ecs_inheritance_plan_build(&from_table->type, target, &plan);
    ecs_type_t new_type = ecs_type_with_added_ids(
        &from_table->type,
        plan.ids,
        plan.count
    );
    new_type.base = target;
    uint16_t to_table_id = ecs_table_index_get_or_create(new_type);
    if (to_table_id == from_table_id) {
        ecs_inheritance_plan_fini(&plan);
        return;
    }

    from_table = ecs_get_table(from_table_id);
    ecs_migrate(record, entity, from_table, to_table_id, 0);
    ecs_table_t *to_table = ecs_get_table(to_table_id);
    ecs_inheritance_plan_copy(&plan, target, to_table, record->table_row);
    ecs_emit_added_components(from_table, to_table, entity, record->table_row);
    ecs_inheritance_plan_fini(&plan);
}

void ecs_is_a(ecs_entity_t entity, ecs_entity_t target) {
    ecs_assert_entity_alive(entity);
    ecs_assert_entity_alive(target);

    if (ecs_is_deferred()) {
        if (!ecs_has_cid_owned(target, ecs_id(Abstract))) {
            ecs_add_cid(target, ecs_id(Abstract));
        }
        ecs_command_buffer_set_base(entity, target);
        return;
    }

    ecs_is_a_now(entity, target);
}

static inline void ecs_entity_index_kill(uint32_t entity_id) {
    ecs_entity_index_t *index = &entity_index;
    ecs_entity_record_t *record = ecs_entity_index_get_record(entity_id);
    record->generation += 1;
    record->table_row = index->first_available;
    record->table_id = UINT16_MAX;
    index->first_available = entity_id;
}

void ecs_kill_now(ecs_entity_t entity) {
    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *initial_table = ecs_get_table(record->table_id);
    const ecs_component_t *components = initial_table->type.ids;
    uint16_t component_count = initial_table->type.component_count;
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
    ecs_assert_entity_alive(entity);

    if (ecs_is_deferred()) {
        ecs_command_buffer_kill(entity);
        return;
    }

    ecs_kill_now(entity);
}

const char *ecs_entity_name(ecs_entity_t entity) {
    static char *buff = NULL;
    if (ecs_has(entity, Name)) {
        return ecs_get(entity, Name)->value;
    }
    if (!buff) {
        buff = calloc(20, sizeof(char));
    }
    sprintf(buff, "(%d, %d)", ecs_first(entity), ecs_second(entity));
    return buff;
}

static uint16_t ecs_inheritance_base_component_capacity(ecs_entity_t base) {
    uint32_t capacity = 0;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *table = ecs_get_table(record->table_id);
        capacity += table->type.component_count;
        base = table->type.base;
    }
    ecs_assert(capacity <= UINT16_MAX, "too many inherited components: %u\n", capacity);
    return (uint16_t)capacity;
}

static bool ecs_inheritance_type_has(
    const ecs_type_t *type,
    ecs_component_t component
) {
    uint16_t first = 0;
    uint16_t last = type->component_count;
    while (first < last) {
        uint16_t middle = (uint16_t)(first + (last - first) / 2);
        ecs_component_t current = type->ids[middle];
        if (current == component) {
            return true;
        }
        if (current < component) {
            first = (uint16_t)(middle + 1);
        } else {
            last = middle;
        }
    }
    return false;
}

static bool ecs_inheritance_component_is_owned(ecs_component_t component) {
    if (component == ecs_id(Abstract)) {
        return false;
    }

    const ecs_component_record_t *record = ecs_component_index_get(component);
    if (record->relation_flags != 0) {
        return false;
    }
    return record->info->inheritance == EcsInheritOwned;
}

static int ecs_inheritance_component_compare(const void *left, const void *right) {
    ecs_component_t a = *(const ecs_component_t *)left;
    ecs_component_t b = *(const ecs_component_t *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

void ecs_inheritance_plan_build(
    const ecs_type_t *child_type,
    ecs_entity_t base,
    ecs_inheritance_plan_t *plan
) {
    plan->ids = NULL;
    plan->count = 0;

    uint16_t capacity = ecs_inheritance_base_component_capacity(base);
    if (capacity == 0) {
        return;
    }

    ecs_component_t *ids = malloc(sizeof(ecs_component_t) * capacity);
    ecs_assert_not_null(ids);

    uint16_t count = 0;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *table = ecs_get_table(record->table_id);
        for (uint16_t i = 0; i < table->type.component_count; i++) {
            ecs_component_t component = table->type.ids[i];
            if (ecs_inheritance_component_is_owned(component) &&
                !ecs_inheritance_type_has(child_type, component)) {
                ids[count++] = component;
            }
        }
        base = table->type.base;
    }

    if (count == 0) {
        free(ids);
        return;
    }

    qsort(ids, count, sizeof(ecs_component_t), ecs_inheritance_component_compare);
    uint16_t unique = 1;
    for (uint16_t i = 1; i < count; i++) {
        if (ids[i] != ids[unique - 1]) {
            ids[unique++] = ids[i];
        }
    }

    plan->ids = ids;
    plan->count = unique;
}

void ecs_inheritance_plan_fini(ecs_inheritance_plan_t *plan) {
    free(plan->ids);
    plan->ids = NULL;
    plan->count = 0;
}

void ecs_inheritance_plan_copy(
    const ecs_inheritance_plan_t *plan,
    ecs_entity_t base,
    ecs_table_t *child_table,
    uint32_t child_row
) {
    for (uint16_t i = 0; i < plan->count; i++) {
        ecs_component_t component = plan->ids[i];
        const ecs_component_record_t *record = ecs_component_index_get(component);
        const void *source = ecs_try_get_cid(base, component);
        void *destination = ecs_table_get_component(child_table, component, child_row);
        if (record->ops.copy) {
            record->ops.copy(destination, source, 1);
        } else if (record->info->size) {
            memcpy(destination, source, record->info->size);
        }
    }
}

static sicore_vec_t ecs_modules;

void ecs_module_storage_init(void) {
    sicore_vec_init(&ecs_modules, sizeof(ecs_module_t));
    sicore_vec_ensure(&ecs_modules, 1, sizeof(ecs_module_t));
}

static inline ecs_module_t *ecs_module_record(ecs_module_id_t id) {
    ecs_assert(id != 0 && id < ecs_modules.size, "invalid module id: %u\n", id);
    return sicore_vec_get_mut(&ecs_modules, id, ecs_module_t);
}

void ecs_module_storage_fini(void) {
    ecs_module_t *modules = ecs_modules.data;
    for (uint32_t i = 1; i < ecs_modules.size; i++) {
        if (modules[i].id) {
            *modules[i].id = 0;
        }
    }
    sicore_vec_fini(&ecs_modules);
}

ecs_module_id_t ecs_module_init(const ecs_module_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("module registration");
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);
    ecs_assert_not_null(desc->import);

    ecs_module_id_t existing =
        desc->id && *desc->id && *desc->id < ecs_modules.size ? *desc->id : 0;
    if (existing) {
        return existing;
    }

    ecs_module_t record = {
        .id = desc->id,
        .name = desc->name,
        .observer = UINT32_MAX,
        .system = UINT16_MAX,
        .enabled = true,
    };
    sicore_vec_push(&ecs_modules, &record, sizeof(record));

    ecs_module_id_t module = (ecs_module_id_t)(ecs_modules.size - 1);
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

static void ecs_module_set_enabled(ecs_module_id_t module, bool enabled) {
    ecs_module_t *record = ecs_module_record(module);
    if (record->enabled == enabled) return;
    for (ecs_system_id_t id = record->system; id != UINT16_MAX;
         id = ecs_system_index_get(id)->next_module) {
        if (enabled) ecs_system_enable(id); else ecs_system_disable(id);
    }
    for (ecs_observer_id_t id = record->observer; id != UINT32_MAX;
         id = sicore_vec_get(&observer_index.observers, id, ecs_observer_t)->next_module) {
        if (enabled) ecs_observer_enable(id); else ecs_observer_disable(id);
    }
    record->enabled = enabled;
}

void ecs_module_enable(ecs_module_id_t module) { ecs_module_set_enabled(module, true); }

ecs_module_id_t ecs_module_find(const ecs_module_id_t *id) {
    if (!id || !*id || *id >= ecs_modules.size) {
        return 0;
    }
    return *id;
}

const char *ecs_module_name(ecs_module_id_t module) {
    return ecs_module_record(module)->name;
}

void ecs_module_disable(ecs_module_id_t module) {
    ecs_module_set_enabled(module, false);
}

bool ecs_module_is_enabled(const ecs_module_id_t module) {
    return ecs_module_record(module)->enabled;
}

void ecs_module_record_system(ecs_system_id_t system) {
    ecs_module_id_t module = ecs_world.active_module;
    if (module) {
        ecs_system_t *value = ecs_system_index_get(system);
        value->next_module = ecs_module_record(module)->system;
        ecs_module_record(module)->system = system;
    }
}

void ecs_module_record_observer(ecs_observer_id_t observer) {
    ecs_module_id_t module = ecs_world.active_module;
    if (module) {
        ecs_observer_t *value =
            sicore_vec_get_mut(&observer_index.observers, observer, ecs_observer_t);
        value->next_module = ecs_module_record(module)->observer;
        ecs_module_record(module)->observer = observer;
    }
}

#define ECS_BUILTIN_EVENT_COUNT 5

ecs_observer_index_t observer_index;

void ecs_observer_index_init(void) {
    sicore_vec_init(&observer_index.observers, sizeof(ecs_observer_t));
    observer_index.event_count = ECS_BUILTIN_EVENT_COUNT;
}

void ecs_observer_index_fini(void) {
    ecs_observer_t *observers = observer_index.observers.data;
    for (uint32_t i = 0; i < observer_index.observers.size; i++)
        ecs_query_fini(observers[i].query);
    sicore_vec_fini(&observer_index.observers);
    observer_index = (ecs_observer_index_t){ 0 };
}

ecs_event_t ecs_event(void) {
    ecs_assert_not_scheduler_parallel("event registration");
    return observer_index.event_count++;
}

ecs_event_t ecs_event_register(ecs_event_t *id) {
    ecs_assert_not_scheduler_parallel("event registration");
    ecs_assert_not_null(id);

    if (*id == UINT16_MAX) {
        *id = ecs_event();
        return *id;
    }

    if (observer_index.event_count <= *id) {
        observer_index.event_count = *id + 1;
    }

    return *id;
}

ecs_observer_id_t ecs_observer_init(const ecs_observer_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("observer registration");
    ecs_assert(desc->callback != NULL, "Observer callback cannot be NULL");
    ecs_observer_t *observer =
        sicore_vec_push_empty(&observer_index.observers, sizeof(ecs_observer_t));
    *observer = (ecs_observer_t){
        .event = desc->on,
        .query = ecs_query_init(&desc->query),
        .callback = desc->callback,
        .user_data = desc->user_data,
        .next_module = UINT32_MAX,
        .enabled = true,
    };
    ecs_observer_id_t oid = observer_index.observers.size - 1;
    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&query_index.queries, observer->query, ecs_query_cache_t);
    cache->observer = oid;
    const uint16_t *table_ids = cache->table_ids.data;
    for (uint32_t i = 0; i < cache->table_ids.size; i++)
        ecs_table_add_observer(&table_index.tables[table_ids[i]], observer->event, oid);
    ecs_module_record_observer(oid);
    return oid;
}

void ecs_observer_enable(ecs_observer_id_t id) {
    sicore_vec_get_mut(&observer_index.observers, id, ecs_observer_t)->enabled = true;
}

void ecs_observer_disable(ecs_observer_id_t id) {
    sicore_vec_get_mut(&observer_index.observers, id, ecs_observer_t)->enabled = false;
}

void ecs_observer_trigger(ecs_entity_t entity, ecs_event_t event, const void *trigger_data) {
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    ecs_emit(table, entity, event, trigger_data);
}

#ifdef _WIN32

static DWORD WINAPI ecs_platform_thread_start(void *argument) {
    struct ecs_platform_thread_start_s {
        ecs_platform_thread_func_t function;
        void *argument;
    } *start = argument;
    ecs_platform_thread_func_t function = start->function;
    void *function_argument = start->argument;
    free(start);
    return function(function_argument);
}

bool ecs_platform_thread_create(
    ecs_platform_thread_t *thread,
    ecs_platform_thread_func_t function,
    void *argument
) {
    struct ecs_platform_thread_start_s {
        ecs_platform_thread_func_t function;
        void *argument;
    } *start = malloc(sizeof(*start));
    if (!start) {
        return false;
    }
    start->function = function;
    start->argument = argument;
    thread->handle = CreateThread(NULL, 0, ecs_platform_thread_start, start, 0, NULL);
    if (!thread->handle) {
        free(start);
        return false;
    }
    return true;
}

void ecs_platform_thread_join(ecs_platform_thread_t *thread) {
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    thread->handle = NULL;
}

uint32_t ecs_platform_hardware_thread_count(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors ? info.dwNumberOfProcessors : 1;
}

#else

#include <stdlib.h>
#include <unistd.h>

uint32_t ecs_platform_hardware_thread_count(void) {
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (uint32_t)count : 1;
}

#endif

#ifdef _WIN32
#include <windows.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <time.h>
#endif

double ecs_platform_time_now_sec(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#elif defined(__EMSCRIPTEN__)
    return emscripten_get_now() / 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

void ecs_platform_time_sleep_sec(double seconds) {
    if (seconds <= 0.0) {
        return;
    }

#ifdef _WIN32
    Sleep((DWORD)(seconds * 1000.0));
#elif defined(__EMSCRIPTEN__)
    /* A web frame must never block the browser's main thread. */
    (void)seconds;
#else
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);
    nanosleep(&ts, NULL);
#endif
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
    ecs_assert_not_scheduler_parallel("query registration");
    return ecs_query_index_create(desc);
}

ecs_iter_t ecs_query_iter(ecs_query_id_t query_id) {
    ecs_assert(query_id < query_index.queries.size, "invalid query id: %u\n", query_id);

    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&query_index.queries, query_id, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", query_id);
    return (ecs_iter_t){
        .cache = cache,
        .table_idx = UINT16_MAX,
        .table_count = cache->table_ids.size,
        .count = 0,
    };
}

uint32_t ecs_query_count(ecs_query_id_t query_id) {
    ecs_assert(query_id < query_index.queries.size, "invalid query id: %u\n", query_id);
    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&query_index.queries, query_id, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", query_id);
    uint16_t *tids = cache->table_ids.data;

    uint32_t count = 0;
    for (uint32_t i = 0; i < cache->table_ids.size; i++) {
        if (ECS_UNLIKELY(cache->query.up_mask) &&
            !ecs_query_resolve_up_fields(cache, ecs_get_table(tids[i]), i)) {
            continue;
        }
        count += table_index.tables[tids[i]].entity_count;
    }
    return count;
}

bool ecs_iter_next(ecs_iter_t *it) {
    uint16_t *tids = it->cache->table_ids.data;
    do {
        if (++it->table_idx >= it->table_count)
            return false;
        it->count = table_index.tables[tids[it->table_idx]].entity_count;
        if (it->count && ECS_UNLIKELY(it->cache->query.up_mask) &&
            !ecs_query_resolve_up_fields(
                it->cache,
                ecs_get_table(tids[it->table_idx]),
                it->table_idx
            )) {
            it->count = 0;
        }
    } while (it->count == 0);
    if (it->cache->query.field_count == 0) {
        it->ptrs = NULL;
        it->field_kind_bits = 0;
    } else {
        it->ptrs = &it->cache->fields_ptr[it->table_idx * it->cache->query.field_count];
        it->field_kind_bits = it->cache->field_kind_bits[it->table_idx];
    }
    it->entities = table_index.tables[tids[it->table_idx]].entities;
    return true;
}

const ecs_relation_target_t *ecs_targets_id(const ecs_iter_t *it, ecs_relation_id_t relation) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_assert(record->info.desc.storage != EcsRelationByTarget, "ecs_targets requires Dense or ByDepth\n");
    const uint16_t *table_ids = it->cache->table_ids.data;
    const ecs_table_t *table = ecs_get_table(table_ids[it->table_idx]);
    uint16_t column = ecs_table_column_or_invalid(table, record->component);
    return column == UINT16_MAX ? NULL : table->cls[column].data;
}

ecs_entity_t ecs_target_shared_id(const ecs_iter_t *it, ecs_relation_id_t relation) {
#ifndef NDEBUG
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_assert(record->info.desc.storage == EcsRelationByTarget, "ecs_target_shared requires ByTarget\n");

#endif
    const uint16_t *table_ids = it->cache->table_ids.data;
    const ecs_table_t *table = ecs_get_table(table_ids[it->table_idx]);
    return ecs_table_target_id(table, relation);
}

void ecs_query_fini(ecs_query_id_t qid) {
    ecs_assert(qid < query_index.queries.size, "invalid query id: %u\n", qid);

    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&query_index.queries, qid, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", qid);

    free(cache->fields_ptr);
    free(cache->field_kind_bits);
    if (cache->table_ids.capacity > ECS_QUERY_RETAIN_TABLE_CAPACITY) {
        sicore_vec_fini(&cache->table_ids);
        sicore_vec_init(&cache->table_ids, sizeof(uint16_t));
    } else {
        cache->table_ids.size = 0;
    }
    cache->fields_ptr = NULL;
    cache->field_kind_bits = NULL;
    cache->field_table_capacity = 0;

    ecs_query_index_remove_active_id(&query_index, qid);
    cache->next_free = query_index.first_free;
    cache->alive = false;
    query_index.first_free = qid;
}

ecs_relation_index_t relation_index;

void ecs_relation_index_init(void) {
    sicore_vec_init_w_size(&relation_index.records, sizeof(ecs_relation_record_t), 1);
    sicore_vec_ensure(&relation_index.records, 1, sizeof(ecs_relation_record_t));
}

void ecs_relation_index_fini(void) {
    ecs_relation_index_t *index = &relation_index;
    ecs_relation_record_t *records = index->records.data;
    for (uint32_t r = 1; r < relation_index.records.size; r++) {
        free((char *)records[r].info.name);
    }
    sicore_vec_fini(&relation_index.records);
    relation_index = (ecs_relation_index_t){ 0 };
}

ecs_relation_id_t
ecs_relation_register(ecs_relation_id_t *id, const char *name, const ecs_relation_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("relation registration");
    ecs_assert_not_null(id);
    ecs_assert_not_null(desc);
    ecs_assert(
        desc->storage == EcsRelationDense || desc->storage == EcsRelationByDepth ||
            desc->storage == EcsRelationByTarget,
        "invalid relation storage\n"
    );
    ecs_assert(
        desc->storage != EcsRelationByDepth || desc->acyclic,
        "ByDepth relation must be acyclic\n"
    );

    if (*id) {
        sicore_vec_ensure(
            &relation_index.records,
            (uint32_t)*id + 1,
            sizeof(ecs_relation_record_t)
        );
        ecs_relation_record_t *existing =
            sicore_vec_get_mut(&relation_index.records, *id, ecs_relation_record_t);
        if (existing->info.name || existing->component) {
            return *id;
        }
    } else {
        *id = (ecs_relation_id_t)relation_index.records.size;
    }

    sicore_vec_ensure(
        &relation_index.records,
        (uint32_t)*id + 1,
        sizeof(ecs_relation_record_t)
    );
    ecs_component_t component =
        ecs_component_register_relation_internal(name, *id, desc->storage == EcsRelationByTarget);
    *sicore_vec_get_mut(&relation_index.records, *id, ecs_relation_record_t) =
        (ecs_relation_record_t){
            .component = component,
            .info = {
                .name = name ? strdup(name) : NULL,
                .desc = {
                    .storage = desc->storage,
                    .on_delete_target = desc->on_delete_target,
                    .acyclic = desc->storage == EcsRelationByDepth || desc->acyclic,
                },
            },
        };
    return *id;
}

ecs_relation_id_t ecs_relation_init(const char *name, const ecs_relation_desc_t *desc) {
    ecs_relation_id_t id = 0;
    return ecs_relation_register(&id, name, desc);
}

uint32_t ecs_relation_count(void) { return relation_index.records.size; }

const ecs_relation_info_t *ecs_relation_info(ecs_relation_id_t relation) {
    if (relation == 0 || relation >= relation_index.records.size) {
        return NULL;
    }
    return &ecs_relation_record(relation)->info;
}

ecs_entity_t
ecs_relation_target_at_table(const ecs_table_t *table, ecs_relation_id_t relation, uint32_t row) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    if (record->info.desc.storage == EcsRelationByTarget) {
        return ecs_type_pair_get(&table->type, relation);
    }
    uint16_t column = ecs_table_column_or_invalid(table, record->component);
    if (column == UINT16_MAX) {
        return 0;
    }
    const RelationTarget *value = ecs_table_component_at_column(table, column, row);
    return value->entity;
}

ecs_entity_t ecs_table_target_id(const ecs_table_t *table, ecs_relation_id_t relation) {
#ifndef NDEBUG
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_assert(record->info.desc.storage == EcsRelationByTarget, "ecs_table_target requires ByTarget\n");
#endif
    return ecs_type_pair_get(&table->type, relation);
}

static void ecs_emit_relation_event(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    ecs_event_t event,
    ecs_entity_t old_target,
    ecs_entity_t new_target
) {
    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    ecs_relation_event_t relation_event = {
        .relation = relation,
        .old_target = old_target,
        .new_target = new_target,
    };
    ecs_emit(table, entity, event, &relation_event);
}

static void ecs_relation_set_dense(
    ecs_entity_t entity,
    ecs_component_t component,
    ecs_table_t *table,
    uint16_t column,
    ecs_entity_t target
) {
    const ecs_component_record_t *crec = ecs_component_index_get(component);
    RelationTarget value = { .entity = target };
    ecs_entity_record_t *record = ecs_get_record(entity);
    RelationTarget *current = ecs_table_component_at_column(table, column, record->table_row);

    ecs_defer_begin();
    if (crec->on_set) {
        crec->on_set(entity, component, &value, current);
    }
    ecs_emit(table, entity, EcsOnSet, &value);
    current->entity = value.entity;
    ecs_defer_end();
}

#ifndef NDEBUG
static bool
ecs_relation_would_cycle(ecs_entity_t source, ecs_relation_id_t relation, ecs_entity_t target) {
    while (target) {
        if (target == source) {
            return true;
        }
        target = ecs_target_id(target, relation);
    }
    return false;
}
#endif

static ecs_table_t *ecs_relation_set_pair(
    ecs_entity_t entity,
    ecs_component_t component,
    uint16_t key,
    uint64_t value
) {
    ecs_entity_record_t *entity_record = ecs_get_record(entity);
    uint16_t from_id = entity_record->table_id;
    ecs_table_t *from = ecs_get_table(from_id);
    ecs_type_t type =
        ecs_type_with(&from->type, component, (ecs_type_pair_t){ .key = key, .value = value });
    uint16_t to_id = ecs_table_index_get_or_create(type);
    if (to_id != from_id) {
        from = ecs_get_table(from_id);
        if (component) {
            ecs_migrate(entity_record, entity, from, to_id, 0);
        } else {
            ecs_migrate_same_layout(entity_record, entity, from, to_id);
        }
    }
    return ecs_get_table(entity_record->table_id);
}

static void ecs_relation_remove_pair(ecs_entity_t entity, uint16_t component_at, uint16_t key) {
    ecs_entity_record_t *entity_record = ecs_get_record(entity);
    uint16_t from_id = entity_record->table_id;
    ecs_table_t *from = ecs_get_table(from_id);
    ecs_type_t type = ecs_type_without(&from->type, component_at, key);
    uint16_t to_id = ecs_table_index_get_or_create(type);
    from = ecs_get_table(from_id);
    if (component_at == UINT16_MAX) {
        ecs_migrate_same_layout(entity_record, entity, from, to_id);
    } else {
        ecs_migrate(entity_record, entity, from, to_id, 0);
    }
}

static void ecs_relation_update_children_depth(
    ecs_entity_t parent,
    ecs_relation_id_t relation,
    uint32_t parent_depth
) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    RelationSource *source = ecs_try_get_cid(parent, record->component + 1);
    uint32_t count = source ? source->entities.size : 0;
    for (uint32_t i = 0; i < count; i++) {
        source = ecs_get_cid(parent, record->component + 1);
        ecs_entity_t child = *sicore_vec_get(&source->entities, i, ecs_entity_t);
        ecs_relation_set_pair(child, 0, relation, parent_depth + 1);
        ecs_relation_update_children_depth(child, relation, parent_depth + 1);
    }
}

static void ecs_relation_set_depth(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    const ecs_relation_record_t *relation_record,
    ecs_entity_t target,
    bool had_relation
) {
    const ecs_entity_record_t *target_record = ecs_get_record(target);
    uint32_t depth =
        (uint32_t)ecs_type_pair_get(&ecs_get_table(target_record->table_id)->type, relation) + 1;
    ecs_table_t *table = ecs_relation_set_pair(
        entity,
        had_relation ? 0 : relation_record->component,
        relation,
        depth
    );
    ecs_entity_record_t *entity_record = ecs_get_record(entity);
    RelationTarget *current =
        ecs_table_get_component(table, relation_record->component, entity_record->table_row);
    const ecs_component_record_t *component = ecs_component_index_get(relation_record->component);
    RelationTarget value = { .entity = target };
    component->on_set(entity, relation_record->component, &value, current);
    current->entity = value.entity;
    ecs_relation_update_children_depth(entity, relation, depth);
}

void ecs_relate_id_now(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_assert(
        !record->info.desc.acyclic || !ecs_relation_would_cycle(entity, relation, target),
        "cyclic relation\n"
    );

    ecs_entity_t old_target;
    ecs_entity_record_t *entity_record = NULL;
    ecs_table_t *entity_table = NULL;
    uint16_t relation_column = UINT16_MAX;
    if (record->info.desc.storage == EcsRelationDense) {
        entity_record = ecs_get_record(entity);
        entity_table = ecs_get_table(entity_record->table_id);
        relation_column = ecs_table_column_or_invalid(entity_table, record->component);
    }

    if (relation_column != UINT16_MAX) {
        const RelationTarget *current =
            ecs_table_component_at_column(entity_table, relation_column, entity_record->table_row);
        old_target = current->entity;
    } else {
        old_target = ecs_target_id(entity, relation);
    }
    if (old_target == target) {
        return;
    }

    if (record->info.desc.storage == EcsRelationDense) {
        if (relation_column != UINT16_MAX) {
            ecs_relation_set_dense(
                entity,
                record->component,
                entity_table,
                relation_column,
                target
            );
        } else {
            RelationTarget value = { .entity = target };
            ecs_set_cid(entity, record->component, &value);
        }
    } else if (record->info.desc.storage == EcsRelationByDepth) {
        ecs_relation_set_depth(entity, relation, record, target, old_target != 0);
    } else {
        if (!ecs_has_cid_owned(target, record->component)) {
            ecs_add_cid_now(target, record->component);
        }
        ecs_relation_set_pair(entity, 0, relation, target);
    }

    ecs_emit_relation_event(entity, relation, EcsOnRelationSet, old_target, target);
}

void ecs_relate_id(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target) {
    ecs_assert_entity_alive(entity);
    ecs_assert_entity_alive(target);
    if (ecs_is_deferred()) {
        ecs_command_buffer_relate(entity, relation, target);
        return;
    }
    ecs_relate_id_now(entity, relation, target);
}

static void ecs_relation_remove_depth(
    ecs_entity_t entity,
    ecs_relation_id_t relation,
    const ecs_relation_record_t *relation_record
) {
    ecs_entity_record_t *entity_record = ecs_get_record(entity);
    uint16_t from_id = entity_record->table_id;
    ecs_table_t *from = ecs_get_table(from_id);
    uint16_t column = ecs_table_column_or_invalid(from, relation_record->component);
    RelationTarget *value = ecs_table_component_at_column(from, column, entity_record->table_row);
    ecs_component_index_get(relation_record->component)
        ->on_remove(entity, relation_record->component, value);

    entity_record = ecs_get_record(entity);
    from_id = entity_record->table_id;
    from = ecs_get_table(from_id);
    column = ecs_table_column_or_invalid(from, relation_record->component);
    ecs_relation_remove_pair(entity, column, relation);
    ecs_relation_update_children_depth(entity, relation, 0);
}

void ecs_unrelate_id_now(ecs_entity_t entity, ecs_relation_id_t relation) {
    ecs_entity_t old_target = ecs_target_id(entity, relation);
    if (!old_target) {
        return;
    }
    ecs_emit_relation_event(entity, relation, EcsOnRelationRemove, old_target, 0);
    if (!ecs_is_alive(entity) || ecs_target_id(entity, relation) != old_target) {
        return;
    }

    const ecs_relation_record_t *record = ecs_relation_record(relation);
    if (record->info.desc.storage == EcsRelationDense) {
        ecs_remove_cid(entity, record->component);
    } else if (record->info.desc.storage == EcsRelationByDepth) {
        ecs_relation_remove_depth(entity, relation, record);
    } else {
        ecs_relation_remove_pair(entity, UINT16_MAX, relation);
    }
}

void ecs_unrelate_id(ecs_entity_t entity, ecs_relation_id_t relation) {
    ecs_assert_entity_alive(entity);
    if (ecs_is_deferred()) {
        ecs_command_buffer_relate(entity, relation, 0);
        return;
    }
    ecs_unrelate_id_now(entity, relation);
}

bool ecs_has_relation_id(ecs_entity_t entity, ecs_relation_id_t relation) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    const ecs_table_t *table = ecs_get_table(ecs_get_record(entity)->table_id);
    if (record->info.desc.storage != EcsRelationByTarget) {
        return ecs_table_column_or_invalid(table, record->component) != UINT16_MAX;
    }
    return ecs_type_pair_index(&table->type, relation) != UINT16_MAX;
}

ecs_entity_t ecs_target_id(ecs_entity_t entity, ecs_relation_id_t relation) {
    const ecs_entity_record_t *entity_record = ecs_get_record(entity);
    const ecs_table_t *table = ecs_get_table(entity_record->table_id);
    return ecs_relation_target_at_table(table, relation, entity_record->table_row);
}

bool ecs_has_relation_to_id(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target) {
    return ecs_target_id(entity, relation) == target;
}

void ecs_relation_target_on_remove(ecs_entity_t target, ecs_component_t component, void *ptr) {
    (void)ptr;
    ecs_relation_id_t relation =
        ECS_COMPONENT_RELATION_ID(ecs_component_index_get(component)->relation_flags);
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_pair_tables_t tables = ecs_table_index_pair_tables(relation, target);
    if (!tables.count) {
        return;
    }

    for (uint16_t i = 0; i < tables.count; i++) {
        ecs_table_t *table = ecs_get_table(tables.ids[i]);
        while (table->entity_count) {
            uint32_t row = table->entity_count - 1;
            ecs_entity_t source = table->entities[row];
            if (source == target) {
                if (row == 0) {
                    break;
                }
                source = table->entities[0];
            }
            if (record->info.desc.on_delete_target == EcsDeleteSources) {
                ecs_kill_now(source);
            } else {
                ecs_unrelate_id_now(source, relation);
            }
        }
    }
}

typedef struct {
    const char *name;
    uint64_t size;
    void *data;
    ecs_type_ops_t ops;
    ecs_resource_hook_t on_set;
    ecs_resource_hook_t on_remove;
    ecs_resource_t previous;
} ecs_resource_record_t;

static sicore_vec_t ecs_resources;
static ecs_resource_t ecs_last_resource;

void ecs_resource_storage_init(void) {
    sicore_vec_init_w_size(&ecs_resources, sizeof(ecs_resource_record_t), 1);
    sicore_vec_ensure(&ecs_resources, 1, sizeof(ecs_resource_record_t));
    ecs_last_resource = 0;
}

static inline ecs_resource_record_t *ecs_resource_record(ecs_resource_t id) {
    return sicore_vec_get_mut(&ecs_resources, id, ecs_resource_record_t);
}

static inline bool ecs_resource_registered(ecs_resource_t id) {
    return id != 0 && id < ecs_resources.size && ecs_resource_record(id)->name != NULL;
}

static inline void ecs_resource_assert_registered(ecs_resource_t id) {
    ecs_assert(ecs_resource_registered(id), "invalid resource id: %u\n", id);
}

static ecs_resource_t ecs_resource_alloc_id(void) {
    ecs_assert(ecs_resources.size < UINT16_MAX, "resource id overflow\n");
    return (ecs_resource_t)ecs_resources.size;
}

ecs_resource_t ecs_resource_init(const ecs_resource_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("resource registration");
    ecs_resource_t id = 0;
    return ecs_resource_register(&id, desc);
}

ecs_resource_t ecs_resource_register(ecs_resource_t *id, const ecs_resource_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("resource registration");
    ecs_assert_not_null(id);
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);

    if (*id && ecs_resource_registered(*id)) {
        return *id;
    }
    if (*id == 0) {
        *id = ecs_resource_alloc_id();
    }

    sicore_vec_ensure(&ecs_resources, (uint32_t)*id + 1, sizeof(ecs_resource_record_t));
    ecs_resource_record_t *record = ecs_resource_record(*id);
    if (record->name) {
        return *id;
    }

    *record = (ecs_resource_record_t){
        .name = desc->name,
        .size = desc->size,
        .data = NULL,
        .ops = desc->ops,
        .on_set = desc->on_set,
        .on_remove = desc->on_remove,
        .previous = ecs_last_resource,
    };
    ecs_last_resource = *id;
    return *id;
}

ecs_resource_t ecs_resource_find(const char *name) {
    ecs_assert_not_null(name);
    ecs_resource_record_t *records = ecs_resources.data;
    for (uint32_t i = 1; i < ecs_resources.size; i++) {
        if (records[i].name && strcmp(records[i].name, name) == 0) {
            return (ecs_resource_t)i;
        }
    }
    return 0;
}

const char *ecs_resource_name(ecs_resource_t resource) {
    ecs_resource_assert_registered(resource);
    return ecs_resource_record(resource)->name;
}

bool ecs_resource_is_registered_rid(ecs_resource_t id) {
    return ecs_resource_registered(id);
}

static inline void ecs_resource_store(ecs_resource_t id, void *data, bool move) {
    ecs_assert_not_null(data);
    ecs_resource_assert_registered(id);
    ecs_resource_record_t *record = ecs_resource_record(id);

    if (record->on_set) {
        record->on_set(data);
    }
    bool construct = !record->data;
    if (construct) {
        record->data = calloc(1, record->size ? record->size : 1);
        ecs_assert_not_null(record->data);
    }
    if (!record->size) return;
    ecs_type_move_t move_op = construct ? record->ops.move_ctor : record->ops.move;
    if (move && move_op) {
        move_op(record->data, data, 1);
    } else {
        ecs_type_copy_t copy_op = construct ? record->ops.copy_ctor : record->ops.copy;
        if (copy_op) {
            copy_op(record->data, data, 1);
            if (move && record->ops.dtor) record->ops.dtor(data, 1);
        }
        else memcpy(record->data, data, record->size);
    }
}

void ecs_set_resource_rid(ecs_resource_t id, const void *data) {
    ecs_resource_store(id, (void *)data, false);
}

void ecs_move_resource_rid(ecs_resource_t id, void *data) {
    ecs_resource_store(id, data, true);
}

void *ecs_resource_rid(ecs_resource_t id) {
    ecs_resource_assert_registered(id);
    void *data = ecs_resource_record(id)->data;
    ecs_assert(data != NULL, "resource does not exist: %u\n", id);
    return data;
}

void *ecs_try_resource_rid(ecs_resource_t id) {
    ecs_resource_assert_registered(id);
    return ecs_resource_record(id)->data;
}

bool ecs_has_resource_rid(const ecs_resource_t id) {
    ecs_resource_assert_registered(id);
    return ecs_resource_record(id)->data != NULL;
}

void ecs_remove_resource_rid(ecs_resource_t id) {
    ecs_resource_assert_registered(id);
    ecs_resource_record_t *record = ecs_resource_record(id);
    void *data = record->data;
    if (!data) {
        return;
    }
    record->data = NULL;
    if (record->on_remove) {
        record->on_remove(data);
    }
    if (record->ops.dtor) {
        record->ops.dtor(data, 1);
    }
    free(data);
}

void ecs_resource_storage_fini(void) {
    for (ecs_resource_t id = ecs_last_resource; id; id = ecs_resource_record(id)->previous) {
        if (ecs_resource_record(id)->data) {
            ecs_remove_resource_rid(id);
        }
    }
    sicore_vec_fini(&ecs_resources);
}

#define ECS_SYSTEM_NO_QUERY UINT16_MAX

ecs_phase_t ecs_phase_init(const ecs_phase_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("phase registration");
    return ecs_phase_register(desc);
}

const char *ecs_phase_name(ecs_phase_t phase) {
    ecs_phase_info_t *pinfo = ecs_system_index_get_phase(phase);
    return pinfo ? pinfo->name : NULL;
}

ecs_system_id_t ecs_system_init(const ecs_system_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("system registration");
    ecs_assert_not_null(desc);
    ecs_assert(desc->callback, "system requires callback function\n");
    ecs_assert(
        ecs_system_index_get_phase(desc->phase) != NULL,
        "invalid system phase: %u\n",
        desc->phase
    );

    const bool has_query = desc->query.terms[0].id || desc->query.relations[0].id ||
                           desc->query.order_by.func || desc->query.is_a;
    ecs_system_id_t system = ecs_system_index_create(
        desc,
        has_query ? ecs_query_init(&desc->query) : ECS_SYSTEM_NO_QUERY
    );
    ecs_module_record_system(system);
    return system;
}

const char *ecs_system_name(ecs_system_id_t system) { return ecs_system_index_get(system)->name; }

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
    ecs_system_index_t *index = &system_index;
    ecs_phase_info_t *pinfo = ecs_system_index_get_phase(phase);
    ecs_assert(pinfo != NULL, "invalid system phase: %u\n", phase);

    if (index->plan_dirty) {
        ecs_system_index_build_plan();
    }

    pinfo = ecs_system_index_get_phase(phase);
    if (!pinfo)
        return;

    const ecs_system_id_t *order = index->execution_order.data;
    uint32_t at = pinfo->plan_first, end = at + pinfo->plan_count;
    while (at < end) {
        uint32_t first = at;
        while (at < end && order[at])
            at++;
        uint32_t count = at - first;
        const ecs_system_id_t *systems = order + first;
        if (!ecs_worker_pool_enabled(&ecs_world.worker_pool) || count == 1) {
            ecs_world.main_context.scheduler_parallel = false;
            ecs_execution_context_set(&ecs_world.main_context);
            for (uint32_t j = 0; j < count; j++) {
                ecs_run_system(systems[j]);
            }
        } else {
            ecs_worker_pool_run_systems(&ecs_world.worker_pool, systems, count);
            ecs_worker_pool_flush(&ecs_world.worker_pool);
        }
        at++;
    }
}

bool ecs_progress(void) {
    double frame_start = ecs_platform_time_now_sec();

    if (ecs_world.last_time == 0.0) {
        ecs_world.delta_time = 0.0;
    } else {
        ecs_world.delta_time = frame_start - ecs_world.last_time;
    }

    ecs_set_resource(DeltaTime, { .value = (float)ecs_world.delta_time });

    ecs_world.last_time = frame_start;

    ecs_system_index_t *index = &system_index;
    if (index->plan_dirty) {
        ecs_system_index_build_plan();
    }

    if (!ecs_world.did_start) {
        for (uint32_t i = 0; i < index->start_phase_count; i++) {
            ecs_phase_t phase = *sicore_vec_get(&index->phase_order, i, ecs_phase_t);
            ecs_run_phase(phase);
        }
        ecs_world.did_start = true;
    }

    for (uint32_t i = index->start_phase_count; i < index->phase_order.size; i++) {
        ecs_phase_t phase = *sicore_vec_get(&index->phase_order, i, ecs_phase_t);
        ecs_run_phase(phase);
    }

    if (ecs_world.features.target_fps) {
        double target_dt = 1.0 / (double)ecs_world.features.target_fps;
        double elapsed = ecs_platform_time_now_sec() - frame_start;
        double remaining = target_dt - elapsed;

        ecs_platform_time_sleep_sec(remaining);
    }

    return !ecs_world.exit;
}

void ecs_run(void) {
    while (ecs_progress()) {
    }
    ecs_fini();
}

static void ecs_system_set_enabled(ecs_system_id_t system, bool enabled) {
    ecs_system_t *sys = ecs_system_index_get(system);
    if (sys->enabled != enabled) {
        sys->enabled = enabled;
        system_index.plan_dirty = true;
    }
}

void ecs_system_enable(ecs_system_id_t system) { ecs_system_set_enabled(system, true); }
void ecs_system_disable(ecs_system_id_t system) { ecs_system_set_enabled(system, false); }

void ecs_table_init(ecs_table_t *table, ecs_type_t type, uint16_t table_id) {
    table->type = type;
    table->entity_capacity = 1;
    table->entity_count = 0;
    table->add_edge.aux = 0;
    table->entities = malloc(sizeof(ecs_entity_t) * table->entity_capacity);
    table->cls =
        type.component_count == 0 ? NULL : malloc(sizeof(ecs_column_t) * type.component_count);
    table->data_columns =
        type.component_count == 0 ? NULL : malloc(sizeof(uint16_t) * type.component_count);
    table->bloom = ecs_type_bloom(&type);

    sicore_vec_init(&table->observers_by_event, sizeof(sicore_vec_t));
    ecs_id_map_init(&table->add_edge);

    for (uint16_t i = 0; i < type.component_count; i++) {
        ecs_component_record_t *rec = ecs_component_index_get(type.ids[i]);
        sicore_vec_push_u16(&rec->tables, table_id);
        table->cls[i].size = rec->info->size;
        table->cls[i].data =
            rec->info->size != 0 ? calloc(table->entity_capacity, rec->info->size) : NULL;
        if (rec->info->size != 0) {
            table->data_columns[table->add_edge.aux++] = i;
        }
        ecs_id_map_set(&table->add_edge, type.ids[i], i);
        table->cls[i].remove_edge = UINT16_MAX;
        table->cls[i].flags = 0;
        if (rec->info->size == 0 || (!rec->ops.move_ctor && !rec->ops.copy_ctor)) {
            table->cls[i].flags |= EcsColumnTrivialMove;
        }
        if (rec->info->size == 0 || !rec->ops.dtor) {
            table->cls[i].flags |= EcsColumnNoDtor;
        }
        if (rec->info->size == 0 || !rec->ops.ctor) {
            table->cls[i].flags |= EcsColumnZeroCtor;
        }
    }

    if (table->add_edge.aux == 0) {
        free(table->data_columns);
        table->data_columns = NULL;
    } else if (table->add_edge.aux < type.component_count) {
        table->data_columns = realloc(table->data_columns, sizeof(uint16_t) * table->add_edge.aux);
    }
}

static inline void ecs_table_grow(ecs_table_t *table) {
    uint64_t new_capacity = table->entity_capacity * (uint64_t)2;
    table->entities = realloc(table->entities, sizeof(ecs_entity_t) * new_capacity);
    for (uint16_t i = 0; i < table->add_edge.aux; i++) {
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
    ecs_query_index_refresh_table_fields(table, (uint16_t)(table - table_index.tables));
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
        for (uint16_t i = 0; i < table->add_edge.aux; i++) {
            uint16_t column_index = table->data_columns[i];
            ecs_table_dtor_column(table, column_index, row);
        }
    }
    if (row != last_row) {
        ecs_entity_t moved_entity = table->entities[last_row];
        table->entities[row] = moved_entity;
        for (uint16_t i = 0; i < table->add_edge.aux; i++) {
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
    for (uint16_t c = 0; c < table->type.component_count; c++) {
        ecs_component_t component = table->type.ids[c];
        const ecs_component_record_t *crec = ecs_component_index_get(component);

        if (crec->relation_flags & EcsComponentRelationSource) {
            for (uint32_t row = 0; row < table->entity_count; row++) {
                void *ptr = ecs_table_component_at_column(table, c, row);
                if (crec->ops.dtor) {
                    crec->ops.dtor(ptr, 1);
                }
            }
            continue;
        }

        if (crec->relation_flags & EcsComponentRelationTarget) {
            continue;
        }

        for (uint32_t row = 0; row < table->entity_count; row++) {
            void *ptr = ecs_table_component_at_column(table, c, row);
            if (crec->on_remove) {
                crec->on_remove(table->entities[row], component, ptr);
            }
            if (crec->ops.dtor) {
                crec->ops.dtor(ptr, 1);
            }
        }
    }
}

void ecs_table_fini(ecs_table_t *table) {
    ecs_table_fini_component_values(table);

    for (uint16_t i = 0; i < table->type.component_count; i++) {
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

bool ecs_table_has_id(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_table_has(table, component_id);
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

void *ecs_migrate(
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_table_id,
    const ecs_component_t requested_id
) {
    ecs_table_t *to_table = ecs_get_table(to_table_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t from_data = 0;
    uint16_t to_data = 0;
    while (from_data < from_table->add_edge.aux && to_data < to_table->add_edge.aux) {
        uint16_t from_col = from_table->data_columns[from_data];
        uint16_t to_col = to_table->data_columns[to_data];
        ecs_component_t from_id = from_table->type.ids[from_col];
        ecs_component_t to_id = to_table->type.ids[to_col];
        if (from_id == to_id) {
            ecs_table_move_column(from_table, from_col, old_row, to_table, to_col, new_row);
            from_data++;
            to_data++;
        } else if (from_id < to_id) {
            ecs_table_dtor_column(from_table, from_col, old_row);
            from_data++;
        } else {
            ecs_table_ctor_column(to_table, to_col, new_row);
            to_data++;
        }
    }
    while (from_data < from_table->add_edge.aux) {
        ecs_table_dtor_column(from_table, from_table->data_columns[from_data], old_row);
        from_data++;
    }
    while (to_data < to_table->add_edge.aux) {
        ecs_table_ctor_column(to_table, to_table->data_columns[to_data], new_row);
        to_data++;
    }

    ecs_table_finish_migration(record, entity, from_table, old_row, to_table_id, new_row);
    if (!requested_id) {
        return NULL;
    }
    return ecs_table_component_at_column(
        to_table,
        ecs_table_get_column_index(to_table, requested_id),
        new_row
    );
}

static size_t ecs_type_pairs_offset(uint16_t count) {
    size_t end = (size_t)count * sizeof(uint16_t);
    return (end + _Alignof(ecs_type_pair_t) - 1) &
           ~(size_t)(_Alignof(ecs_type_pair_t) - 1);
}

static ecs_type_t ecs_type_alloc(const ecs_type_t *type, int components, int pairs) {
    uint16_t component_count = (uint16_t)(type->component_count + components);
    uint16_t pair_count = (uint16_t)(type->pair_count + pairs);
    size_t bytes = ecs_type_pairs_offset(component_count) +
                   (size_t)pair_count * sizeof(ecs_type_pair_t);
    return (ecs_type_t){
        .ids = bytes ? malloc(bytes) : NULL,
        .component_count = component_count,
        .pair_count = pair_count,
        .base = type->base,
    };
}

static void ecs_array_edit(
    void *dst,
    const void *src,
    size_t element_size,
    uint16_t count,
    uint16_t at,
    int delta,
    const void *value
) {
    if (!value && !delta) {
        if (count) memcpy(dst, src, (size_t)count * element_size);
        return;
    }
    uint8_t *out = dst;
    const uint8_t *in = src;
    if (at) memcpy(out, in, (size_t)at * element_size);
    if (delta >= 0) memcpy(out + (size_t)at * element_size, value, element_size);
    uint16_t from = (uint16_t)(at + (delta <= 0));
    uint16_t to = (uint16_t)(at + (delta >= 0));
    if (from < count)
        memcpy(out + (size_t)to * element_size, in + (size_t)from * element_size,
               (size_t)(count - from) * element_size);
}

ecs_type_t ecs_type_with(
    const ecs_type_t *type,
    ecs_component_t component,
    ecs_type_pair_t pair
) {
    uint16_t component_at = 0;
    while (component_at < type->component_count && type->ids[component_at] < component) {
        component_at++;
    }

    uint16_t pair_at = 0;
    int pair_delta = 0;
    if (pair.key) {
        const ecs_type_pair_t *pairs = ecs_type_pairs(type);
        while (pair_at < type->pair_count && pairs[pair_at].key < pair.key) {
            pair_at++;
        }
        pair_delta = pair_at == type->pair_count || pairs[pair_at].key != pair.key;
    }

    ecs_type_t out = ecs_type_alloc(type, component != 0, pair_delta);
    ecs_array_edit(out.ids, type->ids, sizeof *type->ids, type->component_count,
                   component_at, component != 0, component ? &component : NULL);
    ecs_array_edit(ecs_type_pairs(&out), ecs_type_pairs(type), sizeof pair, type->pair_count,
                   pair_at, pair_delta, pair.key ? &pair : NULL);
    return out;
}

ecs_type_t ecs_type_without(
    const ecs_type_t *type,
    uint16_t component_at,
    uint16_t pair_key
) {
    int component_delta = component_at != UINT16_MAX ? -1 : 0;
    int pair_delta = pair_key ? -1 : 0;
    uint16_t pair_at = pair_key ? ecs_type_pair_index(type, pair_key) : 0;
    ecs_type_t out = ecs_type_alloc(type, component_delta, pair_delta);
    ecs_array_edit(out.ids, type->ids, sizeof *type->ids, type->component_count,
                   component_at, component_delta, NULL);
    ecs_array_edit(ecs_type_pairs(&out), ecs_type_pairs(type), sizeof(ecs_type_pair_t),
                   type->pair_count, pair_at, pair_delta, NULL);
    return out;
}

ecs_type_t ecs_type_with_ids(const ecs_type_t *type, const uint16_t *ids, uint16_t count) {
    ecs_type_t out = ecs_type_alloc(type, count - type->component_count, 0);
    if (count) {
        memcpy(out.ids, ids, (size_t)count * sizeof(uint16_t));
    }
    ecs_array_edit(ecs_type_pairs(&out), ecs_type_pairs(type), sizeof(ecs_type_pair_t),
                   type->pair_count, 0, 0, NULL);
    return out;
}

ecs_type_t ecs_type_with_added_ids(
    const ecs_type_t *type,
    const ecs_component_t *ids,
    uint16_t count
) {
    ecs_type_t out = ecs_type_alloc(type, count, 0);
    uint16_t from_i = 0;
    uint16_t added_i = 0;
    uint16_t out_i = 0;

    while (from_i < type->component_count && added_i < count) {
        if (type->ids[from_i] < ids[added_i]) {
            out.ids[out_i++] = type->ids[from_i++];
        } else {
            out.ids[out_i++] = ids[added_i++];
        }
    }
    while (from_i < type->component_count) {
        out.ids[out_i++] = type->ids[from_i++];
    }
    while (added_i < count) {
        out.ids[out_i++] = ids[added_i++];
    }

    ecs_array_edit(ecs_type_pairs(&out), ecs_type_pairs(type), sizeof(ecs_type_pair_t),
                   type->pair_count, 0, 0, NULL);
    return out;
}

ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base) {
    ecs_type_t out = ecs_type_with_ids(type, type->ids, type->component_count);
    out.base = base;
    return out;
}

void ecs_type_fini(ecs_type_t *type) {
    free(type->ids);
    type->ids = NULL;
}

uint64_t ecs_type_bloom(const ecs_type_t *type) {
    uint64_t bloom = 0;
    for (uint16_t i = 0; i < type->component_count; i++) {
        bloom |= UINT64_C(1) << (type->ids[i] % 64);
    }
    return bloom;
}

#ifdef _WIN32
#include <malloc.h>
#endif

#define ECS_WORKER_ALIGNMENT 64

static void *ecs_worker_alloc(size_t size) {
#ifdef _WIN32
    void *memory = _aligned_malloc(size, ECS_WORKER_ALIGNMENT);
    if (memory) {
        memset(memory, 0, size);
    }
    return memory;
#else
    void *memory = NULL;
    if (posix_memalign(&memory, ECS_WORKER_ALIGNMENT, size) != 0) {
        return NULL;
    }
    memset(memory, 0, size);
    return memory;
#endif
}

static void ecs_worker_free(void *memory) {
#ifdef _WIN32
    _aligned_free(memory);
#else
    free(memory);
#endif
}

static void ecs_worker_run_job(ecs_worker_pool_t *pool, uint32_t job_index) {
    ecs_worker_job_t *job = &pool->jobs[job_index];
    ecs_run_system(job->system);
    uint32_t completed = atomic_fetch_add_explicit(
        &pool->completed_jobs,
        1,
        memory_order_release
    ) + 1;
    if (completed == pool->job_count) {
        ecs_platform_mutex_lock(&pool->mutex);
        ecs_platform_condition_signal(&pool->condition);
        ecs_platform_mutex_unlock(&pool->mutex);
    }
}

static inline void ecs_worker_run_jobs(ecs_worker_pool_t *pool) {
    for (;;) {
        uint32_t job = atomic_fetch_add_explicit(&pool->next_job, 1, memory_order_relaxed);
        if (job >= pool->job_count) return;
        ecs_worker_run_job(pool, job);
    }
}

#ifdef _WIN32
static DWORD ECS_PLATFORM_THREAD_CALL ecs_worker_loop(void *argument)
#else
static void *ecs_worker_loop(void *argument)
#endif
{
    ecs_worker_t *worker = argument;
    ecs_worker_pool_t *pool = worker->pool;
    uint32_t seen_epoch = 0;
    ecs_execution_context_set(&worker->context);

    for (;;) {
        ecs_platform_mutex_lock(&pool->mutex);
        while (!atomic_load_explicit(&pool->stop, memory_order_acquire) &&
               atomic_load_explicit(&pool->epoch, memory_order_acquire) == seen_epoch) {
            ecs_platform_condition_wait(&pool->condition, &pool->mutex);
        }
        if (atomic_load_explicit(&pool->stop, memory_order_acquire)) {
            ecs_platform_mutex_unlock(&pool->mutex);
            break;
        }
        seen_epoch = atomic_load_explicit(&pool->epoch, memory_order_relaxed);
        ecs_platform_mutex_unlock(&pool->mutex);

        ecs_worker_run_jobs(pool);
    }

    ecs_execution_context_set(NULL);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

void ecs_worker_pool_init(ecs_worker_pool_t *pool, uint16_t requested_workers) {
    *pool = (ecs_worker_pool_t){ 0 };
    if (requested_workers == ECS_WORKERS_AUTO) {
        uint32_t cpu_count = ecs_platform_hardware_thread_count();
        requested_workers = cpu_count > 1 ? (uint16_t)(cpu_count - 1) : 0;
    }
    if (requested_workers == 0) {
        return;
    }

    pool->worker_count = requested_workers;
    pool->workers = ecs_worker_alloc(requested_workers * sizeof(ecs_worker_t));
    ecs_assert_not_null(pool->workers);
    ecs_platform_mutex_init(&pool->mutex);
    ecs_platform_condition_init(&pool->condition);
    atomic_init(&pool->next_job, 0);
    atomic_init(&pool->completed_jobs, 0);
    atomic_init(&pool->epoch, 0);
    atomic_init(&pool->stop, false);

    for (uint16_t i = 0; i < requested_workers; i++) {
        ecs_worker_t *worker = &pool->workers[i];
        worker->pool = pool;
        worker->index = i;
        atomic_init(&worker->completed, 0);
        ecs_execution_context_init(&worker->context);
        bool created = ecs_platform_thread_create(&worker->thread, ecs_worker_loop, worker);
        ecs_assert(created, "failed to create ECS worker thread\n"); (void)created;
    }
}

void ecs_worker_pool_fini(ecs_worker_pool_t *pool) {
    if (!pool->worker_count) {
        return;
    }

    atomic_store_explicit(&pool->stop, true, memory_order_release);
    ecs_platform_mutex_lock(&pool->mutex);
    ecs_platform_condition_broadcast(&pool->condition);
    ecs_platform_mutex_unlock(&pool->mutex);

    for (uint16_t i = 0; i < pool->worker_count; i++) {
        ecs_platform_thread_join(&pool->workers[i].thread);
        ecs_execution_context_fini(&pool->workers[i].context);
    }
    ecs_platform_condition_fini(&pool->condition);
    ecs_platform_mutex_fini(&pool->mutex);
    free(pool->jobs);
    ecs_worker_free(pool->workers);
    *pool = (ecs_worker_pool_t){ 0 };
}

bool ecs_worker_pool_enabled(const ecs_worker_pool_t *pool) {
    return pool->worker_count != 0;
}

void ecs_worker_pool_run_systems(
    ecs_worker_pool_t *pool,
    const ecs_system_id_t *systems,
    uint32_t system_count
) {
    if (system_count > pool->job_capacity) {
        uint32_t capacity = pool->job_capacity ? pool->job_capacity : 4;
        while (capacity < system_count) {
            capacity *= 2;
        }
        pool->jobs = realloc(pool->jobs, capacity * sizeof(ecs_worker_job_t));
        ecs_assert_not_null(pool->jobs);
        pool->job_capacity = capacity;
    }
    for (uint32_t i = 0; i < system_count; i++) {
        pool->jobs[i].system = systems[i];
    }
    pool->job_count = system_count;
    atomic_store_explicit(&pool->next_job, 0, memory_order_relaxed);
    atomic_store_explicit(&pool->completed_jobs, 0, memory_order_relaxed);
    ecs_world.main_context.scheduler_parallel = true;
    for (uint16_t i = 0; i < pool->worker_count; i++) {
        pool->workers[i].context.scheduler_parallel = true;
    }
    ecs_execution_context_set(&ecs_world.main_context);
    ecs_platform_mutex_lock(&pool->mutex);
    atomic_store_explicit(
        &pool->epoch,
        atomic_load_explicit(&pool->epoch, memory_order_relaxed) + 1,
        memory_order_release
    );
    ecs_platform_condition_broadcast(&pool->condition);
    ecs_platform_mutex_unlock(&pool->mutex);

    ecs_worker_run_jobs(pool);

    while (atomic_load_explicit(&pool->completed_jobs, memory_order_acquire) < system_count) {
        ecs_platform_mutex_lock(&pool->mutex);
        if (atomic_load_explicit(&pool->completed_jobs, memory_order_acquire) < system_count) {
            ecs_platform_condition_wait(&pool->condition, &pool->mutex);
        }
        ecs_platform_mutex_unlock(&pool->mutex);
    }
}

void ecs_worker_pool_flush(ecs_worker_pool_t *pool) {
    ecs_execution_context_t *main_context = &ecs_world.main_context;
    main_context->flushing_commands = true;
    ecs_command_buffer_flush_buffer(&main_context->commands);
    for (uint16_t i = 0; i < pool->worker_count; i++) {
        ecs_command_buffer_flush_buffer(&pool->workers[i].context.commands);
    }
    /* Hooks/observers during worker-buffer application can enqueue main-thread
     * commands. Apply that deterministic tail before releasing the barrier. */
    ecs_command_buffer_flush_buffer(&main_context->commands);
    main_context->flushing_commands = false;
    main_context->scheduler_parallel = false;
    for (uint16_t i = 0; i < pool->worker_count; i++) {
        pool->workers[i].context.scheduler_parallel = false;
    }
}

ecs_world_t ecs_world;
ecs_entity_index_t entity_index;
#ifndef NDEBUG
static bool ecs_world_started;
#endif
static bool ecs_world_finished;

void ecs_init_w_features(const ecs_world_feat_desc_t *features) {
    ecs_assert(!ecs_world_started, "ecs_init called while ECS is already running\n");
    if (ecs_world_finished) {
        memset(&ecs_world, 0, sizeof ecs_world);
        ecs_world_finished = false;
    }
#ifndef NDEBUG
    ecs_world_started = true;
#endif
    sireflect_init();
    sicore_vec_init_w_size(
        &entity_index.entities,
        sizeof(ecs_entity_record_t),
        256
    );
    entity_index.first_available = UINT32_MAX;
    ecs_component_index_init();
    ecs_relation_index_init();
    ecs_table_index_init();
    ecs_query_index_init();
    ecs_observer_index_init();
    ecs_system_index_init();
    ecs_module_storage_init();
    ecs_resource_storage_init();
    ecs_execution_context_init(&ecs_world.main_context);
    ecs_world.active_module = 0;
    ecs_world.features = *features;
    ecs_world.did_start = false;
    ecs_world.exit = false;
    ecs_world.delta_time = 0;
    ecs_world.last_time = 0;
    ecs_bootstrap();
    ecs_worker_pool_init(&ecs_world.worker_pool, ecs_world.features.worker_threads);
}

void ecs_init(void) { ecs_init_w_features(&(ecs_world_feat_desc_t){ 0 }); }

void ecs_fini(void) {
    ecs_assert(ecs_world_started && !ecs_world_finished, "ecs_fini called outside ECS lifetime\n");
    ecs_world_finished = true;

    ecs_worker_pool_fini(&ecs_world.worker_pool);

    /* Live component teardown must finish while world resources are available. */
    ecs_table_index_fini();
    ecs_observer_index_fini();
    ecs_system_index_fini();
    ecs_module_storage_fini();
    ecs_query_index_fini();
    ecs_resource_storage_fini();
    sicore_vec_fini(
        &entity_index.entities
    );
    entity_index =
        (ecs_entity_index_t){ 0 };
    ecs_execution_context_fini(&ecs_world.main_context);
    ecs_component_index_fini();
    ecs_relation_index_fini();
    sireflect_fini();
    sicore_map_fini(&name_map);
#ifndef NDEBUG
    ecs_world_started = false;
#endif
}

void ecs_quit(void) { ecs_world.exit = true; }

#define ECS_ARENA_INITIAL_CAPACITY 4096u

static ecs_arena_block_t *ecs_arena_block_new(uint32_t capacity) {
    ecs_arena_block_t *block = malloc(sizeof(ecs_arena_block_t) + capacity);
    *block = (ecs_arena_block_t){ .capacity = capacity };
    return block;
}

void ecs_arena_init(ecs_arena_t *allocator) {
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

void ecs_arena_fini(ecs_arena_t *allocator) {
    ecs_arena_block_t *block = allocator->first;
    while (block) {
        ecs_arena_block_t *next = block->next;
        free(block);
        block = next;
    }
}

void ecs_id_map_init(ecs_id_map_t *map) {
    map->capacity = 1;
    map->aux = 0;
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

ecs_component_index_t component_index;

static sireflect_struct_desc_t *
ecs_component_reflection_desc_copy(const sireflect_struct_desc_t *desc) {
    if (!desc) {
        return NULL;
    }

    sireflect_struct_desc_t *copy = malloc(sizeof *copy);
    if (!copy) {
        abort();
    }
    *copy = (sireflect_struct_desc_t){
        .name = strdup(desc->name),
        .fields = strdup(desc->fields),
        .size = desc->size,
        .align = desc->align,
    };
    if (!copy->name || !copy->fields) {
        abort();
    }
    return copy;
}

void ecs_component_index_register(
    ecs_component_t id,
    const char *name,
    uint64_t size,
    ecs_type_ops_t ops,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    ecs_component_inheritance_t inheritance,
    uint32_t relation_flags,
    sireflect_handle_t type,
    const sireflect_struct_desc_t *reflection_desc
) {
    sicore_vec_ensure(
        &component_index.components,
        (uint32_t)id + 1,
        sizeof(ecs_component_record_t)
    );

    ecs_component_record_t *existing =
        sicore_vec_get_mut(&component_index.components, id, ecs_component_record_t);
    if (existing->tables.data) {
        return;
    }

    ecs_component_info_t *info = malloc(sizeof *info);
    if (!info) {
        abort();
    }
    sireflect_struct_desc_t *reflection = ecs_component_reflection_desc_copy(reflection_desc);
    *info = (ecs_component_info_t){
        .name = name ? strdup(name) : NULL,
        .size = size,
        .type = type,
        .reflection = reflection,
        .inheritance = inheritance,
    };
    if (name && !info->name) {
        abort();
    }

    ecs_component_record_t record = {
        .info = info,
        .required = NULL,
        .required_count = 0,
        .default_relations = NULL,
        .default_relation_count = 0,
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
    sicore_vec_init_w_size(&component_index.components, sizeof(ecs_component_record_t), 256);
}

void ecs_component_index_fini() {
    ecs_component_record_t *records = component_index.components.data;

    for (uint32_t i = 0; i < component_index.components.size; i++) {
        if (records[i].info) {
            free((char *)records[i].info->name);

            if (records[i].info->reflection) {
                free((char *)records[i].info->reflection->name);
                free((char *)records[i].info->reflection->fields);
                free((void *)records[i].info->reflection);
            }

            free(records[i].info);
        }
        free(records[i].required);
        free(records[i].default_relations);
        sicore_vec_fini(&records[i].tables);
    }
    sicore_vec_fini(&component_index.components);
}

ecs_component_record_t *ecs_component_index_get(ecs_component_t cid) {
    return sicore_vec_get_mut(&component_index.components, cid, ecs_component_record_t);
}

ecs_query_index_t query_index;

static inline int ecs_compare_order_value(uint64_t a, uint64_t b) {
    return a < b ? -1 : a > b ? 1 : 0;
}

static int ecs_query_order_relation(const ecs_table_t *a, const ecs_table_t *b, uint64_t data) {
    const ecs_relation_id_t relation = (ecs_relation_id_t)data;
    return ecs_compare_order_value(
        ecs_type_pair_get(&a->type, relation),
        ecs_type_pair_get(&b->type, relation)
    );
}

ecs_query_order_t ecs_order_by_target_id(ecs_relation_id_t relation) {
    return (ecs_query_order_t){
        .func = ecs_query_order_relation,
        .data = relation,
    };
}

ecs_query_order_t ecs_order_by_depth_id(ecs_relation_id_t relation) {
    return (ecs_query_order_t){
        .func = ecs_query_order_relation,
        .data = relation,
    };
}

void ecs_query_index_init() {
    ecs_query_index_t *index = &query_index;
    sicore_vec_init(&index->queries, sizeof(ecs_query_cache_t));
    sicore_vec_init(&index->active_ids, sizeof(ecs_query_id_t));
    index->first_free = UINT16_MAX;
}

void ecs_query_index_fini() {
    ecs_query_index_t *index = &query_index;
    for (uint32_t i = 0; i < index->queries.size; i++) {
        ecs_query_cache_t *cache = sicore_vec_get_mut(&index->queries, i, ecs_query_cache_t);
        sicore_vec_fini(&cache->table_ids);
        if (cache->alive) {
            free(cache->fields_ptr);
            free(cache->field_kind_bits);
        }
    }
    sicore_vec_fini(&index->active_ids);
    sicore_vec_fini(&index->queries);
    *index = (ecs_query_index_t){ 0 };
}

static void ecs_query_compile_term(
    ecs_query_cache_t *cache,
    ecs_query_term_t term,
    ecs_component_t *rarest,
    uint32_t *rarest_table_count
) {
    ecs_query_t *query = &cache->query;
    uint16_t i = query->term_count++;
    cache->terms[i] = term;
    ecs_term_access_t access = ecs_query_term_access(term);
#ifndef NDEBUG
    ecs_assert_id_valid(term.id);
    ecs_relation_id_t source = ecs_query_term_source_relation(term);
    ecs_assert(access <= EcsInUpOptional, "invalid query term access: %d\n", access);
    ecs_assert((access >= EcsInUp) == (source != 0), "invalid query up relation\n");
    for (uint16_t j = 0; j < i; j++)
        ecs_assert(cache->terms[j].id != term.id, "duplicate query term component: %d\n", term.id);
#endif
    if (access != EcsFilter && access != EcsNot) {
        query->field_mask |= (uint16_t)(1u << i);
        query->field_count++;
        uint32_t access_id = term.id;
        if (access == EcsOut || access == EcsInOut || access == EcsInOutOptional) {
            access_id |= UINT32_C(1) << 16;
        }
        uint16_t at = query->access_count;
        while (at && (cache->accesses[at - 1] & UINT16_MAX) > term.id) {
            cache->accesses[at] = cache->accesses[at - 1];
            at--;
        }
        cache->accesses[at] = access_id;
        query->access_count++;
    }
    if (access <= EcsInOut || access == EcsFilter) {
        query->bloom |= UINT64_C(1) << (term.id % 64);
        uint32_t table_count = ecs_component_index_get(term.id)->tables.size;
        if (table_count < *rarest_table_count) {
            *rarest = term.id;
            *rarest_table_count = table_count;
        }
    }
    if (access == EcsInUp || access == EcsInUpOptional) {
        query->up_mask |= (uint16_t)(1u << i);
#ifndef NDEBUG
        const ecs_relation_record_t *record =
            ecs_relation_record(ecs_query_term_source_relation(term));
        ecs_assert(
            record->info.desc.storage == EcsRelationByTarget && record->info.desc.acyclic,
            "ecs_up requires acyclic ByTarget\n"
        );
#endif
    }
}

static ecs_component_t ecs_query_build(const ecs_query_desc_t *desc, ecs_query_cache_t *cache) {
    cache->query = (ecs_query_t){ .is_a = desc->is_a, .order_by = desc->order_by };
    ecs_component_t rarest = 0;
    uint32_t rarest_table_count = UINT32_MAX;
    const ecs_component_t excludes[] = { ecs_id(Disabled), ecs_id(Abstract) };
    bool has_exclude[2] = { false, false };
    for (uint16_t i = 0; desc->terms[i].id; i++) {
        has_exclude[0] |= desc->terms[i].id == excludes[0];
        has_exclude[1] |= desc->terms[i].id == excludes[1];
        ecs_query_compile_term(cache, desc->terms[i], &rarest, &rarest_table_count);
    }
    for (uint16_t i = 0; i < 2; i++) {
        if (excludes[i] && !has_exclude[i]) {
            ecs_query_compile_term(
                cache, (ecs_query_term_t){ excludes[i], EcsNot }, &rarest, &rarest_table_count);
        }
    }
    for (uint16_t i = 0; desc->relations[i].id; i++) {
        ecs_query_relation_term_t term = desc->relations[i];
        const ecs_relation_record_t *record = ecs_relation_record(term.id);
#ifndef NDEBUG
        ecs_assert(term.kind <= EcsRelationDepth, "invalid relation query kind\n");
        ecs_assert(term.kind != EcsRelationTarget ||
                       record->info.desc.storage == EcsRelationByTarget,
                   "ecs_to requires ByTarget\n");
        ecs_assert(term.kind != EcsRelationDepth ||
                       record->info.desc.storage == EcsRelationByDepth,
                   "ecs_depth requires ByDepth\n");
        for (uint16_t j = 0; j < i; j++)
            ecs_assert(desc->relations[j].id != term.id, "duplicate relation query term\n");
#endif
        if (record->info.desc.storage != EcsRelationByTarget &&
            (term.kind == EcsRelationRequired || term.kind == EcsRelationExcluded)) {
            ecs_query_compile_term(
                cache,
                (ecs_query_term_t){
                    record->component,
                    term.kind == EcsRelationRequired ? EcsFilter : EcsNot,
                },
                &rarest,
                &rarest_table_count
            );
        } else if (term.kind != EcsRelationOptional) {
            cache->filters[cache->query.filter_count++] = (ecs_query_type_filter_t){
                .value = term.target,
                .id = term.id,
                .op = term.kind == EcsRelationRequired   ? EcsQueryFilterRequired
                      : term.kind == EcsRelationExcluded ? EcsQueryFilterExcluded
                                                         : EcsQueryFilterExact,
            };
        }
    }
    return rarest;
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
        const uint16_t term_index = (uint16_t)ECS_CTZ(remaining_fields);
        remaining_fields &= (uint16_t)(remaining_fields - 1);
        const ecs_query_term_t term = cache->terms[term_index];
        const ecs_term_access_t access = ecs_query_term_access(term);
        void *field_ptr = NULL;
        ecs_field_kind_t field_kind = EcsFieldNone;

        if (access == EcsInUp || access == EcsInUpOptional) {
            field_kind = EcsFieldNone;
        } else if (ecs_query_term_requires_owned(term)) {
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
            field_kind != EcsFieldNone || access == EcsInOptional || access == EcsInOutOptional ||
                access == EcsInUp || access == EcsInUpOptional,
            "query cache matched table without field component: %d\n",
            term.id
        );

        cache->fields_ptr[base + field_index] = field_ptr;
        field_kind_bits |= (uint32_t)field_kind << (field_index * 2);
        field_index++;
    }

    cache->field_kind_bits[table_index] = field_kind_bits;
}

bool ecs_query_resolve_up_fields(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_index
) {
    uint16_t remaining_fields = cache->query.field_mask;
    uint16_t field_index = 0;
    uint32_t field_kind_bits = cache->field_kind_bits[table_index];
    uint32_t base = (uint32_t)table_index * cache->query.field_count;
    while (remaining_fields) {
        uint16_t term_index = (uint16_t)ECS_CTZ(remaining_fields);
        remaining_fields &= (uint16_t)(remaining_fields - 1);
        ecs_query_term_t term = cache->terms[term_index];
        ecs_term_access_t access = ecs_query_term_access(term);
        if (access != EcsInUp && access != EcsInUpOptional) {
            field_index++;
            continue;
        }

        ecs_relation_id_t source_relation = ecs_query_term_source_relation(term);
        ecs_entity_t target = ecs_relation_target_at_table(table, source_relation, 0);
        void *ptr = NULL;
        while (target) {
            ptr = ecs_try_get_cid(target, term.id);
            if (ptr) {
                break;
            }
            target = ecs_target_id(target, source_relation);
        }
        cache->fields_ptr[base + field_index] = ptr;
        field_kind_bits &= ~(3u << (field_index * 2));
        if (ptr) {
            field_kind_bits |= (uint32_t)EcsFieldShared << (field_index * 2);
        } else if (access == EcsInUp) {
            cache->field_kind_bits[table_index] = field_kind_bits;
            return false;
        }
        field_index++;
    }
    cache->field_kind_bits[table_index] = field_kind_bits;
    return true;
}

static void ecs_query_cache_reserve_fields(ecs_query_cache_t *cache, uint16_t count) {
    if (!cache->query.field_count || count <= cache->field_table_capacity) {
        return;
    }
    uint16_t capacity = cache->field_table_capacity ? cache->field_table_capacity : 4;
    while (capacity < count) {
        capacity *= 2;
    }
    cache->fields_ptr =
        realloc(cache->fields_ptr, sizeof(void *) * (uint32_t)capacity * cache->query.field_count);
    cache->field_kind_bits = realloc(cache->field_kind_bits, sizeof(uint32_t) * capacity);
    cache->field_table_capacity = capacity;
}

static void ecs_query_cache_add_matched_table(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_id
) {
    sicore_vec_push_u16(&cache->table_ids, table_id);
    const uint16_t table_count = cache->table_ids.size;
    const uint16_t old_count = table_count - 1;
    const uint16_t field_count = cache->query.field_count;
    ecs_query_cache_reserve_fields(cache, table_count);

    uint16_t insert = old_count;
    const ecs_query_order_t order_by = cache->query.order_by;
    uint16_t *ids = cache->table_ids.data;
    while (order_by.func && insert &&
           order_by.func(ecs_get_table(ids[insert - 1]), table, order_by.data) > 0) {
        ids[insert] = ids[insert - 1];
        insert--;
    }
    ids[insert] = table_id;
    if (field_count && insert != old_count) {
        memmove(
            &cache->fields_ptr[(uint32_t)(insert + 1) * field_count],
            &cache->fields_ptr[(uint32_t)insert * field_count],
            (size_t)(old_count - insert) * field_count * sizeof(void *)
        );
        memmove(
            &cache->field_kind_bits[insert + 1],
            &cache->field_kind_bits[insert],
            (size_t)(old_count - insert) * sizeof(uint32_t)
        );
    }

    if (field_count) ecs_query_cache_set_table_fields(cache, table, insert);
}

static void
ecs_query_index_update_matches(ecs_query_cache_t *query_cache, ecs_component_t component);

ecs_query_id_t ecs_query_index_create(const ecs_query_desc_t *desc) {
    ecs_query_index_t *index = &query_index;
    ecs_query_id_t id;
    ecs_query_cache_t *query_cache;
    bool reused;

    if (index->first_free != UINT16_MAX) {
        reused = true;
        id = index->first_free;
        query_cache = sicore_vec_get_mut(&index->queries, id, ecs_query_cache_t);
        index->first_free = query_cache->next_free;
    } else {
        reused = false;
        query_cache = sicore_vec_push_empty(&index->queries, sizeof(ecs_query_cache_t));
        id = index->queries.size - 1;
    }

    const ecs_component_t match_component = ecs_query_build(desc, query_cache);
    if (reused) {
        query_cache->table_ids.size = 0;
    } else {
        sicore_vec_init(&query_cache->table_ids, sizeof(uint16_t));
    }
    query_cache->fields_ptr = NULL;
    query_cache->field_kind_bits = NULL;
    query_cache->field_table_capacity = 0;
    query_cache->active_index = index->active_ids.size;
    query_cache->observer = UINT32_MAX;
    query_cache->next_free = UINT16_MAX;
    query_cache->alive = true;
    sicore_vec_push_u16(&index->active_ids, id);

    ecs_query_index_update_matches(query_cache, match_component);

    return id;
}

static void
ecs_query_index_update_matches(ecs_query_cache_t *query_cache, ecs_component_t component) {
    if (query_cache->query.filter_count) {
        for (uint16_t i = 0; i < query_cache->query.filter_count; i++) {
            ecs_query_type_filter_t filter = query_cache->filters[i];
            if (filter.op == EcsQueryFilterExact) {
                ecs_pair_tables_t tables = ecs_table_index_pair_tables(filter.id, filter.value);
                if (!tables.count) {
                    return;
                }
                for (uint16_t t = 0; t < tables.count; t++) {
                    const ecs_table_t *table = ecs_get_table(tables.ids[t]);
                    if (ecs_query_match_table(query_cache, table)) {
                        ecs_query_cache_add_matched_table(query_cache, table, tables.ids[t]);
                    }
                }
                return;
            }
        }
    }
    if (component) {
        const sicore_vec_t *tables_vec = &ecs_component_index_get(component)->tables;
        sicore_vec_iter(tables_vec, uint16_t, table_id, {
            const ecs_table_t *table = &table_index.tables[*table_id];

            if (ecs_query_match_table(query_cache, table)) {
                    ecs_query_cache_add_matched_table(query_cache, table, *table_id);
            }
        });
    } else {
        const uint16_t table_count = table_index.table_count;
        const ecs_table_t *tables = table_index.tables;
        for (uint16_t i = 0; i < table_count; i++) {
            if (ecs_query_match_table(query_cache, &tables[i])) {
                ecs_query_cache_add_matched_table(query_cache, &tables[i], i);
            }
        }
    }
}

void ecs_query_index_add_table(const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = query_index.active_ids.data;
    for (uint32_t i = 0; i < query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            sicore_vec_get_mut(&query_index.queries, active_ids[i], ecs_query_cache_t);
        if (ecs_query_match_table(cache, table)) {
            ecs_query_cache_add_matched_table(cache, table, table_id);
            if (cache->observer != UINT32_MAX) {
                const ecs_observer_t *observer = sicore_vec_get(
                    &observer_index.observers, cache->observer, ecs_observer_t);
                ecs_table_add_observer((ecs_table_t *)table, observer->event, cache->observer);
            }
        }
    }
}

void ecs_query_index_refresh_table_fields(const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = query_index.active_ids.data;

    for (uint32_t i = 0; i < query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            sicore_vec_get_mut(&query_index.queries, active_ids[i], ecs_query_cache_t);
        if (cache->query.field_count == 0) {
            continue;
        }

        const uint16_t *table_ids = cache->table_ids.data;
        for (uint32_t table_index = 0; table_index < cache->table_ids.size; table_index++) {
            if (table_ids[table_index] == table_id) {
                ecs_query_cache_set_table_fields(cache, table, table_index);
                break;
            }
        }
    }
}

ecs_system_index_t system_index;

static inline bool ecs_system_id_valid(ecs_system_id_t id) {
    return id && id < system_index.systems.size;
}

static inline ecs_system_t *ecs_system_get_unchecked(ecs_system_id_t id) {
    return sicore_vec_get_mut(&system_index.systems, id, ecs_system_t);
}

static uint32_t ecs_phase_order_index(ecs_phase_t phase) {
    const ecs_phase_t *order = system_index.phase_order.data;
    for (uint32_t i = 0; i < system_index.phase_order.size; i++) {
        if (order[i] == phase) return i;
    }
    return UINT32_MAX;
}

ecs_phase_info_t *ecs_system_index_get_phase(ecs_phase_t phase) {
    return phase < system_index.phases.size
               ? sicore_vec_get_mut(&system_index.phases, phase, ecs_phase_info_t)
               : NULL;
}

ecs_phase_t ecs_phase_register(const ecs_phase_desc_t *desc) {
    ecs_system_index_t *index = &system_index;
    ecs_phase_t id = (ecs_phase_t)index->phases.size;
    ecs_phase_t after = desc ? desc->after : ECS_PHASE_NONE;
    ecs_phase_t before = desc ? desc->before : ECS_PHASE_NONE;
    ecs_assert(after == ECS_PHASE_NONE || after < id, "invalid phase dependency\n");
    ecs_assert(before == ECS_PHASE_NONE || before < id, "invalid phase dependency\n");
    bool is_start = (after != ECS_PHASE_NONE &&
                     ecs_phase_order_index(after) < index->start_phase_count) ||
                    (before != ECS_PHASE_NONE &&
                     ecs_phase_order_index(before) < index->start_phase_count);

    if (id >= EcsPhaseCount && after == ECS_PHASE_NONE && before == ECS_PHASE_NONE) {
        after = EcsOnUpdate;
        before = EcsPostUpdate;
    }

    uint32_t end = is_start ? index->start_phase_count : index->phase_order.size;
    uint32_t insert = end;
    if (after != ECS_PHASE_NONE) insert = ecs_phase_order_index(after) + 1;
    if (before != ECS_PHASE_NONE) {
        uint32_t before_index = ecs_phase_order_index(before);
        if (after == ECS_PHASE_NONE || before_index < insert) insert = before_index;
    }
    ecs_assert(insert >= (is_start ? 0 : index->start_phase_count) && insert <= end,
               "phase dependency crosses start boundary\n");

    ecs_phase_info_t info = {
        .id = id,
        .name = desc && desc->name ? desc->name : "unnamed",
    };
    sicore_vec_init(&info.systems, sizeof(ecs_system_id_t));
    sicore_vec_push(&index->phases, &info, sizeof info);
    uint32_t old_count = index->phase_order.size;
    sicore_vec_push(&index->phase_order, &id, sizeof id);
    ecs_phase_t *order = index->phase_order.data;
    memmove(order + insert + 1, order + insert, (old_count - insert) * sizeof *order);
    order[insert] = id;
    if (is_start) index->start_phase_count++;
    index->plan_dirty = true;
    return id;
}

void ecs_system_index_init(void) {
    ecs_system_index_t *index = &system_index;
    sicore_vec_init(&index->systems, sizeof(ecs_system_t));
    sicore_vec_ensure(&index->systems, 1, sizeof(ecs_system_t));
    sicore_vec_init(&index->phases, sizeof(ecs_phase_info_t));
    sicore_vec_init(&index->phase_order, sizeof(ecs_phase_t));
    sicore_vec_init(&index->execution_order, sizeof(ecs_system_id_t));

    static const ecs_phase_desc_t phases[] = {
        { "EcsPreStart", ECS_PHASE_NONE, ECS_PHASE_NONE },
        { "EcsStart", EcsPreStart, ECS_PHASE_NONE },
        { "EcsPostStart", EcsStart, ECS_PHASE_NONE },
        { "EcsOnLoad", ECS_PHASE_NONE, ECS_PHASE_NONE },
        { "EcsPostLoad", EcsOnLoad, ECS_PHASE_NONE },
        { "EcsPreUpdate", EcsPostLoad, ECS_PHASE_NONE },
        { "EcsOnUpdate", EcsPreUpdate, ECS_PHASE_NONE },
        { "EcsPostUpdate", EcsOnUpdate, ECS_PHASE_NONE },
        { "EcsPreRender", EcsPostUpdate, ECS_PHASE_NONE },
        { "EcsOnRender", EcsPreRender, ECS_PHASE_NONE },
        { "EcsPostRender", EcsOnRender, ECS_PHASE_NONE },
    };
    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        ecs_phase_register(&phases[i]);
        if (i == EcsPostStart) index->start_phase_count = 3;
    }
}

static void ecs_access_add(uint32_t *accesses, uint16_t *count, uint16_t id, bool write) {
    uint16_t at = 0;
    while (at < *count && (accesses[at] & UINT16_MAX) < id) at++;
    if (at < *count && (accesses[at] & UINT16_MAX) == id) {
        accesses[at] |= (uint32_t)write << 16;
        return;
    }
    memmove(accesses + at + 1, accesses + at, (*count - at) * sizeof *accesses);
    accesses[at] = id | ((uint32_t)write << 16);
    (*count)++;
}

ecs_system_id_t ecs_system_index_create(const ecs_system_desc_t *desc, ecs_query_id_t qid) {
    ecs_system_index_t *index = &system_index;
    ecs_system_t system = {
        .name = desc->name,
        .qid = qid,
        .callback = desc->callback,
        .user_data = desc->user_data,
        .user_data_dtor = desc->user_data_dtor,
        .phase = desc->phase,
        .next_module = UINT16_MAX,
        .enabled = !desc->disabled,
        .main_thread_only = desc->main_thread_only,
    };
    for (uint16_t i = 0; i < ECS_SYSTEM_AFTER_CAPACITY && desc->after[i]; i++) {
#ifndef NDEBUG
        ecs_assert(ecs_system_id_valid(desc->after[i]), "invalid system dependency: %u\n",
                   desc->after[i]);
        ecs_assert(ecs_system_get_unchecked(desc->after[i])->phase == system.phase,
                   "system dependency must be in the same phase\n");
#endif
        if (desc->after[i] > system.after) system.after = desc->after[i];
    }
    for (uint16_t i = 0; i < ECS_SYSTEM_RESOURCE_CAPACITY && desc->read_resources[i]; i++)
        ecs_access_add(system.resource_accesses, &system.resource_access_count,
                       desc->read_resources[i], false);
    for (uint16_t i = 0; i < ECS_SYSTEM_RESOURCE_CAPACITY && desc->write_resources[i]; i++)
        ecs_access_add(system.resource_accesses, &system.resource_access_count,
                       desc->write_resources[i], true);

    sicore_vec_push(&index->systems, &system, sizeof system);
    ecs_system_id_t id = index->systems.size - 1;
    sicore_vec_push_u16(&ecs_system_index_get_phase(system.phase)->systems, id);
    index->plan_dirty = true;
    return id;
}

ecs_system_t *ecs_system_index_get(ecs_system_id_t system) {
    ecs_assert(ecs_system_id_valid(system), "invalid system id: %u\n", system);
    return ecs_system_get_unchecked(system);
}

static bool ecs_access_conflict(
    const uint32_t *a, uint16_t a_count, const uint32_t *b, uint16_t b_count
) {
    uint16_t ai = 0, bi = 0;
    while (ai < a_count && bi < b_count) {
        uint16_t aid = a[ai], bid = b[bi];
        if (aid < bid) ai++;
        else if (bid < aid) bi++;
        else if ((a[ai] | b[bi]) >> 16) return true;
        else { ai++; bi++; }
    }
    return false;
}

static bool ecs_query_tables_overlap(const ecs_query_cache_t *a, const ecs_query_cache_t *b) {
    const uint16_t *a_ids = a->table_ids.data, *b_ids = b->table_ids.data;
    for (uint32_t ai = 0; ai < a->table_ids.size; ai++)
        for (uint32_t bi = 0; bi < b->table_ids.size; bi++)
            if (a_ids[ai] == b_ids[bi]) return true;
    return false;
}

static bool ecs_system_conflict(const ecs_system_t *a, const ecs_system_t *b) {
    if (a->main_thread_only || b->main_thread_only ||
        ecs_access_conflict(a->resource_accesses, a->resource_access_count,
                            b->resource_accesses, b->resource_access_count)) return true;
    if (a->qid == UINT16_MAX || b->qid == UINT16_MAX) return false;
    const ecs_query_cache_t *aq = sicore_vec_get(&query_index.queries, a->qid, ecs_query_cache_t);
    const ecs_query_cache_t *bq = sicore_vec_get(&query_index.queries, b->qid, ecs_query_cache_t);
    return ecs_access_conflict(aq->accesses, aq->query.access_count,
                               bq->accesses, bq->query.access_count) &&
           ecs_query_tables_overlap(aq, bq);
}

void ecs_system_index_build_plan(void) {
    ecs_system_index_t *index = &system_index;
    sicore_vec_clear(&index->execution_order);
    for (uint32_t p = 0; p < index->phase_order.size; p++) {
        ecs_phase_info_t *phase = ecs_system_index_get_phase(
            *sicore_vec_get(&index->phase_order, p, ecs_phase_t));
        phase->plan_first = index->execution_order.size;
        uint32_t batch_first = phase->plan_first;
        const ecs_system_id_t *systems = phase->systems.data;
        for (uint32_t i = 0; i < phase->systems.size; i++) {
            ecs_system_id_t id = systems[i];
            ecs_system_t *current = ecs_system_index_get(id);
            if (!current->enabled) continue;
            bool blocked = false;
            const ecs_system_id_t *order = index->execution_order.data;
            for (uint32_t j = batch_first; j < index->execution_order.size && !blocked; j++) {
                ecs_system_id_t previous_id = order[j];
                blocked |= current->after == previous_id;
                blocked |= ecs_system_conflict(current, ecs_system_index_get(previous_id));
            }
            if (blocked) {
                sicore_vec_push_u16(&index->execution_order, 0);
                batch_first = index->execution_order.size;
            }
            sicore_vec_push_u16(&index->execution_order, id);
        }
        phase->plan_count = index->execution_order.size - phase->plan_first;
    }
    index->plan_dirty = false;
}

void ecs_system_index_fini(void) {
    ecs_system_index_t *index = &system_index;
    ecs_system_t *systems = index->systems.data;
    for (uint32_t i = 1; i < index->systems.size; i++)
        if (systems[i].user_data_dtor) systems[i].user_data_dtor(systems[i].user_data);
    for (uint32_t i = 0; i < index->phases.size; i++) {
        ecs_phase_info_t *phase = sicore_vec_get_mut(&index->phases, i, ecs_phase_info_t);
        sicore_vec_fini(&phase->systems);
    }
    sicore_vec_fini(&index->execution_order);
    sicore_vec_fini(&index->phase_order);
    sicore_vec_fini(&index->phases);
    sicore_vec_fini(&index->systems);
    *index = (ecs_system_index_t){ 0 };
}

#define INITIAL_SLOT_SHIFT 12
#define INITIAL_PAIR_SLOT_SHIFT 3
#define LOAD_FACTOR 0.75
#define ECS_TABLE_SLOT_EMPTY UINT16_MAX

ecs_table_index_t table_index;

static inline uint32_t ecs_type_hash(ecs_type_t type) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < type.component_count; ++i) {
        h ^= (uint32_t)type.ids[i];
        h *= 16777619u;
    }
    const ecs_type_pair_t *pairs = ecs_type_pairs(&type);
    for (uint16_t i = 0; i < type.pair_count; i++) {
        h ^= pairs[i].key;
        h *= 16777619u;
        h ^= (uint32_t)pairs[i].value;
        h *= 16777619u;
        h ^= (uint32_t)(pairs[i].value >> 32);
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

static uint32_t ecs_pair_hash(uint16_t key, uint64_t value) {
    value ^= (uint64_t)key * UINT64_C(0x9e3779b97f4a7c15);
    value ^= value >> 33;
    value *= UINT64_C(0xff51afd7ed558ccd);
    return (uint32_t)(value ^ (value >> 33));
}

static uint32_t ecs_pair_slot_capacity(const ecs_table_index_t *index) {
    return index->pair_slot_shift ? 1u << index->pair_slot_shift : 0;
}

static void
ecs_pair_slot_insert(ecs_pair_table_slot_t *slots, uint32_t mask, ecs_pair_table_slot_t slot) {
    uint32_t i = ecs_pair_hash(slot.key, slot.value) & mask;
    while (slots[i].key) {
        i = (i + 1) & mask;
    }
    slots[i] = slot;
}

static void ecs_pair_slots_grow(ecs_table_index_t *index) {
    uint32_t old_capacity = ecs_pair_slot_capacity(index);
    ecs_pair_table_slot_t *old = index->pair_slots;
    index->pair_slot_shift =
        index->pair_slot_shift ? index->pair_slot_shift + 1 : INITIAL_PAIR_SLOT_SHIFT;
    uint32_t capacity = ecs_pair_slot_capacity(index);
    index->pair_slots = calloc(capacity, sizeof(ecs_pair_table_slot_t));
    for (uint32_t i = 0; i < old_capacity; i++) {
        if (old[i].key) {
            ecs_pair_slot_insert(index->pair_slots, capacity - 1, old[i]);
        }
    }
    free(old);
}

static ecs_pair_table_slot_t *
ecs_pair_slot(ecs_table_index_t *index, uint16_t key, uint64_t value, bool create) {
    if (!index->pair_slot_shift ||
        (create && (index->pair_slot_count + 1) * 4 >= ecs_pair_slot_capacity(index) * 3)) {
        if (!create) {
            return NULL;
        }
        ecs_pair_slots_grow(index);
    }
    uint32_t mask = ecs_pair_slot_capacity(index) - 1;
    uint32_t i = ecs_pair_hash(key, value) & mask;
    while (index->pair_slots[i].key &&
           (index->pair_slots[i].key != key || index->pair_slots[i].value != value)) {
        i = (i + 1) & mask;
    }
    ecs_pair_table_slot_t *slot = &index->pair_slots[i];
    if (!slot->key && create) {
        slot->key = key;
        slot->value = value;
        index->pair_slot_count++;
    }
    return slot->key ? slot : NULL;
}

ecs_pair_tables_t ecs_table_index_pair_tables(uint16_t key, uint64_t value) {
    ecs_pair_table_slot_t *slot = ecs_pair_slot(&table_index, key, value, false);
    if (!slot) {
        return (ecs_pair_tables_t){ 0 };
    }
    return (ecs_pair_tables_t){
        .ids = slot->tables ? slot->tables : &slot->first_table,
        .count = slot->table_count,
    };
}

static void ecs_pair_slot_add_table(ecs_pair_table_slot_t *slot, uint16_t table) {
    if (!slot->table_count) {
        slot->first_table = table;
    } else {
        if (!slot->tables) {
            slot->table_capacity = 4;
            slot->tables = malloc(sizeof(uint16_t) * slot->table_capacity);
            slot->tables[0] = slot->first_table;
        } else if (slot->table_count == slot->table_capacity) {
            slot->table_capacity *= 2;
            slot->tables = realloc(slot->tables, sizeof(uint16_t) * slot->table_capacity);
        }
        slot->tables[slot->table_count] = table;
    }
    slot->table_count++;
}

static void ecs_table_index_pairs(const ecs_table_t *table, uint16_t table_id) {
    const ecs_type_pair_t *pairs = ecs_type_pairs(&table->type);
    for (uint16_t i = 0; i < table->type.pair_count; i++) {
        ecs_pair_table_slot_t *slot =
            ecs_pair_slot(&table_index, pairs[i].key, pairs[i].value, true);
        ecs_pair_slot_add_table(slot, table_id);
    }
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
    ecs_table_index_t *map = &table_index;
    map->table_count = 0;
    map->table_capacity = 1;
    map->tables = malloc(sizeof(ecs_table_t) * map->table_capacity);
    map->slot_shift = INITIAL_SLOT_SHIFT;
    ecs_table_index_init_slots(map);
    map->pair_slots = NULL;
    map->pair_slot_count = 0;
    map->pair_slot_shift = 0;
}

void ecs_table_index_fini() {
    ecs_table_index_t *map = &table_index;
    for (uint16_t i = 0; i < map->table_count; i++) {
        ecs_table_fini(&map->tables[i]);
    }
    uint32_t pair_capacity = ecs_pair_slot_capacity(map);
    for (uint32_t i = 0; i < pair_capacity; i++) {
        if (map->pair_slots[i].key) {
            free(map->pair_slots[i].tables);
        }
    }
    free(map->pair_slots);
    free(map->tables);
    free(map->slots);
    *map = (ecs_table_index_t){ 0 };
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

        for (uint16_t i = 0; i < base_table->type.component_count; i++) {
            ecs_component_t component = base_table->type.ids[i];
            if (ecs_table_column_or_invalid(table, component) != UINT16_MAX ||
                ecs_table_index_inherits_component_before(table, base, component)) {
                continue;
            }

            table->bloom |= 1ull << (component % 64);
            ecs_component_record_t *record = ecs_component_index_get(component);
            sicore_vec_push_u16(&record->tables, table_id);
        }

        base = base_table->type.base;
    }
}

uint16_t ecs_table_index_get_or_create(ecs_type_t type) {
    ecs_table_index_t *map = &table_index;
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
    ecs_table_index_pairs(&map->tables[table_idx], table_idx);
    ecs_table_index_register_inherited_components(&map->tables[table_idx], table_idx);

    map->slots[slot_idx].hash = hash_fingerprint;
    map->slots[slot_idx].table_index = table_idx;

    ecs_query_index_add_table(ecs_table_index_at(table_idx), table_idx);
    system_index.plan_dirty = true;
    return (uint16_t)table_idx;
}

