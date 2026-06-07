---
title: Getting Started
description: Minimal SIECS program using typed components.
---

This page shows the normal user flow:

1. Create a world.
2. Register component types.
3. Create entities.
4. Set or read component data.
5. Destroy the world.

## Minimal Program

```c
#include <siecs.h>

ECS_COMPONENT_DECLARE(Position, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(Position);

int main(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Position);

    ecs_entity_t entity = ecs_new(world);

    ecs_set(world, entity, Position, {
        .x = 10.0f,
        .y = 20.0f,
    });

    Position *position = ecs_get(world, entity, Position);
    position->x += 1.0f;

    ecs_fini(world);
    return 0;
}
```

## Include Path

Applications include the public header:

```c
#include <siecs.h>
```

The old `ecs/world.h` header is not part of the current public API.

## Build With Bake

SIECS is a Bake package. A consuming Bake project should depend on `siecs` and
include `siecs.h` from user code.

For local development in this repository:

```sh
bake rebuild
bake rebuild test
bake run test
```

## Important Contracts

`ecs_get()` assumes the component exists on the entity. Use `ecs_try_get()` when
the component may be absent.

`ecs_set()` adds the component when needed, then writes the value.

Entities passed to API functions must come from the same world.
