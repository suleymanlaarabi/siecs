use siecs::{Component, ObserverEvent, OnAdd, OnRemove, OnSet, World};
use std::sync::atomic::{AtomicI32, AtomicUsize, Ordering};

#[derive(Component)]
struct Observed {
    value: i32,
}

#[derive(Component)]
struct EnableObserved {
    value: i32,
}

static ADDS: AtomicUsize = AtomicUsize::new(0);
static SETS: AtomicI32 = AtomicI32::new(0);
static ENABLE_SETS: AtomicI32 = AtomicI32::new(0);
static REMOVES: AtomicUsize = AtomicUsize::new(0);
static CUSTOM: AtomicI32 = AtomicI32::new(0);

fn on_add(_event: ObserverEvent<'_>, observed: &Observed) {
    assert_eq!(observed.value, 0);
    ADDS.fetch_add(1, Ordering::SeqCst);
}

fn on_set(event: ObserverEvent<'_>, _observed: &Observed) {
    SETS.fetch_add(event.data::<Observed>().unwrap().value, Ordering::SeqCst);
}

fn on_set_enable_test(event: ObserverEvent<'_>, _observed: &EnableObserved) {
    ENABLE_SETS.fetch_add(
        event.data::<EnableObserved>().unwrap().value,
        Ordering::SeqCst,
    );
}

fn on_remove(_observed: &Observed) {
    REMOVES.fetch_add(1, Ordering::SeqCst);
}

fn on_custom(event: ObserverEvent<'_>, _observed: &Observed) {
    CUSTOM.store(*event.data::<i32>().unwrap(), Ordering::SeqCst);
}

#[test]
fn observers_handle_lifecycle_and_custom_events() {
    ADDS.store(0, Ordering::SeqCst);
    SETS.store(0, Ordering::SeqCst);
    REMOVES.store(0, Ordering::SeqCst);
    CUSTOM.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let custom = world.event();
    world.observer(OnAdd).each(on_add);
    world.observer(OnSet).each(on_set);
    world.observer(OnRemove).each(on_remove);
    world.observer(custom).each(on_custom);

    let entity = world.entity();
    world.add::<Observed>(entity);
    world.set(entity, Observed { value: 3 });
    world.trigger(entity, custom, &42);
    world.remove::<Observed>(entity);

    assert_eq!(ADDS.load(Ordering::SeqCst), 1);
    assert_eq!(SETS.load(Ordering::SeqCst), 3);
    assert_eq!(CUSTOM.load(Ordering::SeqCst), 42);
    assert_eq!(REMOVES.load(Ordering::SeqCst), 1);
}

#[test]
fn observer_enable_disable_controls_callbacks() {
    ENABLE_SETS.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let observer = world.observer(OnSet).each(on_set_enable_test);
    let entity = world.entity();
    world.add::<EnableObserved>(entity);

    world.disable_observer(observer);
    world.set(entity, EnableObserved { value: 5 });
    assert_eq!(ENABLE_SETS.load(Ordering::SeqCst), 0);

    world.enable_observer(observer);
    world.set(entity, EnableObserved { value: 7 });
    assert_eq!(ENABLE_SETS.load(Ordering::SeqCst), 7);
}
