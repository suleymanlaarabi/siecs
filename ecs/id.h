#pragma once
#include <stdint.h>

#define ecs_entity(index, generation)                                              \
  (((uint64_t)(index) << 32) | (generation & 0xffffffff))

#define ecs_first(id) ((uint32_t)((id) >> 32))
#define ecs_second(id) ((uint32_t)((id) & 0xffffffff))

typedef uint64_t ecs_entity_t;
typedef uint16_t ecs_component_t;
