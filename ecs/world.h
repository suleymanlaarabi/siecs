#pragma once
#include <stdint.h>
#include <stdbool.h>
#define ecs_id(name) _ecs_id_##name##__

struct ecs_world_s;
typedef struct ecs_world_s ecs_world_t;
typedef uint64_t ecs_entity_t;
typedef uint16_t ecs_component_t;

typedef struct {
    const char *name;
    uint64_t size;
    bool is_bitset;
} ecs_component_desc_t;

#ifndef MAX_QUERY_REQUIRED
#define MAX_QUERY_REQUIRED 8
#endif

#ifndef MAX_QUERY_EXCLUDED
#define MAX_QUERY_EXCLUDED 4
#endif

typedef struct {
    ecs_component_t required[MAX_QUERY_REQUIRED];
    ecs_component_t excluded[MAX_QUERY_EXCLUDED];
} ecs_query_desc_t;

ecs_world_t *ecs_init();
void ecs_fini(ecs_world_t *world);

#define ECS_COMPONENT_DECLARE(cname, ...) typedef struct __VA_ARGS__ cname; \
    extern ecs_component_t ecs_id(cname); \
    extern ecs_component_desc_t ecs_id(cname##_desc);

#define ECS_COMPONENT_DEFINE(cname) \
    ecs_component_desc_t ecs_id(cname##_desc) = { \
        .name = #cname, \
        .size = sizeof(cname), \
    }; \
    ecs_component_t ecs_id(cname);

#define ECS_BIT_DEFINE(cname) \
ecs_component_desc_t ecs_id(cname##_desc) = { \
        .name = #cname, \
        .size = 0, \
        .is_biset = true \
    }; \
    ecs_component_t ecs_id(cname);

#define ECS_COMPONENT_REGISTER(world, cname) \
    ecs_id(cname) = ecs_component_init(world, &ecs_id(cname##_desc))

#define ecs_component(world, ...) ecs_component_init(world, &(ecs_component_desc_t) __VA_ARGS__)
ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc);

ecs_entity_t ecs_new(ecs_world_t *world);
int ecs_is_alive(ecs_world_t *world, ecs_entity_t entity);
void ecs_kill(ecs_world_t *world, ecs_entity_t entity);

#define ecs_query(world, ...) ecs_query_init(world, &(ecs_query_desc_t) __VA_ARGS__)
uint32_t ecs_query_init(ecs_world_t *world, const ecs_query_desc_t *query);

#define ecs_add(world, entity, cname) ecs_add_cid(world, entity, ecs_id(cname))
void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

#define ecs_remove(world, entity, cname) ecs_remove_cid(world, entity, ecs_id(cname))
void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

#define ecs_has(world, entity, cname) ecs_has_cid(world, entity, ecs_id(cname))
bool ecs_has_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

#define ecs_get(world, entity, cname) ecs_get_cid(world, entity, ecs_id(cname))
void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

void ecs_set_bit(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id, bool value);
bool ecs_get_bit(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

void ecs_with(ecs_world_t *world, ecs_component_t component, ecs_component_t require);
