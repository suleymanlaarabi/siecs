# SIECS Rust bindings

Rust bindings for [SIECS](https://github.com/suleymanlaarabi/siecs), a C entity
component system.

The crate exposes safe wrappers for the common ECS workflow and keeps a `raw`
module available when a feature is not wrapped yet. The API is experimental and
may change before the first stable release.

## Install

From crates.io:

```toml
[dependencies]
siecs = "0.1.1"
```

From this repository:

```toml
[dependencies]
siecs = { path = "bindings/siecs_rust" }
```

## Quick Start

```rust
use siecs::prelude::*;

#[derive(Component)]
struct Position {
    x: f32,
    y: f32,
}

#[derive(Component)]
struct Velocity {
    x: f32,
    y: f32,
}

fn move_system(mut query: Query<(&mut Position, &Velocity)>) {
    for (position, velocity) in &mut query {
        position.x += velocity.x;
        position.y += velocity.y;
    }
}

fn main() {
    let mut world = World::new();
    let entity = world.entity();

    world.set(entity, Position { x: 0.0, y: 0.0 });
    world.set(entity, Velocity { x: 1.0, y: 2.0 });

    world.system("Move", move_system);
    world.progress();
}
```

## Components And Entities

Derive `Component` for Rust data types and store them on entities:

```rust
use siecs::prelude::*;

#[derive(Component)]
struct Health {
    value: i32,
}

let mut world = World::new();
let entity = world.entity();

world.set(entity, Health { value: 100 });
assert_eq!(world.get::<Health>(entity).unwrap().value, 100);
world.remove::<Health>(entity);
assert!(!world.has::<Health>(entity));
```

## Queries

Queries are typed over the data they return. Use `With<T>` and `Without<T>` for
filter terms that should not be returned as fields:

```rust
use siecs::prelude::*;

#[derive(Component)]
struct Position {
    x: f32,
    y: f32,
}

#[derive(Component)]
struct Velocity {
    x: f32,
    y: f32,
}

#[derive(Component)]
struct Player;

let mut world = World::new();
let mut query = world.query_filtered::<(&mut Position, &Velocity), With<Player>>();
for (position, velocity) in &mut query {
    position.x += velocity.x;
    position.y += velocity.y;
}
```

## Systems

Systems run when the world progresses. System functions can request queries,
resources, and commands:

```rust
use siecs::prelude::*;

#[derive(Component)]
struct Position {
    x: f32,
    y: f32,
}

#[derive(Component)]
struct Velocity {
    x: f32,
    y: f32,
}

#[derive(Resource)]
struct DeltaTime(f32);

fn move_system(mut query: Query<(&mut Position, &Velocity)>, time: Res<DeltaTime>) {
    for (position, velocity) in &mut query {
        position.x += velocity.x * time.0;
        position.y += velocity.y * time.0;
    }
}

let mut world = World::new();
world.set_resource(DeltaTime(0.016));
world.system("Move", move_system);
world.progress();
```

## Observers And Events

Observers react to lifecycle events or custom events:

```rust
use siecs::prelude::*;

#[derive(Component)]
struct Health {
    value: i32,
}

#[derive(Event)]
struct Damage {
    amount: i32,
}

fn on_damage(damage: &Damage, health: &mut Health) {
    health.value -= damage.amount;
}

let mut world = World::new();
world.observe::<Damage>().each(on_damage);

let entity = world.entity();
world.set(entity, Health { value: 10 });
world.trigger(entity, Damage { amount: 3 });
assert_eq!(world.get::<Health>(entity).unwrap().value, 7);
```

## Raw FFI

Use `siecs::raw` only when the safe wrapper does not expose a feature yet. Raw
functions follow the C API and keep the same pointer and same-world safety
requirements.
