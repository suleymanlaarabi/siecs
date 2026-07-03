---
title: Entities
description: Entity handles, liveness, and destruction.
---

Entities are `ecs_entity_t` handles created by a world.

```c
ecs_entity_t entity = ecs_new(world);
```

New entities start alive and have no user components.

## Liveness

Use `ecs_is_alive()` with entity handles created by the same world:

```c
if (ecs_is_alive(world, entity)) {
    /* entity can be used */
}
```

`ecs_is_alive()` is not a general validator for arbitrary integers. Passing ids
that did not come from the world is not supported.

## Destroy Entities

```c
ecs_kill(world, entity);
```

Killing an entity removes all of its components, runs remove hooks, removes the
entity from its current archetype table, and invalidates the handle.

After `ecs_kill()`, do not use the old handle with component access functions.

## Abstract Entities And Inheritance

An abstract entity is a base entity used for inheritance or shared defaults
rather than normal per-instance state:

```c
ecs_entity_t base = ecs_new(world);
ecs_add(world, base, Abstract);

ecs_entity_t instance = ecs_new(world);
ecs_is_a(world, instance, base);
```

Once an entity has `Abstract`, normal component add, set, and remove operations
on that entity are rejected. Put component data on the abstract entity before
marking it abstract, or use a normal entity when you need to mutate it directly.

Use `ecs_is()` to test inheritance:

```c
if (ecs_is(world, instance, base)) {
    /* instance inherits from base */
}
```

Queries can be restricted to entities that inherit from a base with
`ecs_query_desc_t.is_a`:

```c
ecs_query_id_t q = ecs_query(world, {
    .is_a = base,
    .terms = { ecs_in(Position) },
});
```

## Handle Layout

The public API treats `ecs_entity_t` as an opaque handle. Internally, SIECS packs
an entity index and generation into a 64-bit value so stale handles can be
detected.
