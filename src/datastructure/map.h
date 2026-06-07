#ifndef SIECS_DATASTRUCTURE_MAP_H
#define SIECS_DATASTRUCTURE_MAP_H
#ifndef NDEBUG

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    const char *key;
    uint64_t hash;
    uint32_t value;
} ecs_map_slot_t;

typedef struct {
    ecs_map_slot_t *slots;
    size_t cap;
    size_t len;
} ecs_map_t;

void ecs_map_init(ecs_map_t *m, size_t initial_capacity);
void ecs_map_fini(ecs_map_t *m);

void ecs_map_set(ecs_map_t *m, const char *key, uint32_t value);
uint32_t ecs_map_get(const ecs_map_t *m, const char *key);
bool ecs_map_has(const ecs_map_t *m, const char *key);
#endif

#endif
