---
title: C++ API
description: Type-safe C++23 wrapper around the SIECS C runtime.
---

The C++ addon is a thin C++23 layer over the C runtime. Include:

```cpp
#include <siecs_cpp/siecs_cpp.hpp>
```

use `siecs_cpp` with Bake.

## World And Entities

```cpp
ecs::world world;

struct Position {
    float x, y;
};

ecs::entity e = world.entity();
e.set(Position{ .x = 0, .y = 0 });
```

The C++ world owns the C `ecs_world_t` by default. Use `ecs::world::borrow(raw)`
when wrapping a world owned elsewhere.

## Queries

The query builder infers component access from callback parameter types:

```cpp
world.query()
    .require<Player>()
    .exclude<Disabled>()
    .each([](Position &pos, const Velocity &vel) {
        pos.x += vel.x;
        pos.y += vel.y;
    });
```

Use mutable references for read/write terms and `const` references for read
terms. Filter-only requirements are added with `.require<T>()`.

## Systems

Systems use the same callback style and run through `world.progress()`:

```cpp
world.system("Move")
    .phase(EcsOnUpdate)
    .each([](Position &pos, const Velocity &vel) {
        pos.x += vel.x;
    });

world.progress();
```

## Resources

Resources are world-level values:

```cpp
struct Time {
    float dt;
};

world.set_resource(Time{ .dt = 0.016f });

world.system("Move").each([](
    ecs::res<const Time> time,
    Position &pos,
    const Velocity &vel
) {
    pos.x += vel.x * time->dt;
});
```

Resource parameters do not become query terms and do not consume field indexes.

## Modules And Observers

The C++ addon wraps C modules and observers while still using the same runtime
ids. Use C docs for lifecycle semantics: modules capture registrations during
import, observers match event plus query, and disabled entities are skipped
unless `Disabled` is mentioned explicitly.
