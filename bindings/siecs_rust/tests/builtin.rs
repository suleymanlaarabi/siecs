use siecs::{Abstract, ChildOf, Component, Disabled, Name, World};

#[derive(Component)]
struct BuiltinPosition {
    value: i32,
}

#[test]
fn disabled_builtin_uses_c_id_and_helpers() {
    let mut world = World::new();
    let entity = world.entity();

    assert!(world.is_enabled(entity));
    assert!(!world.has::<Disabled>(entity));

    world.add::<Disabled>(entity);
    assert!(world.has::<Disabled>(entity));
    assert!(world.is_disabled(entity));

    world.enable(entity);
    assert!(!world.has::<Disabled>(entity));
    assert!(world.is_enabled(entity));

    world.disable(entity);
    assert!(world.has::<Disabled>(entity));
}

#[test]
fn name_and_child_of_have_rust_helpers() {
    let mut world = World::new();
    let parent = world.entity();
    let child = world.entity();

    world.set_name(parent, "Parent");
    world.child_of(child, parent);

    assert_eq!(world.name(parent), Some("Parent"));
    assert_eq!(world.parent(child), Some(parent));
    assert_eq!(world.get::<ChildOf>(child).unwrap().parent(), parent);
}

#[test]
fn builtins_can_mix_with_regular_components() {
    let mut world = World::new();
    let base = world.entity();
    let instance = world.entity();

    world.set(base, BuiltinPosition { value: 12 });
    world.add::<Abstract>(base);
    world.is_a(instance, base);

    assert!(world.has::<Abstract>(base));
    assert!(world.has::<BuiltinPosition>(instance));
    assert_eq!(world.get::<BuiltinPosition>(base).unwrap().value, 12);
    assert_ne!(Name::id(&mut world), 0);
    assert_ne!(ChildOf::id(&mut world), 0);
    assert_ne!(Disabled::id(&mut world), 0);
}
