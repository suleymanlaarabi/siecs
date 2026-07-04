use siecs::{raw, Resource, World};
use std::sync::atomic::{AtomicUsize, Ordering};

#[derive(Resource)]
struct Time {
    dt: f32,
}

#[derive(Resource)]
struct Counter {
    value: i32,
}

static RESOURCE_SET: AtomicUsize = AtomicUsize::new(0);
static RESOURCE_REMOVE: AtomicUsize = AtomicUsize::new(0);

unsafe extern "C" fn resource_on_set(_world: *mut raw::WorldRaw, ptr: *const core::ffi::c_void) {
    assert_eq!((*ptr.cast::<HookedResource>()).value, 7);
    RESOURCE_SET.fetch_add(1, Ordering::SeqCst);
}

unsafe extern "C" fn resource_on_remove(_world: *mut raw::WorldRaw, ptr: *const core::ffi::c_void) {
    assert_eq!((*ptr.cast::<HookedResource>()).value, 7);
    RESOURCE_REMOVE.fetch_add(1, Ordering::SeqCst);
}

#[derive(Resource)]
#[resource(name = "HookedResourceName", on_set = resource_on_set, on_remove = resource_on_remove)]
struct HookedResource {
    value: i32,
}

#[test]
fn resource_set_get_mut_try_remove() {
    let mut world = World::new();

    assert!(world.try_resource::<Time>().is_none());
    assert!(!world.has_resource::<Time>());

    world.set_resource(Time { dt: 0.016 });
    assert!(world.has_resource::<Time>());
    assert_eq!(world.resource::<Time>().dt, 0.016);

    world.resource_mut::<Time>().dt = 0.033;
    assert_eq!(world.try_resource::<Time>().unwrap().dt, 0.033);

    world.remove_resource::<Time>();
    assert!(!world.has_resource::<Time>());
}

#[test]
fn resources_are_isolated_per_world() {
    let mut a = World::new();
    let mut b = World::new();

    a.set_resource(Counter { value: 1 });
    b.set_resource(Counter { value: 2 });

    assert_eq!(a.resource::<Counter>().value, 1);
    assert_eq!(b.resource::<Counter>().value, 2);
}

#[test]
fn resource_hooks_fire() {
    RESOURCE_SET.store(0, Ordering::SeqCst);
    RESOURCE_REMOVE.store(0, Ordering::SeqCst);

    let mut world = World::new();
    world.set_resource(HookedResource { value: 7 });
    world.remove_resource::<HookedResource>();

    assert_eq!(RESOURCE_SET.load(Ordering::SeqCst), 1);
    assert_eq!(RESOURCE_REMOVE.load(Ordering::SeqCst), 1);
}
