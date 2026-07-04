use siecs::{raw, Component, Resource, World};
use std::sync::atomic::{AtomicUsize, Ordering};

static COMPONENT_SET: AtomicUsize = AtomicUsize::new(0);

unsafe extern "C" fn component_on_set(
    _world: *mut raw::WorldRaw,
    _entity: raw::EntityId,
    _component: raw::ComponentId,
    new_value: *const core::ffi::c_void,
    _current_value: *mut core::ffi::c_void,
) {
    assert_eq!((*new_value.cast::<HookedComponent>()).value, 10);
    COMPONENT_SET.fetch_add(1, Ordering::SeqCst);
}

#[derive(Component)]
#[component(name = "CustomComponentName", on_set = component_on_set)]
struct HookedComponent {
    value: i32,
}

#[derive(Component)]
#[component(relation(cascade_delete, one_to_many))]
struct RelationComponent {
    target: raw::EntityId,
}

#[derive(Resource)]
#[resource(name = "CustomResourceName")]
struct NamedResource {
    value: i32,
}

#[test]
fn derive_component_attrs_and_resource_name_compile_and_run() {
    COMPONENT_SET.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, HookedComponent { value: 10 });
    assert_eq!(COMPONENT_SET.load(Ordering::SeqCst), 1);

    world.set_resource(NamedResource { value: 4 });
    assert_eq!(world.resource::<NamedResource>().value, 4);
}

#[test]
fn derive_relation_flags_compile_and_register() {
    let mut world = World::new();
    let entity = world.entity();
    let target = world.entity();

    world.set(
        entity,
        RelationComponent {
            target: target.id(),
        },
    );

    assert!(world.has::<RelationComponent>(entity));
    assert_eq!(world.get::<RelationComponent>(entity).unwrap().target, target.id());
}
