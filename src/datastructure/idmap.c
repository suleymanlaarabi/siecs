#include "idmap.h"
#include "helper.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
