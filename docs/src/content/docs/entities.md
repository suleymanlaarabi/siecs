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

## Handle Layout

The public API treats `ecs_entity_t` as an opaque handle. Internally, SIECS packs
an entity index and generation into a 64-bit value so stale handles can be
detected.
