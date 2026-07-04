use siecs::{Component, Event, OnAdd, OnRemove, OnSet, World};
use std::sync::atomic::{AtomicI32, Ordering};

#[derive(Component)]
struct EventObserved;

#[derive(Event)]
#[event(name = "EventA")]
struct EventA {
    value: i32,
}

#[derive(Event)]
struct EventB {
    value: i32,
}

static EVENT_A_TOTAL: AtomicI32 = AtomicI32::new(0);
static EVENT_B_TOTAL: AtomicI32 = AtomicI32::new(0);

fn on_event_a(data: &EventA, _observed: &EventObserved) {
    EVENT_A_TOTAL.fetch_add(data.value, Ordering::SeqCst);
}

fn on_event_b(data: &EventB, _observed: &EventObserved) {
    EVENT_B_TOTAL.fetch_add(data.value, Ordering::SeqCst);
}

#[test]
fn builtin_events_have_c_ids() {
    let mut world = World::new();

    assert_eq!(
        world.event::<OnAdd<EventObserved>>().raw(),
        siecs::raw::ECS_ON_ADD
    );
    assert_eq!(
        world.event::<OnRemove<EventObserved>>().raw(),
        siecs::raw::ECS_ON_REMOVE
    );
    assert_eq!(
        world.event::<OnSet<EventObserved>>().raw(),
        siecs::raw::ECS_ON_SET
    );
}

#[test]
fn derived_events_are_typed_and_work_across_worlds() {
    EVENT_A_TOTAL.store(0, Ordering::SeqCst);
    EVENT_B_TOTAL.store(0, Ordering::SeqCst);

    let mut first = World::new();
    let first_a = first.event::<EventA>();
    let first_b = first.event::<EventB>();
    assert_ne!(first_a, first_b);

    let mut second = World::new();
    let second_b = second.event::<EventB>();
    let second_a = second.event::<EventA>();
    assert_eq!(first_a, second_a);
    assert_eq!(first_b, second_b);

    second.observe::<EventA>().each(on_event_a);
    second.observe::<EventB>().each(on_event_b);
    let entity = second.entity();
    second.add::<EventObserved>(entity);

    second.trigger(entity, EventB { value: 7 });
    second.trigger(entity, EventA { value: 5 });

    assert_eq!(EVENT_A_TOTAL.load(Ordering::SeqCst), 5);
    assert_eq!(EVENT_B_TOTAL.load(Ordering::SeqCst), 7);
}
