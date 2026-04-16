#include "bitset.h"
#include <stdlib.h>

void ecs_bitset_init(ecs_bit_set *bitset, uint64_t bit_count) {
    uint64_t word_count = (bit_count + 63) / 64;
    bitset->words = (uint64_t *)calloc(word_count, sizeof(uint64_t));
}

void ecs_bitset_grow(ecs_bit_set *bitset, uint64_t bit_count) {
    uint64_t word_count = (bit_count + 63) / 64;
    bitset->words = realloc(bitset->words, word_count * sizeof(uint64_t));
}
