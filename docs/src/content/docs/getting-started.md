---
title: Getting Started
description: Minimal SIECS program using typed components.
---

This page shows the expected flow: initialize a world, register components,
create entities, attach components, write data, then destroy the world.

## Minimal program

```c
#include "ecs/world.h"

ECS_COMPONENT_DECLARE(Position, { float x, y; })
ECS_COMPONENT_DEFINE(Position)

int main(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Position);

    ecs_entity_t entity = ecs_new(world);
    ecs_add(world, entity, Position);

    Position *position = ecs_get(world, entity, Position);
    position->x = 10.0f;
    position->y = 20.0f;

    ecs_fini(world);
    return 0;
}
```

## Lifecycle

1. `ecs_init()` allocates and initializes ECS storage.
2. `ECS_COMPONENT_REGISTER()` assigns runtime component IDs.
3. `ecs_new()` creates entities in the empty table.
4. `ecs_add()` migrates entities to the target archetype table.
5. `ecs_get()` returns component storage for the entity row.
6. `ecs_fini()` releases world-owned storage.

## Build include path

The project Makefile uses `-I.`. Code can include:

```c
#include "ecs/world.h"
```
