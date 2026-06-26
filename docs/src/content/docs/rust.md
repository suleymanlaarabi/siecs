---
title: Rust API
description: Rust bindings for SIECS worlds, components, queries, and systems.
---

The Rust addon lives in `addons/siecs_rust`. It exposes safe wrappers for common
world, entity, component, query, and system workflows, plus a `raw` module for
direct FFI access.

## Prelude

Most user code should import the prelude:

```rust
use siecs::prelude::*;
```

It exports the common types:

```rust
Component, Entity, Phase, Query, System, World
```

## Components

Derive `Component` for Rust component types:

```rust
#[derive(Component)]
struct Position {
    x: f32,
    y: f32,
}
```

Components are registered lazily by the wrapper when needed. Keep component
types plain data and avoid storing references into ECS-owned memory.

## World And Entities

```rust
let mut world = World::new();
let entity = world.entity();

world.set(entity, Position { x: 0.0, y: 0.0 });

let position = world.get::<Position>(entity).unwrap();
assert_eq!(position.x, 0.0);
```

Entity handles belong to the world that created them.

## Queries

The query API mirrors the C/C++ model: build component requirements, then run a
callback over matching components.

```rust
world
    .query()
    .require::<Player>()
    .exclude::<Disabled>()
    .each(|pos: &mut Position, vel: &Velocity| {
        pos.x += vel.x;
        pos.y += vel.y;
    });
```

The binding is designed so query callback plumbing can also be reused for Rust
systems.

## Systems

Systems run through `world.progress()` and use `Phase` for scheduling:

```rust
world
    .system("Move")
    .phase(Phase::OnUpdate)
    .each(|pos: &mut Position, vel: &Velocity| {
        pos.x += vel.x;
    });

world.progress();
```

Rust system callbacks are currently required to be stateless function items or
non-capturing closures. Store shared state in components or C resources through
the raw API until a safe Rust resource wrapper is added.

## Raw FFI

Use `siecs::raw` only when the safe wrapper does not expose a feature yet. Raw
functions follow the C API exactly, including pointer validity and same-world
requirements.
