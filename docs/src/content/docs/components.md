---
title: Components
description: Declaring, registering, setting, reading, and removing components.
---

Components are data types registered in a world. They can be used through typed
macros or directly through `ecs_component_t` ids.

## Typed Components

Declare a component in a header or source file:

```c
ECS_COMPONENT_DECLARE(Position, {
    float x;
    float y;
});
```

Define the component id storage in exactly one C file:

```c
ECS_COMPONENT_DEFINE(Position);
```

Register the component once per world before using it:

```c
ECS_COMPONENT_REGISTER(world, Position);
```

## Set And Read

`ecs_set()` adds the component if needed, then writes the value:

```c
ecs_set(world, entity, Position, {
    .x = 1.0f,
    .y = 2.0f,
});
```

`ecs_get()` assumes the component exists:

```c
Position *position = ecs_get(world, entity, Position);
position->x += 1.0f;
```

When the component may be absent, use `ecs_try_get()`:

```c
Position *position = ecs_try_get(world, entity, Position);
if (position != NULL) {
    position->x += 1.0f;
}
```

## Add, Remove, Has

Use `ecs_add()` for components that do not need an immediate value, including
tag-like zero-sized components:

```c
ecs_add(world, entity, Position);
```

Check and remove components with:

```c
if (ecs_has(world, entity, Position)) {
    ecs_remove(world, entity, Position);
}
```

Removing a component that is not present is a no-op.

## Id-Based Components

For generic code, register a component from a descriptor:

```c
typedef struct {
    float x;
    float y;
} Position;

ecs_component_t position_id = ecs_component(world, {
    .name = "Position",
    .size = sizeof(Position),
});
```

Then use the `_cid` functions:

```c
ecs_set_cid(world, entity, position_id, &(Position){ .x = 1.0f, .y = 2.0f });

Position *position = ecs_get_cid(world, entity, position_id);
```

## Hooks

Component descriptors can provide lifecycle hooks:

```c
static void on_set_position(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *ptr
) {
    const Position *new_value = ptr;
    (void)world;
    (void)entity;
    (void)component;
    (void)new_value;
}
```

`on_set` receives the new value passed to `ecs_set()` or `ecs_set_cid()`. The
stored value is still the previous value until the hook returns.

`on_remove` receives the value that is about to be removed.
