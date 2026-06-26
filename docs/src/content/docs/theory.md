---
title: ECS Theory
description: How SIECS models data, storage, queries, and structural changes.
---

SIECS is an archetype ECS. Entities are ids, components are plain data, and
systems run logic over batches of entities that share the same component set.

## Entities Are Handles

An `ecs_entity_t` identifies a row in world-owned storage. It is not an object
and it does not own behavior. Keep the handle, but store state in components:

```c
ecs_entity_t player = ecs_new(world);
ecs_set(world, player, Position, { .x = 0, .y = 0 });
ecs_set(world, player, Velocity, { .x = 1, .y = 0 });
```

Entity handles belong to one world. Do not use a handle with a different world.

## Components Are Data

A component is a registered type. Data components have a non-zero size, while
tags have size `0` and represent state through presence:

```c
ECS_COMPONENT(Player, {});
ecs_add(world, entity, Player);
```

Use components for per-entity state. Use resources for one value stored on the
world, such as time, input, renderer state, or shared configuration.

## Archetype Tables

SIECS stores entities in archetype tables keyed by component set. Adding or
removing a component is a structural change: the entity moves to another table.
Shared component columns are copied, removed columns are dropped, and newly
added data starts zeroed before `ecs_set()` writes the final value.

This is why iteration is fast: a query visits matching tables and receives
contiguous arrays for each requested component.

## Queries Match Tables

A query does not scan every entity one by one. It stores the matching table ids
and returns batches:

```c
ecs_iter_t it = ecs_query_iter(world, moving);
while (ecs_iter_next(&it)) {
    Position *p = ecs_field(&it, 0);
    Velocity *v = ecs_field(&it, 1);
}
```

`ecs_filter(T)` and `ecs_not(T)` affect matching but do not consume field
indexes. Optional terms consume a field index and return `NULL` for tables where
the component is absent.

## Structural Changes During Iteration

Writing component values in place is expected. Adding or removing components can
move entities between tables and invalidate the current batch pointers. Prefer
collecting structural changes and applying them after iteration unless a
specific API documents that the operation is safe during iteration.

## Disabled Entities

Adding the built-in `Disabled` component excludes an entity from queries,
systems, and observers by default. Mention `Disabled` explicitly when you want
to match disabled entities:

```c
ecs_query(world, {
    .terms = { ecs_in(Position), ecs_filter(Disabled) },
});
```
