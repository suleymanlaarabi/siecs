#include "type.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline void ecs_type_sort(ecs_type_t *type) {
    // insertion sort is efficient for nearly sorted arrays
    for (int i = 1; i < type->count; i++) {
        uint16_t key = type->ids[i];
        int j = i - 1;
        while (j >= 0 && type->ids[j] > key) {
            type->ids[j + 1] = type->ids[j];
            j--;
        }
        type->ids[j + 1] = key;
    }
}

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id) {
    ecs_type_t new_type = {
        .ids = malloc((type->count + 1) * sizeof(uint16_t)),
        .count = type->count + 1,
    };
    memcpy(new_type.ids, type->ids, type->count * sizeof(uint16_t));
    new_type.ids[type->count] = id;
    ecs_type_sort(&new_type);
    return new_type;
}

ecs_type_t ecs_type_with_remove(const ecs_type_t *type, uint16_t id) {
    ecs_type_t new_type = {
        .ids = malloc((type->count - 1) * sizeof(uint16_t)),
        .count = type->count - 1,
    };
    int j = 0;
    for (int i = 0; i < type->count; i++) {
        if (type->ids[i] != id) {
            new_type.ids[j++] = type->ids[i];
        }
    }
    return new_type;
}

// returns the index of the id in the type, or -1 if not found
int ecs_type_find(const ecs_type_t *type, uint16_t id) {
    // binary search
    int left = 0, right = type->count - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (type->ids[mid] == id) {
            return mid;
        } else if (type->ids[mid] < id) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}

void ecs_type_fini(ecs_type_t *type) {
    if (type->ids) {
        free(type->ids);
        type->ids = NULL;
    }
}

uint64_t ecs_type_bloom(ecs_type_t *type) {
    uint64_t filter = 0;

    for (uint16_t i = 0; i < type->count; i++) {
        filter |= (1ull << (type->ids[i] % 64));
    }

    return filter;
}
