---
title: Bitset Components
description: Zero-sized boolean tags stored as packed bits.
---

Bitset components are boolean tags. They use component IDs like normal
components, but store values as packed bits instead of structs.

## Define and register

```c
ECS_BIT_DEFINE(IsActive)
ECS_BIT_DEFINE(IsVisible)

ECS_COMPONENT_REGISTER(world, IsActive);
ECS_COMPONENT_REGISTER(world, IsVisible);
```

## Add and set values

```c
ecs_entity_t e = ecs_new(world);

ecs_add(world, e, IsActive);
ecs_set_bit(world, e, ecs_id(IsActive), true);

bool active = ecs_get_bit(world, e, ecs_id(IsActive));
```

## Migration behavior

Bit values are copied during archetype migration. Removing another component or
killing another entity does not corrupt the remaining bit values.

Use bitset components for flags such as `IsActive`, `IsVisible`, `Selected`, or
`Dirty`.
