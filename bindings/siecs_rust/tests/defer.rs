use siecs::{Component, EachCtx, World};

#[derive(Component)]
struct Position {
    value: i32,
}

#[derive(Component)]
struct Marker;

fn mark_with_ctx(ctx: EachCtx<'_>, position: &mut Position) {
    position.value += 1;
    ctx.add::<Marker>();
}

#[test]
fn defer_begin_end_and_nested_flush() {
    let mut world = World::new();
    let entity = world.entity();

    world.defer_begin();
    world.defer_begin();
    world.add::<Marker>(entity);
    assert!(world.is_deferred());
    assert!(!world.has::<Marker>(entity));

    world.defer_end();
    assert!(world.is_deferred());
    assert!(!world.has::<Marker>(entity));

    world.defer_end();
    assert!(!world.is_deferred());
    assert!(world.has::<Marker>(entity));
}

#[test]
fn defer_guard_flushes_on_drop() {
    let mut world = World::new();
    let entity = world.entity();

    {
        let mut guard = world.defer();
        guard.add::<Marker>(entity);
        assert!(guard.is_deferred());
        assert!(!guard.has::<Marker>(entity));
    }

    assert!(!world.is_deferred());
    assert!(world.has::<Marker>(entity));
}

#[test]
fn system_each_ctx_can_mutate_entity() {
    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, Position { value: 1 });

    world.system("Ctx").each(mark_with_ctx);
    world.progress();

    assert_eq!(world.get::<Position>(entity).unwrap().value, 2);
    assert!(world.has::<Marker>(entity));
}
