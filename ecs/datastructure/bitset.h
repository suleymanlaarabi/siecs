#pragma once
#include <stdint.h>

typedef struct {
    uint64_t *words;
} ecs_bit_set;

void ecs_bitset_init(ecs_bit_set *bitset, uint64_t bit_count);

static inline bool ecs_bitset_is_set(const ecs_bit_set *bitset, uint64_t i) {
    return (bitset->words[i >> 6] & (1ULL << (i & 63))) != 0;
}

static inline void ecs_bitset_set(ecs_bit_set *bitset, uint64_t i) {
    bitset->words[i >> 6] |= (1ULL << (i & 63));
}

static inline void ecs_bitset_unset(ecs_bit_set *bitset, uint64_t i) {
    bitset->words[i >> 6] &= ~(1ULL << (i & 63));
}

void ecs_bitset_grow(ecs_bit_set *bitset, uint64_t bit_count);

#define ECS_BITSET_ITER(words_, bitcount_, name, ...)                                              \
    do {                                                                                           \
        uint64_t _ecs_word_count = ((uint64_t)(bitcount_) + 63u) >> 6;                             \
        const uint64_t *_ecs_words = (words_);                                                     \
        for (uint64_t _ecs_w = 0; _ecs_w < _ecs_word_count; ++_ecs_w) {                            \
            uint64_t _ecs_word = _ecs_words[_ecs_w];                                               \
            while (_ecs_word) {                                                                    \
                uint32_t _ecs_bit = (uint32_t)__builtin_ctzll(_ecs_word);                          \
                uint64_t name = (_ecs_w << 6) + _ecs_bit;                                          \
                __VA_ARGS__                                                                        \
                _ecs_word &= _ecs_word - 1;                                                        \
            }                                                                                      \
        }                                                                                          \
    } while (0)
