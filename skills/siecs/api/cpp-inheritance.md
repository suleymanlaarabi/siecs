# C++20 API: inheritance

[Usage and contracts](../references/cpp/inheritance.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C inheritance reference](c-inheritance.md). Public base members also apply to derived builders.

## Functions

- `ecs::instantiate` — `inline entity instantiate(entity base)`

## Types and members

- `ecs::entity::abstract` — `entity abstract()`
- `ecs::entity::instantiate` — `static entity instantiate(entity prefab)`
- `ecs::entity::is` — `[[nodiscard]] bool is(entity target) const`
- `ecs::entity::is` — `template <typename T> bool is()`
- `ecs::entity::is_a` — `entity is_a(ecs_entity_t target)`
- `ecs::entity::is_a` — `entity is_a(entity target)`
- `ecs::entity::is_a` — `template <typename T> entity is_a()`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ecs_propagate_add` — `ecs_propagate_add(entity,relation,component)`
