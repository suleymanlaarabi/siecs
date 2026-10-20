# C17 API: inheritance

[Usage and contracts](../references/c/inheritance.md). Signatures only; consult the guide for behavior.

## Functions

- `ecs_abstract` — `void ecs_abstract(ecs_entity_t entity)`
- `ecs_entity_base` — `ecs_entity_t ecs_entity_base(ecs_entity_t entity)`
- `ecs_is` — `bool ecs_is(ecs_entity_t entity, ecs_entity_t target)`
- `ecs_is_a` — `void ecs_is_a(ecs_entity_t entity, ecs_entity_t target)`
- `ecs_propagate_add_id` — `void ecs_propagate_add_id(ecs_entity_t entity, ecs_relation_id_t relation, ecs_component_t component)`

## Types and members

- `ecs_component_inheritance_t` — `typedef enum { /* values below */ } ecs_component_inheritance_t`
- `EcsInheritOwned` — `EcsInheritOwned = 0`
- `EcsInheritShared` — `EcsInheritShared = 1`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ecs_propagate_add` — `ecs_propagate_add(entity,relation,component)`
