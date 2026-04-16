#pragma once
#include "ecs/datastructure/vec.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t *ids;
    uint16_t capacity;
} ecs_id_map_t;

void ecs_id_map_init(ecs_id_map_t *map);
void ecs_id_map_fini(ecs_id_map_t *map);

static inline void ecs_id_map_ensure(ecs_id_map_t *map, uint16_t id) {
    if (ECS_UNLIKELY(id >= map->capacity)) {
        map->ids = realloc(map->ids, sizeof(uint16_t) * (id + 1));
        memset(map->ids + map->capacity, 0xFF, sizeof(uint16_t) * (id - map->capacity));
        map->capacity = id + 1;
    }
}

static inline void ecs_id_map_set(ecs_id_map_t *map, uint16_t id, uint16_t value) {
    ecs_id_map_ensure(map, id);
    map->ids[id] = value;
}

static inline uint16_t ecs_id_map_at(ecs_id_map_t *map, uint16_t id) {
    return map->ids[id];
}

static inline uint16_t ecs_id_map_at_or_invalid(const ecs_id_map_t *map, uint16_t id) {
    if (map->capacity > id) {
        return map->ids[id];
    }
    return UINT16_MAX;
}

static inline uint16_t ecs_id_map_has(ecs_id_map_t *map, uint16_t id) {
    if (map->capacity > id) {
        return map->ids[id] != UINT16_MAX;
    }
    return false;
}
