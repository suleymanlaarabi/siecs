---
title: Queries
description: Creating queries and iterating matching entities.
---

Queries match archetype tables and return component arrays in batches.

## Create A Query

```c
ecs_query_id_t moving = ecs_query(world, {
    .terms = { ecs_inout(Position), ecs_in(Velocity) },
});
```

The `terms` array is a zero-terminated list of component terms.

| Term | Meaning |
| --- | --- |
| `ecs_in(T)` | `T` must exist and is returned by `ecs_field()` for reading. |
| `ecs_out(T)` | `T` must exist and is returned by `ecs_field()` for writing. |
| `ecs_inout(T)` | `T` must exist and is returned by `ecs_field()` for reading and writing. |
| `ecs_filter(T)` | `T` must exist but is not returned. |
| `ecs_not(T)` | `T` must not exist. |

Current descriptor limits:

| Field | Maximum entries |
| --- | --- |
| `terms` | 16 |

## Iterate

```c
ecs_iter_t it = ecs_query_iter(world, moving);

while (ecs_iter_next(&it)) {
    Position *positions = ecs_field(&it, 0);
    Velocity *velocities = ecs_field(&it, 1);

    for (uint32_t i = 0; i < it.count; i++) {
        positions[i].x += velocities[i].x;
        positions[i].y += velocities[i].y;
    }
}
```

`ecs_iter_next()` advances to the next non-empty batch. `it.count` is the number
of entities in the current batch.

`ecs_field(&it, index)` returns component arrays for `ecs_in`, `ecs_out`, and
`ecs_inout` terms in declaration order. `ecs_filter` and `ecs_not` do not create
field indexes.

## Required And Excluded Components

Use `ecs_filter` when a component must exist but does not need to be returned:

```c
ecs_query_id_t visible_positions = ecs_query(world, {
    .terms = { ecs_inout(Position), ecs_filter(Visible) },
});
```

Use `ecs_not` to skip entities with a component:

```c
ecs_query_id_t active_positions = ecs_query(world, {
    .terms = { ecs_inout(Position), ecs_not(Disabled) },
});
```

## Destroy A Query

```c
ecs_query_fini(world, moving);
```

Destroy queries before destroying the world if you no longer need them. The
world also owns query storage and releases it during `ecs_fini()`.
