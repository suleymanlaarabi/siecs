#pragma once
#include <stdint.h>

#define ecs_id(name) _ecs_id_##name##__

struct ecs_world_s;
typedef struct ecs_world_s ecs_world_t;
typedef uint64_t ecs_entity_t;
typedef uint16_t ecs_component_t;
typedef uint16_t ecs_query_id_t;
typedef uint16_t ecs_system_id_t;
typedef uint16_t ecs_event_t;

typedef struct {
    ecs_world_t *world;
    ecs_entity_t entity;
    ecs_event_t event;
    uintptr_t user_data;
    const void *trigger_data;
} ecs_observer_event_t;

typedef void (*ecs_observer_callback_t)(ecs_observer_event_t *event);
// Component hooks receive the new value passed to ecs_set_cid. The stored value
// is still the old one until the hook returns, so ecs_get_cid can read old data.
typedef void (*ecs_component_hook_t)(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *ptr
);

typedef struct {
    const char *name;
    uint64_t size;
    ecs_component_hook_t on_set;
    ecs_component_hook_t on_remove;
    bool is_relation;
    const char *source_name;
} ecs_component_desc_t;

typedef struct {
    ecs_component_t read[8];
    ecs_component_t required[6];
    ecs_component_t excluded[4];
} ecs_query_desc_t;

ecs_world_t *ecs_init();
void ecs_fini(ecs_world_t *world);

#define ECS_COMPONENT_DECLARE(cname, ...)                                                          \
    typedef struct __VA_ARGS__ cname;                                                              \
    extern ecs_component_t ecs_id(cname);                                                          \
    extern ecs_component_desc_t ecs_id(cname##_desc)

#define ECS_COMPONENT_DEFINE(cname)                                                                \
    ecs_component_desc_t ecs_id(cname##_desc) = {                                                  \
        .name = #cname,                                                                            \
        .size = sizeof(cname),                                                                     \
    };                                                                                             \
    ecs_component_t ecs_id(cname) = 0

#define ECS_COMPONENT_REGISTER(world, cname)                                                       \
    ecs_id(cname) = ecs_component_init(world, &ecs_id(cname##_desc))

#define ECS_RELATION_DEFINE(cname)                                                                 \
    ecs_component_desc_t ecs_id(cname##_desc) = {                                                  \
        .name = #cname,                                                                            \
        .size = sizeof(cname),                                                                     \
        .is_relation = true,                                                                       \
        .source_name = "Source" #cname,                                                            \
    };                                                                                             \
    ecs_component_t ecs_id(cname) = 0

#define ecs_source(name) (ecs_id(name) + 1)

#define ECS_RELATION_DECLARE(name) ECS_COMPONENT_DECLARE(name, { ecs_entity_t target; })

ECS_RELATION_DECLARE(ChildOf);

#define ecs_component(world, ...) ecs_component_init(world, &(ecs_component_desc_t)__VA_ARGS__)
ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc);

ecs_entity_t ecs_new(ecs_world_t *world);
int ecs_is_alive(const ecs_world_t *world, ecs_entity_t entity);
void ecs_kill(ecs_world_t *world, ecs_entity_t entity);

#define ecs_query(world, ...) ecs_query_init(world, &(ecs_query_desc_t)__VA_ARGS__)
uint32_t ecs_query_init(ecs_world_t *world, const ecs_query_desc_t *query);

void ecs_query_fini(ecs_world_t *world, ecs_query_id_t qid);

#define ecs_add(world, entity, cname) ecs_add_cid(world, entity, ecs_id(cname))
void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

#define ecs_remove(world, entity, cname) ecs_remove_cid(world, entity, ecs_id(cname))
void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

#define ecs_has(world, entity, cname) ecs_has_cid(world, entity, ecs_id(cname))
bool ecs_has_cid(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

#define ecs_get(world, entity, cname) ((cname *)ecs_get_cid(world, entity, ecs_id(cname)))
void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

#define ecs_try_get(world, entity, cname) ((cname *)ecs_try_get_cid(world, entity, ecs_id(cname)))
void *ecs_try_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid);

#define ecs_set(world, entity, cname, ...)                                                         \
    ecs_set_cid(world, entity, ecs_id(cname), &(cname)__VA_ARGS__)
void ecs_set_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id, const void *data);

void ecs_with(ecs_world_t *world, ecs_component_t component, ecs_component_t require);

// Observers & events.
// Event ids are dense and live in their own namespace (separate from entity and
// component ids). Builtin events occupy the first slots; ecs_event() hands out
// the next dense id for user-defined events.
enum {
    OnAdd = 0,
    OnRemove = 1,
    OnSet = 2,
};

typedef struct {
    ecs_event_t on;
    ecs_query_desc_t query;
    ecs_observer_callback_t callback;
    uintptr_t user_data;
} ecs_observer_desc_t;

#define ecs_observer(world, ...) ecs_observer_init(world, &(ecs_observer_desc_t)__VA_ARGS__)

ecs_event_t ecs_event(ecs_world_t *world);
uint32_t ecs_observer_init(ecs_world_t *world, const ecs_observer_desc_t *desc);
void ecs_observer_trigger(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_event_t event,
    const void *trigger_data
);

typedef struct {
    ecs_world_t *world;
    uint32_t count;
    struct ecs_query_cache_s *cache;
    void ***ptrs;
    uint16_t table_idx;
    uint16_t table_count;
} ecs_iter_t;

ecs_iter_t ecs_query_iter(ecs_world_t *world, ecs_query_id_t query_id);
bool ecs_iter_next(ecs_iter_t *it);
static inline void *ecs_field(ecs_iter_t *it, uint16_t query_term) { return *it->ptrs[query_term]; }

typedef enum {
    OnPreUpdate,
    OnUpdate,
    OnPostUpdate,
    OnRender,
} ecs_phase_t;

typedef struct {
    ecs_query_desc_t query;
    void (*callback)(ecs_iter_t *);
    ecs_phase_t phase;
    ecs_system_id_t after[4];
} ecs_system_desc_t;

#define ecs_system(world, ...) ecs_system_init(world, &(ecs_system_desc_t)__VA_ARGS__)
ecs_system_id_t ecs_system_init(ecs_world_t *world, const ecs_system_desc_t *desc);
