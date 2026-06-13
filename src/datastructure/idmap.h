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
