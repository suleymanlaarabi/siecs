#ifndef SIECS_UTILS_H
#define SIECS_UTILS_H
#include <stdio.h>
#include <stdlib.h>

/* Invariant whose violation would corrupt the world: checked in every build. */
#define ecs_fatal_if(condition, ...)                                                               \
    do {                                                                                           \
        if (condition) {                                                                           \
            fprintf(stderr, __VA_ARGS__);                                                          \
            abort();                                                                               \
        }                                                                                          \
    } while (0)

#ifndef NDEBUG
#define ecs_cid_valid(id) ((id) != 0)
#define ecs_entity_valid(entity) (ecs_first(entity) != 0)

#define ecs_assert(condition, ...) ecs_fatal_if(!(condition), __VA_ARGS__)

#define ecs_assert_id_valid(id)                                                                    \
    ecs_assert(ecs_cid_valid(id), "invalid id: %d, id must be registered\n", id)
#define ecs_assert_not_null(ptr) ecs_assert((ptr) != NULL, "null pointer: %s\n", #ptr)
#define ecs_assert_entity_valid(entity)                                                            \
    ecs_assert(                                                                                    \
        ecs_entity_valid(entity),                                                                  \
        "invalid entity: %d, entity must be registered\n",                                         \
        ecs_first(entity)                                                                          \
    )
#define ecs_assert_is_alive(entity)                                                                \
    ecs_assert(ecs_is_alive(entity), "entity is dead: %d\n", ecs_first(entity))
#define ecs_assert_entity_alive(entity)                                                            \
    do {                                                                                           \
        ecs_assert_entity_valid(entity);                                                           \
        ecs_assert_is_alive(entity);                                                               \
    } while (0)
#define ecs_assert_component_access(entity, id)                                                    \
    do {                                                                                           \
        ecs_assert_id_valid(id);                                                                   \
        ecs_assert_entity_alive(entity);                                                           \
    } while (0)

#else
#define ecs_assert(condition, ...) ((void)0)
#define ecs_assert_id_valid(id) ((void)0)
#define ecs_assert_not_null(ptr) ((void)0)
#define ecs_assert_entity_valid(entity) ((void)0)
#define ecs_assert_is_alive(entity) ((void)0)
#define ecs_assert_entity_alive(entity) ((void)0)
#define ecs_assert_component_access(entity, id) ((void)0)
#endif

#endif
