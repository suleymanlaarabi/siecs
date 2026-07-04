use siecs::{Component, Event, ObserverEvent, Res, ResMut, Resource, World};
use std::sync::atomic::{AtomicI32, AtomicUsize, Ordering};

#[derive(Component)]
struct Observed {
    value: i32,
}

#[derive(Component)]
struct ObservedVelocity {
    value: i32,
}

#[derive(Component)]
struct EnableObserved {
    value: i32,
}

#[derive(Component)]
struct ResourceObserved;

#[derive(Event)]
struct CustomEvent {
    value: i32,
}

#[derive(Resource)]
struct ObserverScale {
    scale: i32,
}

#[derive(Resource)]
struct ObserverStats {
    total: i32,
}

#[derive(Resource)]
struct AddStats {
    total: i32,
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

fn on_set(observed: &Observed) {
    SETS.fetch_add(observed.value, Ordering::SeqCst);
}

fn on_set_enable_test(observed: &EnableObserved) {
    ENABLE_SETS.fetch_add(observed.value, Ordering::SeqCst);
}

fn on_remove(_observed: &Observed) {
    REMOVES.fetch_add(1, Ordering::SeqCst);
}

fn on_custom(
    data: &CustomEvent,
    _observed: &Observed,
    scale: Res<ObserverScale>,
    mut stats: ResMut<ObserverStats>,
) {
    stats.total += data.value * scale.scale;
    CUSTOM.store(stats.total, Ordering::SeqCst);
}

fn on_custom_with_query(
    data: &CustomEvent,
    observed: &Observed,
    velocity: &mut ObservedVelocity,
    scale: Res<ObserverScale>,
    mut stats: ResMut<ObserverStats>,
) {
    velocity.value += observed.value + data.value * scale.scale;
    stats.total += velocity.value;
    CUSTOM.store(stats.total, Ordering::SeqCst);
}

fn on_custom_raw(event: ObserverEvent<'_>) {
    let data = unsafe { event.data_unchecked::<i32>() }.unwrap();
    CUSTOM.store(*data, Ordering::SeqCst);
}

fn on_custom_raw_with_observed(event: ObserverEvent, observed: &Observed) {
    let data = unsafe { event.data_unchecked::<i32>() }.unwrap();
    CUSTOM.store(*data + observed.value, Ordering::SeqCst);
}

fn on_add_with_resource(_observed: &ResourceObserved, mut stats: ResMut<AddStats>) {
    stats.total += 1;
}

fn on_set_with_query_component(observed: &Observed, velocity: &mut ObservedVelocity) {
    velocity.value += observed.value;
}

#[test]
fn observers_handle_lifecycle_and_custom_events() {
    ADDS.store(0, Ordering::SeqCst);
    SETS.store(0, Ordering::SeqCst);
    REMOVES.store(0, Ordering::SeqCst);
    CUSTOM.store(0, Ordering::SeqCst);

    let mut world = World::new();
    world.set_resource(ObserverScale { scale: 2 });
    world.set_resource(ObserverStats { total: 0 });
    world.on_add::<Observed>().each(on_add);
    world.on_set::<Observed>().each(on_set);
    world.on_remove::<Observed>().each(on_remove);
    world.observe::<CustomEvent>().each(on_custom);

    let entity = world.entity();
    world.add::<Observed>(entity);
    world.set(entity, Observed { value: 3 });
    world.trigger(entity, CustomEvent { value: 42 });
    world.remove::<Observed>(entity);

    assert_eq!(ADDS.load(Ordering::SeqCst), 1);
    assert_eq!(SETS.load(Ordering::SeqCst), 3);
    assert_eq!(CUSTOM.load(Ordering::SeqCst), 84);
    assert_eq!(REMOVES.load(Ordering::SeqCst), 1);
}

#[test]
fn observer_enable_disable_controls_callbacks() {
    ENABLE_SETS.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let observer = world.on_set::<EnableObserved>().each(on_set_enable_test);
    let entity = world.entity();
    world.add::<EnableObserved>(entity);

    world.disable_observer(observer);
    world.set(entity, EnableObserved { value: 5 });
    assert_eq!(ENABLE_SETS.load(Ordering::SeqCst), 0);

    world.enable_observer(observer);
    world.set(entity, EnableObserved { value: 7 });
    assert_eq!(ENABLE_SETS.load(Ordering::SeqCst), 7);
}

#[test]
fn raw_event_id_observer_remains_available() {
    CUSTOM.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let event = world.alloc_event();
    world
        .observe_raw(event)
        .require::<Observed>()
        .each(on_custom_raw);

    let entity = world.entity();
    world.add::<Observed>(entity);
    unsafe { world.trigger_raw(entity, event, &5) };

    assert_eq!(CUSTOM.load(Ordering::SeqCst), 5);
}

#[test]
fn raw_observer_can_read_event_metadata_and_query_components() {
    CUSTOM.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let event = world.alloc_event();
    world.observe_raw(event).each(on_custom_raw_with_observed);

    let entity = world.entity();
    world.set(entity, Observed { value: 4 });
    unsafe { world.trigger_raw(entity, event, &5) };

    assert_eq!(CUSTOM.load(Ordering::SeqCst), 9);
}

#[test]
fn observer_callbacks_can_use_resources_without_event_data() {
    let mut world = World::new();
    world.set_resource(AddStats { total: 0 });
    world
        .on_add::<ResourceObserved>()
        .each(on_add_with_resource);

    let entity = world.entity();
    world.add::<ResourceObserved>(entity);

    assert_eq!(world.resource::<AddStats>().total, 1);
}

#[test]
fn observer_callbacks_can_query_components_and_resources() {
    CUSTOM.store(0, Ordering::SeqCst);

    let mut world = World::new();
    world.set_resource(ObserverScale { scale: 2 });
    world.set_resource(ObserverStats { total: 0 });
    world.observe::<CustomEvent>().each(on_custom_with_query);

    let entity = world.entity();
    world.set(entity, Observed { value: 3 });
    world.set(entity, ObservedVelocity { value: 1 });
    world.trigger(entity, CustomEvent { value: 5 });

    assert_eq!(world.get::<ObservedVelocity>(entity).unwrap().value, 14);
    assert_eq!(CUSTOM.load(Ordering::SeqCst), 14);
}

#[test]
fn lifecycle_observer_payload_can_query_additional_components() {
    let mut world = World::new();
    world.on_set::<Observed>().each(on_set_with_query_component);

    let entity = world.entity();
    world.set(entity, ObservedVelocity { value: 2 });
    world.set(entity, Observed { value: 5 });

    assert_eq!(world.get::<ObservedVelocity>(entity).unwrap().value, 7);
}

#[test]
#[should_panic(
    expected = "callback cannot request mutable and immutable access to the same resource"
)]
fn observer_rejects_duplicate_resource_access() {
    let mut world = World::new();
    world.set_resource(ObserverScale { scale: 1 });

    world
        .observe::<CustomEvent>()
        .each(|_event: &CustomEvent, _read: Res<ObserverScale>, _write: ResMut<ObserverScale>| {});
}
