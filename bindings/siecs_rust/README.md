# SIECS Rust bindings

Rust bindings for [SIECS](https://github.com/suleymanlaarabi/siecs), a C entity
component system.

```rust
use siecs::prelude::*;

#[derive(Component)]
struct Position {
    x: f32,
    y: f32,
}

fn main() {
    let mut world = World::new();
    let entity = world.entity();

    world.set(entity, Position { x: 1.0, y: 2.0 });
}
```
