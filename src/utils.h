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
#define ecs_assert_is_alive(world, entity) ecs_assert(ecs_is_alive(world, entity), "entity is dead: %d\n", ecs_first(entity))

#else
#define ecs_assert(condition, ...)
#define ecs_assert_id_valid(id)
#define ecs_assert_not_null(ptr)
#define ecs_assert_entity_valid(entity)
#define ecs_assert_is_alive(world, entity)
#endif

#endif
