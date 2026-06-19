use siecs::{raw, Component, World};
use std::mem::{align_of, offset_of, size_of};

#[derive(Component)]
struct AddPosition {
    x: f32,
    y: f32,
}

#[derive(Component)]
struct SetPosition {
    x: f32,
    y: f32,
}

#[derive(Component)]
struct QueryPosition {
    x: f32,
    y: f32,
}

#[test]
fn raw_query_layout_matches_c() {
    assert_eq!(size_of::<raw::TermAccess>(), 4);
    assert_eq!(align_of::<raw::TermAccess>(), 4);

    assert_eq!(size_of::<raw::QueryTerm>(), 8);
    assert_eq!(align_of::<raw::QueryTerm>(), 4);
    assert_eq!(offset_of!(raw::QueryTerm, id), 0);
    assert_eq!(offset_of!(raw::QueryTerm, access), 4);

    assert_eq!(size_of::<raw::QueryDesc>(), 128);
    assert_eq!(align_of::<raw::QueryDesc>(), 4);
    assert_eq!(offset_of!(raw::QueryDesc, terms), 0);

    assert_eq!(size_of::<raw::Iter>(), 48);
    assert_eq!(align_of::<raw::Iter>(), 8);
    assert_eq!(offset_of!(raw::Iter, world), 0);
    assert_eq!(offset_of!(raw::Iter, count), 8);
    assert_eq!(offset_of!(raw::Iter, entities), 16);
    assert_eq!(offset_of!(raw::Iter, cache), 24);
    assert_eq!(offset_of!(raw::Iter, ptrs), 32);
    assert_eq!(offset_of!(raw::Iter, table_idx), 40);
    assert_eq!(offset_of!(raw::Iter, table_count), 42);
}

#[test]
fn raw_create_world_and_entity() {
    unsafe {
        let world = raw::ecs_init();
        assert!(!world.is_null());

        let entity = raw::ecs_new(world);
        assert_ne!(entity, 0);
        assert_ne!(raw::ecs_is_alive(world, entity), 0);

        raw::ecs_fini(world);
    }
}

#[test]
fn world_creates_entity() {
    let mut world = World::new();
    let entity = world.entity();

    assert_ne!(entity.id(), 0);
    assert!(world.is_alive(entity));
}

#[test]
fn world_kills_entity() {
    let mut world = World::new();
    let entity = world.entity();

    world.kill(entity);

    assert!(!world.is_alive(entity));
}

#[test]
fn derived_component_add_remove() {
    let mut world = World::new();
    let entity = world.entity();

    world.add::<AddPosition>(entity);

    assert!(world.has::<AddPosition>(entity));
    let position = world.get::<AddPosition>(entity).unwrap();
    assert_eq!(position.x, 0.0);
    assert_eq!(position.y, 0.0);

    world.remove::<AddPosition>(entity);

    assert!(!world.has::<AddPosition>(entity));
    assert!(world.get::<AddPosition>(entity).is_none());
}

#[test]
fn derived_component_set_get_mut() {
    let mut world = World::new();
    let entity = world.entity();

    world.set(entity, SetPosition { x: 1.0, y: 2.0 });

    let position = world.get::<SetPosition>(entity).unwrap();
    assert_eq!(position.x, 1.0);
    assert_eq!(position.y, 2.0);

    let position = world.get_mut::<SetPosition>(entity).unwrap();
    position.x = 4.0;

    assert_eq!(world.get::<SetPosition>(entity).unwrap().x, 4.0);
}

#[test]
fn raw_query_iter_reads_component_field() {
    let mut world = World::new();
    let entity = world.entity();

    world.set(entity, QueryPosition { x: 3.0, y: 7.0 });

    let component = QueryPosition::id(&mut world);
    let mut desc = raw::QueryDesc::default();
    desc.terms[0] = raw::QueryTerm {
        id: component,
        access: raw::TermAccess::In,
    };

    let query = unsafe { raw::ecs_query_init(world.as_raw_mut(), &desc) as raw::QueryId };
    let mut iter = unsafe { raw::ecs_query_iter(world.as_raw_mut(), query) };

    assert!(unsafe { raw::ecs_iter_next(&mut iter) });
    assert_eq!(iter.count, 1);
    assert_eq!(unsafe { *iter.entities }, entity.id());

    let positions = unsafe { raw::ecs_field(&mut iter, 0).cast::<QueryPosition>() };
    assert!(!positions.is_null());

    let position = unsafe { &*positions };
    assert_eq!(position.x, 3.0);
    assert_eq!(position.y, 7.0);
    assert!(!unsafe { raw::ecs_iter_next(&mut iter) });

    unsafe {
        raw::ecs_query_fini(world.as_raw_mut(), query);
    }
}
