---
title: Performance Guide
description: Practical rules for writing cache-friendly SIECS code.
---

SIECS is optimized around table iteration. The main goal is to keep hot code in
contiguous component arrays and keep structural changes out of tight loops.

## Prefer Persistent Queries

`ecs_query_each()` is convenient, but it creates and destroys a temporary query.
Use persistent queries in repeated work:

```c
ecs_query_id_t moving = ecs_query(world, {
    .terms = { ecs_inout(Position), ecs_in(Velocity) },
});

ecs_iter_t it = ecs_query_iter(world, moving);
while (ecs_iter_next(&it)) {
    Position *p = ecs_field(&it, 0);
    const Velocity *v = ecs_field(&it, 1);
}
```

Systems already own persistent queries internally.

## Resolve Once Per Batch

Call `ecs_field()` once per component per batch, then index arrays in the entity
loop. Resolve resources before the loop:

```c
const Time *time = ecs_get_resource_read(it->world, Time);
Position *p = ecs_field(it, 0);
const Velocity *v = ecs_field(it, 1);

for (uint32_t i = 0; i < it->count; i++) {
    p[i].x += v[i].x * time->dt;
}
```

## Tags Are Cheap In Storage

Tags are zero-sized components. They do not allocate component data columns, but
adding or removing a tag is still a structural change because the entity moves
to a different archetype table.

Use tags freely for stable state such as `Player`, `Selected`, or `Visible`.
Avoid toggling many tags inside a hot iteration loop.

## Avoid Structural Changes In Hot Loops

These operations can migrate an entity to another table:

- `ecs_add()` / `ecs_add_cid()`
- `ecs_remove()` / `ecs_remove_cid()`
- `ecs_set()` when the component was absent
- relation changes such as setting `ChildOf`

Changing fields of existing components does not migrate the entity.

## Keep Query Terms Tight

Use `ecs_filter(T)` when a component is required but not read. Use optional
terms only when one query really benefits from handling both table shapes. Two
smaller queries are sometimes clearer and faster than one query with many
optional branches.

## Module And System Costs

Modules add no work to table iteration. Enable/disable only toggles captured
systems and observers. System ordering is resolved into per-phase execution
plans and rebuilt when systems are enabled or disabled.
