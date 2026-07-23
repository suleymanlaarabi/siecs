---
title: Rust API
description: Rust bindings for SIECS worlds, components, queries, and systems.
---

The Rust addon lives in `bindings/siecs_rust`. It exposes safe wrappers for common
world, entity, component, query, and system workflows, plus a `raw` module for
direct FFI access.

Install it from crates.io:

```toml
[dependencies]
siecs = "0.1.1"
```

Inside this repository, use the local path crate:

```toml
[dependencies]
siecs = { path = "bindings/siecs_rust" }
```

## Prelude

Most user code should import the prelude:

```rust
use siecs::prelude::*;
```

It exports the common types:

```rust
Commands, Component, Entity, Query, QueryState, Res, ResMut, Resource, With,
Without, World
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

Direct queries are typed over the fields they return:

```rust
let mut query = world.query::<(&mut Position, &Velocity)>();
query.each(|(position, velocity)| {
    position.x += velocity.x;
    position.y += velocity.y;
});
```

Optional fields are expressed with `Option`, and filter-only terms use
`With<T>` or `Without<T>`:

```rust
let mut query = world.query_filtered::<(Entity, &Position), With<Player>>();
query.each(|(entity, position)| {
    println!("{entity:?}: {}, {}", position.x, position.y);
});
```

Use `QueryState` when the same query is reused many times:

```rust
let mut state = world.query_state::<&Position>();
state.each(&mut world, |position| {
    println!("{}, {}", position.x, position.y);
});
```

## Systems

Systems run through `world.progress()`. A system function can request a query:

```rust
fn move_system(mut query: Query<(&mut Position, &Velocity)>) {
    query.each(|(position, velocity)| {
        position.x += velocity.x;
        position.y += velocity.y;
    });
}

world.system("Move", move_system);
world.progress();
```

Systems can also request resources and deferred commands:

```rust
#[derive(Resource)]
struct DeltaTime(f32);

fn move_with_time(mut query: Query<(&mut Position, &Velocity)>, time: Res<DeltaTime>) {
    query.each(|(position, velocity)| {
        position.x += velocity.x * time.0;
        position.y += velocity.y * time.0;
    });
}

world.set_resource(DeltaTime(0.016));
world.system("MoveWithTime", move_with_time);
```

```rust
fn add_velocity(mut query: Query<(Entity, &Position)>, commands: Commands) {
    query.each(|(entity, position)| {
        commands.set(entity, Velocity { x: position.x, y: position.y });
    });
}
```

## Resources

Resources are one typed value stored on the world:

```rust
#[derive(Resource)]
struct Counter {
    value: i32,
}

world.set_resource(Counter { value: 0 });
world.resource_mut::<Counter>().value += 1;
assert_eq!(world.resource::<Counter>().value, 1);
world.remove_resource::<Counter>();
```

## Observers And Events

Observers react to component lifecycle events:

```rust
fn on_set(position: &Position) {
    println!("{}, {}", position.x, position.y);
}

world.on_set::<Position>().each(on_set);
```

Custom events can be derived and triggered:

```rust
#[derive(Event)]
struct Damage {
    amount: i32,
}

#[derive(Component)]
struct Health {
    value: i32,
}

fn on_damage(damage: &Damage, health: &mut Health) {
    health.value -= damage.amount;
}

world.observe::<Damage>().each(on_damage);

let entity = world.entity();
world.set(entity, Health { value: 10 });
world.trigger(entity, Damage { amount: 3 });
```

For event ids that are allocated dynamically, use `alloc_typed_event`,
`observe_typed`, and `trigger_typed`.

## Raw FFI

Use `siecs::raw` only when the safe wrapper does not expose a feature yet. Raw
functions follow the C API exactly, including pointer validity and same-world
requirements.
