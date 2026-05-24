---
title: Components
description: Declaring, registering, adding, removing, and reading components.
---

Components describe data columns stored in archetype tables.

## Typed components

Declare components in a header or source file:

```c
ECS_COMPONENT_DECLARE(Position, { float x, y; })
ECS_COMPONENT_DECLARE(Velocity, { float x, y; })
```

Define storage for the generated component IDs in one C file:

```c
ECS_COMPONENT_DEFINE(Position)
ECS_COMPONENT_DEFINE(Velocity)
```

Register each component once per world:

```c
ECS_COMPONENT_REGISTER(world, Position);
ECS_COMPONENT_REGISTER(world, Velocity);
```

## Add and remove

```c
ecs_entity_t e = ecs_new(world);

ecs_add(world, e, Position);
ecs_add(world, e, Velocity);

if (ecs_has(world, e, Position)) {
    Position *p = ecs_get(world, e, Position);
    p->x = 1.0f;
}

ecs_remove(world, e, Velocity);
```

Adding or removing a component migrates the entity between archetype tables.
Existing component data is copied to the new table.

## Dynamic components

When a typed macro is not needed, register by descriptor:

```c
ecs_component_t pos_id = ecs_component(world, {
    .name = "Position",
    .size = sizeof(Position),
});

ecs_add_cid(world, entity, pos_id);
Position *p = ecs_get_cid(world, entity, pos_id);
```

Use typed macros for normal application code. Use component IDs directly for
generic tooling or tests.
