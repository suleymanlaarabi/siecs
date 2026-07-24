#include "type.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
