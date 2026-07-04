use siecs::{Abstract, Component, Event, ObserverEvent, ParamError, Res, ResMut, Resource, World};
use std::cell::Cell;
use std::rc::Rc;
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

#[derive(Component)]
struct InheritedObserved {
    value: i32,
}

#[derive(Component)]
struct OptionalObserved {
    value: i32,
}

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
static INHERITED_TOTAL: AtomicI32 = AtomicI32::new(0);
static OPTIONAL_REF_TOTAL: AtomicI32 = AtomicI32::new(0);
static OPTIONAL_MUT_TOTAL: AtomicI32 = AtomicI32::new(0);
static OPTIONAL_RESOURCE_TOTAL: AtomicI32 = AtomicI32::new(0);
static OPTIONAL_RESOURCE_MUT_TOTAL: AtomicI32 = AtomicI32::new(0);
static TYPED_EVENT_TOTAL: AtomicI32 = AtomicI32::new(0);

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

fn on_inherited_read(data: &CustomEvent, inherited: &InheritedObserved) {
    INHERITED_TOTAL.store(data.value + inherited.value, Ordering::SeqCst);
}

fn on_optional_read(_data: &CustomEvent, optional: Option<&OptionalObserved>) {
    OPTIONAL_REF_TOTAL.fetch_add(optional.map_or(1, |value| value.value), Ordering::SeqCst);
}

fn on_optional_mut(_data: &CustomEvent, optional: Option<&mut OptionalObserved>) {
    if let Some(optional) = optional {
        optional.value += 10;
        OPTIONAL_MUT_TOTAL.fetch_add(optional.value, Ordering::SeqCst);
    } else {
        OPTIONAL_MUT_TOTAL.fetch_add(1, Ordering::SeqCst);
    }
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

#[test]
fn observer_refs_abstract_inherited_components_without_field_kind() {
    INHERITED_TOTAL.store(0, Ordering::SeqCst);

    let mut world = World::new();
    world.observe::<CustomEvent>().each(on_inherited_read);

    let base = world.entity();
    world.set(base, InheritedObserved { value: 7 });
    world.add::<Abstract>(base);

    let entity = world.entity();
    world.is_a(entity, base);
    world.trigger(entity, CustomEvent { value: 5 });

    assert_eq!(INHERITED_TOTAL.load(Ordering::SeqCst), 12);
}

#[test]
fn observer_optional_refs_handle_absent_owned_and_shared() {
    OPTIONAL_REF_TOTAL.store(0, Ordering::SeqCst);

    let mut world = World::new();
    world.observe::<CustomEvent>().each(on_optional_read);

    let absent = world.entity();
    world.trigger(absent, CustomEvent { value: 0 });

    let owned = world.entity();
    world.set(owned, OptionalObserved { value: 5 });
    world.trigger(owned, CustomEvent { value: 0 });

    let base = world.entity();
    world.set(base, OptionalObserved { value: 9 });
    world.add::<Abstract>(base);
    let shared = world.entity();
    world.is_a(shared, base);
    world.trigger(shared, CustomEvent { value: 0 });

    assert_eq!(OPTIONAL_REF_TOTAL.load(Ordering::SeqCst), 15);
}

#[test]
fn observer_optional_mut_ignores_absent_and_shared_components() {
    OPTIONAL_MUT_TOTAL.store(0, Ordering::SeqCst);

    let mut world = World::new();
    world.observe::<CustomEvent>().each(on_optional_mut);

    let absent = world.entity();
    world.trigger(absent, CustomEvent { value: 0 });

    let owned = world.entity();
    world.set(owned, OptionalObserved { value: 5 });
    world.trigger(owned, CustomEvent { value: 0 });

    let base = world.entity();
    world.set(base, OptionalObserved { value: 9 });
    world.add::<Abstract>(base);
    let shared = world.entity();
    world.is_a(shared, base);
    world.trigger(shared, CustomEvent { value: 0 });

    assert_eq!(OPTIONAL_MUT_TOTAL.load(Ordering::SeqCst), 17);
    assert_eq!(world.get::<OptionalObserved>(owned).unwrap().value, 15);
    assert_eq!(world.get::<OptionalObserved>(base).unwrap().value, 9);
}

#[test]
fn observer_try_each_reports_missing_required_resource() {
    let mut world = World::new();

    let result = world
        .observe::<CustomEvent>()
        .try_each(|_event: &CustomEvent, _scale: Res<ObserverScale>| {});

    assert_eq!(result, Err(ParamError::MissingRequiredResource));
}

#[test]
#[should_panic(expected = "callback requested a missing required resource")]
fn observer_each_panics_on_missing_required_resource() {
    let mut world = World::new();

    world
        .observe::<CustomEvent>()
        .each(|_event: &CustomEvent, _scale: Res<ObserverScale>| {});
}

#[test]
fn observer_optional_resources_handle_absent_and_present() {
    OPTIONAL_RESOURCE_TOTAL.store(0, Ordering::SeqCst);

    let mut world = World::new();
    world.observe::<CustomEvent>().each(
        |_event: &CustomEvent, scale: Option<Res<ObserverScale>>| {
            OPTIONAL_RESOURCE_TOTAL
                .fetch_add(scale.map_or(1, |scale| scale.scale), Ordering::SeqCst);
        },
    );

    let entity = world.entity();
    world.trigger(entity, CustomEvent { value: 0 });
    world.set_resource(ObserverScale { scale: 4 });
    world.trigger(entity, CustomEvent { value: 0 });

    assert_eq!(OPTIONAL_RESOURCE_TOTAL.load(Ordering::SeqCst), 5);
}

#[test]
fn observer_optional_mut_resources_handle_absent_and_present() {
    OPTIONAL_RESOURCE_MUT_TOTAL.store(0, Ordering::SeqCst);

    let mut world = World::new();
    world.observe::<CustomEvent>().each(
        |_event: &CustomEvent, stats: Option<ResMut<ObserverStats>>| {
            if let Some(mut stats) = stats {
                stats.total += 4;
                OPTIONAL_RESOURCE_MUT_TOTAL.fetch_add(stats.total, Ordering::SeqCst);
            } else {
                OPTIONAL_RESOURCE_MUT_TOTAL.fetch_add(1, Ordering::SeqCst);
            }
        },
    );

    let entity = world.entity();
    world.trigger(entity, CustomEvent { value: 0 });
    world.set_resource(ObserverStats { total: 2 });
    world.trigger(entity, CustomEvent { value: 0 });

    assert_eq!(OPTIONAL_RESOURCE_MUT_TOTAL.load(Ordering::SeqCst), 7);
    assert_eq!(world.resource::<ObserverStats>().total, 6);
}

#[test]
fn observer_each_boxed_accepts_captures_and_keeps_state() {
    let mut world = World::new();
    let total = Rc::new(Cell::new(0));
    let seen = Rc::clone(&total);

    world
        .observe::<CustomEvent>()
        .each_boxed(move |event: &CustomEvent| {
            seen.set(seen.get() + event.value);
        });

    let entity = world.entity();
    world.trigger(entity, CustomEvent { value: 2 });
    world.trigger(entity, CustomEvent { value: 3 });

    assert_eq!(total.get(), 5);
}

#[test]
fn typed_event_handle_observes_and_triggers_payload() {
    TYPED_EVENT_TOTAL.store(0, Ordering::SeqCst);

    #[derive(Clone, Copy)]
    struct Damage {
        amount: i32,
    }

    let mut world = World::new();
    let damage = world.alloc_typed_event::<Damage>();
    world.observe_typed(damage).each(|event: &Damage| {
        TYPED_EVENT_TOTAL.store(event.amount, Ordering::SeqCst);
    });

    let entity = world.entity();
    world.trigger_typed(entity, damage, &Damage { amount: 11 });

    assert_eq!(TYPED_EVENT_TOTAL.load(Ordering::SeqCst), 11);
}
