use siecs::{Component, World};

#[derive(Component)]
struct Transform;

#[derive(Component)]
struct Renderable;

#[test]
fn world_with_adds_required_component() {
    let mut world = World::new();
    world.with::<Renderable, Transform>();

    let entity = world.entity();
    world.add::<Renderable>(entity);

    assert!(world.has::<Renderable>(entity));
    assert!(world.has::<Transform>(entity));
}
