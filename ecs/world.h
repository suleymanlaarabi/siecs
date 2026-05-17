#pragma once
#include <stdbool.h>
#include <stdint.h>
#define ecs_id(name) _ecs_id_##name##__

struct ecs_world_s;
typedef struct ecs_world_s ecs_world_t;
typedef uint64_t ecs_entity_t;
typedef uint16_t ecs_component_t;
typedef uint32_t ecs_query_id_t;

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

#define ECS_COMPONENT_DECLARE(cname, ...)                                                          \
    typedef struct __VA_ARGS__ cname;                                                              \
    extern ecs_component_t ecs_id(cname);                                                          \
    extern ecs_component_desc_t ecs_id(cname##_desc);

#define ECS_COMPONENT_DEFINE(cname)                                                                \
    ecs_component_desc_t ecs_id(cname##_desc) = {                                                  \
        .name = #cname,                                                                            \
        .size = sizeof(cname),                                                                     \
    };                                                                                             \
    ecs_component_t ecs_id(cname)

#define ECS_BIT_DEFINE(cname)                                                                      \
    ecs_component_desc_t ecs_id(cname##_desc) = { .name = #cname, .size = 0, .is_bitset = true };  \
    ecs_component_t ecs_id(cname)

#define ECS_COMPONENT_REGISTER(world, cname)                                                       \
    ecs_id(cname) = ecs_component_init(world, &ecs_id(cname##_desc))

#define ecs_component(world, ...) ecs_component_init(world, &(ecs_component_desc_t)__VA_ARGS__)
ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc);

ecs_entity_t ecs_new(ecs_world_t *world);
int ecs_is_alive(ecs_world_t *world, ecs_entity_t entity);
void ecs_kill(ecs_world_t *world, ecs_entity_t entity);

#define ecs_query(world, ...) ecs_query_init(world, &(ecs_query_desc_t)__VA_ARGS__)
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

// Observers & events.
// Event ids are dense and live in their own namespace (separate from entity and
// component ids). Builtin events occupy the first slots; ecs_event() hands out
// the next dense id for user-defined events.
typedef uint16_t ecs_event_t;

enum {
    OnAdd = 0,
    OnRemove = 1,
    OnSet = 2,
};

typedef void (*ecs_observer_callback_t)(ecs_world_t *world, ecs_entity_t entity, void *event);

typedef struct {
    ecs_event_t on;
    ecs_query_desc_t query;
    ecs_observer_callback_t callback;
} ecs_observer_desc_t;

#define ecs_observer(world, ...) ecs_observer_init(world, &(ecs_observer_desc_t)__VA_ARGS__)

ecs_event_t ecs_event(ecs_world_t *world);
uint32_t ecs_observer_init(ecs_world_t *world, const ecs_observer_desc_t *desc);
void ecs_observer_trigger(ecs_world_t *world, ecs_entity_t entity, ecs_event_t event, void *data);

typedef struct {
    ecs_world_t *world;
    ecs_query_id_t query_id;
    uint16_t table_idx;
    uint32_t table_count;
} ecs_iter_t;

ecs_iter_t ecs_query_iter(ecs_world_t *world, ecs_query_id_t query_id);
bool ecs_iter_next(ecs_iter_t *it);
struct ecs_table_s *ecs_iter_table(ecs_iter_t *it);
void *ecs_field(ecs_iter_t *it, ecs_component_t cid);

typedef struct {
    const uint64_t *words;
    uint32_t word_count;
} ecs_bitfield_t;

ecs_bitfield_t ecs_bitfield(ecs_iter_t *it, ecs_component_t cid);

#define ECS_BITS_FOREACH_SET(bits, name, ...)                                                      \
    do {                                                                                           \
        const uint64_t *_ecs_words = (bits).words;                                                 \
        uint32_t _ecs_word_count = (bits).word_count;                                              \
        for (uint32_t _ecs_w = 0, _ecs_base = 0; _ecs_w < _ecs_word_count;                         \
             ++_ecs_w, _ecs_base += 64u) {                                                         \
            uint64_t _ecs_word = _ecs_words[_ecs_w];                                               \
            while (_ecs_word) {                                                                    \
                uint32_t name = _ecs_base + (uint32_t)__builtin_ctzll(_ecs_word);                  \
                _ecs_word &= _ecs_word - 1;                                                        \
                __VA_ARGS__                                                                        \
            }                                                                                      \
        }                                                                                          \
    } while (0)
