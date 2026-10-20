# C17 API: entities

[Usage and contracts](../references/c/entities.md). Signatures only; consult the guide for behavior.

## Functions

- `ecs_entity_from_index` — `ecs_entity_t ecs_entity_from_index(uint32_t index)`
- `ecs_entity_name` — `const char *ecs_entity_name(ecs_entity_t entity)`
- `ecs_entity_path` — `char *ecs_entity_path(ecs_entity_t entity)`
- `ecs_is_alive` — `bool ecs_is_alive(const ecs_entity_t entity)`
- `ecs_kill` — `void ecs_kill(ecs_entity_t entity)`
- `ecs_lookup` — `ecs_entity_t ecs_lookup(const char *path)`
- `ecs_lookup_from` — `ecs_entity_t ecs_lookup_from(ecs_entity_t parent, const char *path)`
- `ecs_new` — `ecs_entity_t ecs_new(void)`
- `ecs_new_named` — `ecs_entity_t ecs_new_named(ecs_entity_t parent, const char *path)`
- `ecs_new_no_reuse` — `ecs_entity_t ecs_new_no_reuse(void)`
- `ecs_set_name` — `bool ecs_set_name(ecs_entity_t entity, const char *name)`

## Types and members

- `Abstract` — `typedef struct Abstract_tag_t Abstract`
- `Disabled` — `typedef struct Disabled_tag_t Disabled`
- `Name` — `typedef struct { /* fields below */ } Name`
- `Name.value` — `char *value`
- `ecs_entity_t` — `typedef uint64_t ecs_entity_t`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ecs_entity_generation` — `ecs_entity_generation(entity)`
- `ecs_entity_id` — `ecs_entity_id(entity)`
