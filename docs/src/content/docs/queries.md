---
title: Queries
description: Creating required/excluded component queries and iterating tables.
---

Queries cache matching archetype tables.

## Create a query

```c
ecs_query_id_t moving = ecs_query(world, {
    .read = { ecs_id(Position), ecs_id(Velocity) },
    .required = { ecs_id(Position), ecs_id(Velocity) },
});
```

Read and required component arrays are capped at 8 components, and excluded
arrays are capped at 4 components.

## Exclude components

```c
ecs_query_id_t visible_moving = ecs_query(world, {
    .read = { ecs_id(Position), ecs_id(Velocity) },
    .required = { ecs_id(Position), ecs_id(Velocity) },
    .excluded = { ecs_id(Hidden) },
});
```

## Iterate

`ecs_iter_next()` advances table by table. Each table contains entities with the
same component set.

```c
ecs_iter_t it = ecs_query_iter(world, moving);

while (ecs_iter_next(&it)) {
    ecs_table_t *table = ecs_iter_table(&it);
    Position *positions = ecs_field(&it, 0);
    Velocity *velocities = ecs_field(&it, 1);

    for (uint32_t i = 0; i < table->entity_count; i++) {
        positions[i].x += velocities[i].x;
        positions[i].y += velocities[i].y;
    }
}
```

`ecs_field()` returns the raw column storage for the current table by `.read`
term index.

## Cache updates

When a new table is created after query initialization, the query index is
updated so future iteration sees matching entities.
