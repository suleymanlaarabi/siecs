---
title: Queries
description: Creating queries and iterating matching entities.
---

Queries match archetype tables and return component arrays in batches.

## Create A Query

```c
ecs_query_id_t moving = ecs_query(world, {
    .read = { ecs_id(Position), ecs_id(Velocity) },
});
```

The `read`, `required`, and `excluded` arrays are zero-terminated component id
lists.

| Field | Meaning |
| --- | --- |
| `read` | Components returned by `ecs_field()`. At least one read component is required. |
| `required` | Components that must exist but are not returned. |
| `excluded` | Components that must not exist. |

Current descriptor limits:

| Field | Maximum entries |
| --- | --- |
| `read` | 10 |
| `required` | 8 |
| `excluded` | 6 |

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

`ecs_field(&it, index)` returns the component array for the matching entry in
`.read`. The index is zero-based.

## Required And Excluded Components

Use `required` when a component must exist but does not need to be returned:

```c
ecs_query_id_t visible_positions = ecs_query(world, {
    .read = { ecs_id(Position) },
    .required = { ecs_id(Visible) },
});
```

Use `excluded` to skip entities with a component:

```c
ecs_query_id_t active_positions = ecs_query(world, {
    .read = { ecs_id(Position) },
    .excluded = { ecs_id(Disabled) },
});
```

## Destroy A Query

```c
ecs_query_fini(world, moving);
```

Destroy queries before destroying the world if you no longer need them. The
world also owns query storage and releases it during `ecs_fini()`.
