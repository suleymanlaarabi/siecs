use siecs::{
    raw, Abstract, Component, EachCtx, Entity, Field, FieldKind, Phase, Res, ResMut, Resource,
    World,
};
use std::mem::{align_of, offset_of, size_of};
use std::sync::atomic::{AtomicI32, AtomicUsize, Ordering};

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

#[derive(Component)]
struct QueryVelocity {
    x: f32,
    y: f32,
}

#[derive(Component)]
struct QueryPlayer;

#[derive(Component)]
struct QueryDisabled;

#[derive(Component)]
struct MultiWorldPosition {
    value: i32,
}

#[derive(Component)]
struct SystemPosition {
    x: i32,
}

#[derive(Component)]
struct SystemVelocity {
    x: i32,
}

#[derive(Component)]
struct SystemPlayer;

#[derive(Component)]
struct SystemDisabled;

#[derive(Component)]
struct InheritPosition {
    value: i32,
}

#[derive(Component)]
struct InheritVelocity {
    value: i32,
}

#[derive(Component)]
struct InheritPlayer;

#[derive(Resource)]
struct QueryScale {
    value: i32,
}

#[derive(Resource)]
struct DeltaTime {
    ticks: i32,
}

static MOVE_CALLS: AtomicUsize = AtomicUsize::new(0);
static FILTER_CALLS: AtomicUsize = AtomicUsize::new(0);
static ENABLE_CALLS: AtomicUsize = AtomicUsize::new(0);
static MULTI_WORLD_CALLS: AtomicUsize = AtomicUsize::new(0);
static SYSTEM_ORDER: AtomicI32 = AtomicI32::new(0);
static SYSTEM_AFTER_ORDER: AtomicI32 = AtomicI32::new(0);

fn move_system(position: &mut SystemPosition, velocity: &SystemVelocity) {
    position.x += velocity.x;
    MOVE_CALLS.fetch_add(1, Ordering::SeqCst);
}

fn filter_count_system(position: &SystemPosition) {
    FILTER_CALLS.fetch_add(position.x as usize, Ordering::SeqCst);
}

fn enable_count_system(position: &SystemPosition) {
    ENABLE_CALLS.fetch_add(position.x as usize, Ordering::SeqCst);
}

fn multi_world_move_system(position: &mut SystemPosition, velocity: &SystemVelocity) {
    position.x += velocity.x;
    MULTI_WORLD_CALLS.fetch_add(1, Ordering::SeqCst);
}

fn pre_update_system(_position: &SystemPosition) {
    assert_eq!(SYSTEM_ORDER.swap(1, Ordering::SeqCst), 0);
}

fn on_update_system(_position: &SystemPosition) {
    assert_eq!(SYSTEM_ORDER.swap(2, Ordering::SeqCst), 1);
}

fn after_first_system(_position: &SystemPosition) {
    assert_eq!(SYSTEM_AFTER_ORDER.swap(1, Ordering::SeqCst), 0);
}

fn after_second_system(_position: &SystemPosition) {
    assert_eq!(SYSTEM_AFTER_ORDER.swap(2, Ordering::SeqCst), 1);
}

fn resource_move_system(
    ctx: EachCtx,
    position: &mut SystemPosition,
    velocity: &SystemVelocity,
    dt: Res<DeltaTime>,
) {
    assert_ne!(ctx.entity().id(), 0);
    position.x += velocity.x * dt.ticks;
}

#[test]
fn raw_query_layout_matches_c() {
    assert_eq!(size_of::<raw::TermAccess>(), 4);
    assert_eq!(align_of::<raw::TermAccess>(), 4);

    assert_eq!(size_of::<raw::QueryTerm>(), 8);
    assert_eq!(align_of::<raw::QueryTerm>(), 4);
    assert_eq!(offset_of!(raw::QueryTerm, id), 0);
    assert_eq!(offset_of!(raw::QueryTerm, access), 4);

    assert_eq!(size_of::<raw::QueryDesc>(), 136);
    assert_eq!(align_of::<raw::QueryDesc>(), 8);
    assert_eq!(offset_of!(raw::QueryDesc, terms), 0);
    assert_eq!(offset_of!(raw::QueryDesc, is_a), 128);

    assert_eq!(size_of::<raw::FieldKind>(), 4);
    assert_eq!(align_of::<raw::FieldKind>(), 4);

    assert_eq!(size_of::<raw::Iter>(), 56);
    assert_eq!(align_of::<raw::Iter>(), 8);
    assert_eq!(offset_of!(raw::Iter, world), 0);
    assert_eq!(offset_of!(raw::Iter, count), 8);
    assert_eq!(offset_of!(raw::Iter, entities), 16);
    assert_eq!(offset_of!(raw::Iter, cache), 24);
    assert_eq!(offset_of!(raw::Iter, ptrs), 32);
    assert_eq!(offset_of!(raw::Iter, field_kinds), 40);
    assert_eq!(offset_of!(raw::Iter, table_idx), 48);
    assert_eq!(offset_of!(raw::Iter, table_count), 50);
}

#[test]
fn raw_system_layout_matches_c() {
    assert_eq!(size_of::<raw::Phase>(), 4);
    assert_eq!(align_of::<raw::Phase>(), 4);

    assert_eq!(size_of::<raw::SystemDesc>(), 168);
    assert_eq!(align_of::<raw::SystemDesc>(), 8);
    assert_eq!(offset_of!(raw::SystemDesc, name), 0);
    assert_eq!(offset_of!(raw::SystemDesc, query), 8);
    assert_eq!(offset_of!(raw::SystemDesc, callback), 144);
    assert_eq!(offset_of!(raw::SystemDesc, phase), 152);
    assert_eq!(offset_of!(raw::SystemDesc, after), 156);
    assert_eq!(offset_of!(raw::SystemDesc, disabled), 164);
}

#[test]
fn raw_create_world_and_entity() {
    unsafe {
        let world = raw::ecs_init();
        assert!(!world.is_null());

        let entity = raw::ecs_new(world);
        assert_ne!(entity, 0);
        assert!(raw::ecs_is_alive(world, entity));

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
fn world_inherits_component_from_abstract_base() {
    let mut world = World::new();
    let base = world.entity();
    world.set(base, InheritPosition { value: 42 });
    world.add::<Abstract>(base);

    let entity = world.entity();
    world.is_a(entity, base);

    assert!(world.is(entity, base));
    assert!(world.has::<InheritPosition>(entity));
    assert!(!world.has_owned::<InheritPosition>(entity));
    assert!(world.get::<InheritPosition>(entity).is_none());
    assert!(world.get_mut::<InheritPosition>(entity).is_none());

    world.set(entity, InheritPosition { value: 100 });
    assert!(world.has_owned::<InheritPosition>(entity));
    assert_eq!(world.get_mut::<InheritPosition>(entity).unwrap().value, 100);
    assert_eq!(world.get::<InheritPosition>(base).unwrap().value, 42);
}

#[test]
fn query_is_a_matches_transitive_base() {
    let mut world = World::new();
    let character = world.entity();
    world.add::<Abstract>(character);

    let player = world.entity();
    world.is_a(player, character);
    world.add::<Abstract>(player);

    let knight = world.entity();
    world.is_a(knight, player);
    world.set(knight, InheritVelocity { value: 7 });

    let mut seen = 0;
    world
        .query()
        .is_a(character)
        .each(|velocity: &InheritVelocity| {
            assert_eq!(velocity.value, 7);
            seen += 1;
        });

    assert_eq!(seen, 1);
}

#[test]
fn query_field_reports_shared_for_inherited_component() {
    let mut world = World::new();
    let base = world.entity();
    world.set(base, InheritPosition { value: 42 });
    world.add::<Abstract>(base);

    let entity = world.entity();
    world.is_a(entity, base);

    let mut seen = 0;
    world
        .query()
        .is_a(base)
        .each(|position: Field<'_, InheritPosition>| {
            assert_eq!(position.kind(), FieldKind::Shared);
            assert!(position.is_shared());
            assert!(!position.is_owned());
            assert_eq!(position.value, 42);
            seen += 1;
        });

    assert_eq!(seen, 1);
}

#[test]
fn query_inout_does_not_match_inherited_shared_component() {
    let mut world = World::new();
    let base = world.entity();
    world.set(base, InheritPosition { value: 42 });
    world.add::<Abstract>(base);

    let entity = world.entity();
    world.is_a(entity, base);

    let mut seen = 0;
    world
        .query()
        .is_a(base)
        .each(|position: &mut InheritPosition| {
            position.value += 1;
            seen += 1;
        });

    assert_eq!(seen, 0);
    assert_eq!(world.get::<InheritPosition>(base).unwrap().value, 42);
}

#[test]
fn query_optional_reads_absent_owned_and_inherited_components() {
    let mut world = World::new();
    let base = world.entity();
    world.set(base, InheritPosition { value: 10 });
    world.add::<Abstract>(base);

    let absent = world.entity();
    world.add::<InheritPlayer>(absent);

    let owned = world.entity();
    world.add::<InheritPlayer>(owned);
    world.set(owned, InheritPosition { value: 20 });

    let inherited = world.entity();
    world.add::<InheritPlayer>(inherited);
    world.is_a(inherited, base);

    let mut values = Vec::new();
    world
        .query()
        .require::<InheritPlayer>()
        .each(|position: Option<&InheritPosition>| {
            values.push(position.map(|position| position.value));
        });
    values.sort();

    assert_eq!(values, [None, Some(10), Some(20)]);
}

#[test]
fn query_optional_mut_ignores_inherited_shared_component() {
    let mut world = World::new();
    let base = world.entity();
    world.set(base, InheritPosition { value: 10 });
    world.add::<Abstract>(base);

    let owned = world.entity();
    world.add::<InheritPlayer>(owned);
    world.set(owned, InheritPosition { value: 20 });

    let inherited = world.entity();
    world.add::<InheritPlayer>(inherited);
    world.is_a(inherited, base);

    let mut seen_none = 0;
    let mut seen_some = 0;
    world
        .query()
        .require::<InheritPlayer>()
        .each(|position: Option<&mut InheritPosition>| {
            if let Some(position) = position {
                position.value += 1;
                seen_some += 1;
            } else {
                seen_none += 1;
            }
        });

    assert_eq!(seen_some, 1);
    assert_eq!(seen_none, 1);
    assert_eq!(world.get::<InheritPosition>(owned).unwrap().value, 21);
    assert_eq!(world.get::<InheritPosition>(base).unwrap().value, 10);
}

#[test]
fn query_optional_field_reports_owned_shared_and_none() {
    let mut world = World::new();
    let base = world.entity();
    world.set(base, InheritPosition { value: 10 });
    world.add::<Abstract>(base);

    let absent = world.entity();
    world.add::<InheritPlayer>(absent);

    let owned = world.entity();
    world.add::<InheritPlayer>(owned);
    world.set(owned, InheritPosition { value: 20 });

    let inherited = world.entity();
    world.add::<InheritPlayer>(inherited);
    world.is_a(inherited, base);

    let mut values = Vec::new();
    world.query().require::<InheritPlayer>().each(
        |position: Option<Field<'_, InheritPosition>>| {
            values.push(position.map(|position| (position.kind(), position.value)));
        },
    );
    values.sort_by_key(|value| value.map(|(_, position)| position));

    assert_eq!(
        values,
        [
            None,
            Some((FieldKind::Shared, 10)),
            Some((FieldKind::Owned, 20)),
        ]
    );
}

#[test]
fn query_each_accepts_entity_fields_and_resources_without_explicit_lifetimes() {
    let mut world = World::new();
    world.set_resource(QueryScale { value: 3 });

    let first = world.entity();
    world.set(first, QueryPosition { x: 2.0, y: 0.0 });
    world.set(first, QueryVelocity { x: 5.0, y: 0.0 });

    let second = world.entity();
    world.set(second, QueryPosition { x: 7.0, y: 0.0 });

    let mut rows = Vec::new();
    world.query().each(
        |entity: Entity,
         position: Field<QueryPosition>,
         velocity: Option<Field<QueryVelocity>>,
         scale: Res<QueryScale>| {
            rows.push((
                entity,
                position.kind(),
                (position.x as i32) * scale.value,
                velocity.map(|velocity| velocity.x as i32),
            ));
        },
    );
    rows.sort_by_key(|(_, _, position, _)| *position);

    assert_eq!(
        rows,
        [
            (first, FieldKind::Owned, 6, Some(5)),
            (second, FieldKind::Owned, 21, None),
        ]
    );
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

#[test]
fn query_each_reads_components() {
    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, QueryPosition { x: 3.0, y: 7.0 });
    world.set(entity, QueryVelocity { x: 2.0, y: 4.0 });

    let mut seen = 0;
    world
        .query()
        .each(|position: &QueryPosition, velocity: &QueryVelocity| {
            assert_eq!(position.x, 3.0);
            assert_eq!(position.y, 7.0);
            assert_eq!(velocity.x, 2.0);
            assert_eq!(velocity.y, 4.0);
            seen += 1;
        });

    assert_eq!(seen, 1);
}

#[test]
fn query_each_mutates_inout_component() {
    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, QueryPosition { x: 1.0, y: 2.0 });
    world.set(entity, QueryVelocity { x: 3.0, y: 4.0 });

    world
        .query()
        .each(|position: &mut QueryPosition, velocity: &QueryVelocity| {
            position.x += velocity.x;
            position.y += velocity.y;
        });

    let position = world.get::<QueryPosition>(entity).unwrap();
    assert_eq!(position.x, 4.0);
    assert_eq!(position.y, 6.0);
}

#[test]
fn query_require_filters_without_field() {
    let mut world = World::new();
    let matching = world.entity();
    let skipped = world.entity();

    world.set(matching, QueryPosition { x: 10.0, y: 0.0 });
    world.set(skipped, QueryPosition { x: 20.0, y: 0.0 });
    world.add::<QueryPlayer>(matching);

    let mut sum = 0.0;
    world
        .query()
        .require::<QueryPlayer>()
        .each(|position: &QueryPosition| {
            sum += position.x;
        });

    assert_eq!(sum, 10.0);
}

#[test]
fn query_exclude_skips_entities() {
    let mut world = World::new();
    let matching = world.entity();
    let skipped = world.entity();

    world.set(matching, QueryPosition { x: 1.0, y: 0.0 });
    world.set(skipped, QueryPosition { x: 100.0, y: 0.0 });
    world.add::<QueryDisabled>(skipped);

    let mut sum = 0.0;
    world
        .query()
        .exclude::<QueryDisabled>()
        .each(|position: &QueryPosition| {
            sum += position.x;
        });

    assert_eq!(sum, 1.0);
}

#[test]
fn system_progress_runs_and_mutates_components() {
    MOVE_CALLS.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, SystemPosition { x: 1 });
    world.set(entity, SystemVelocity { x: 4 });

    world.system("Move").each(move_system);
    assert!(world.progress());

    assert_eq!(MOVE_CALLS.load(Ordering::SeqCst), 1);
    assert_eq!(world.get::<SystemPosition>(entity).unwrap().x, 5);
}

#[test]
fn system_each_accepts_each_ctx_and_resources_without_explicit_lifetimes() {
    let mut world = World::new();
    world.set_resource(DeltaTime { ticks: 4 });

    let entity = world.entity();
    world.set(entity, SystemPosition { x: 1 });
    world.set(entity, SystemVelocity { x: 3 });

    world.system("ResourceMove").each(resource_move_system);
    world.progress();

    assert_eq!(world.get::<SystemPosition>(entity).unwrap().x, 13);
}

#[test]
fn system_require_and_exclude_filter_entities() {
    FILTER_CALLS.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let matching = world.entity();
    let missing_player = world.entity();
    let disabled = world.entity();

    world.set(matching, SystemPosition { x: 2 });
    world.set(missing_player, SystemPosition { x: 10 });
    world.set(disabled, SystemPosition { x: 100 });
    world.add::<SystemPlayer>(matching);
    world.add::<SystemPlayer>(disabled);
    world.add::<SystemDisabled>(disabled);

    world
        .system("CountPlayers")
        .require::<SystemPlayer>()
        .exclude::<SystemDisabled>()
        .each(filter_count_system);
    world.progress();

    assert_eq!(FILTER_CALLS.load(Ordering::SeqCst), 2);
}

#[test]
fn system_enable_disable_controls_execution() {
    ENABLE_CALLS.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, SystemPosition { x: 3 });

    let system = world.system("Count").disabled().each(enable_count_system);
    world.progress();
    assert_eq!(ENABLE_CALLS.load(Ordering::SeqCst), 0);

    world.enable_system(system);
    world.progress();
    assert_eq!(ENABLE_CALLS.load(Ordering::SeqCst), 3);

    world.disable_system(system);
    world.progress();
    assert_eq!(ENABLE_CALLS.load(Ordering::SeqCst), 3);
}

#[test]
fn system_phase_order_matches_c_order() {
    SYSTEM_ORDER.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, SystemPosition { x: 1 });

    world
        .system("OnUpdate")
        .phase(Phase::OnUpdate)
        .each(on_update_system);
    world
        .system("PreUpdate")
        .phase(Phase::PreUpdate)
        .each(pre_update_system);

    world.progress();
    assert_eq!(SYSTEM_ORDER.load(Ordering::SeqCst), 2);
}

#[test]
fn system_after_orders_same_phase_systems() {
    SYSTEM_AFTER_ORDER.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, SystemPosition { x: 1 });

    let first = world.system("AfterFirst").each(after_first_system);
    world
        .system("AfterSecond")
        .after(first)
        .each(after_second_system);

    world.progress();
    assert_eq!(SYSTEM_AFTER_ORDER.load(Ordering::SeqCst), 2);
}

#[test]
fn system_multi_worlds_are_isolated() {
    MULTI_WORLD_CALLS.store(0, Ordering::SeqCst);

    let mut world_a = World::new();
    let mut world_b = World::new();
    let entity_a = world_a.entity();
    let entity_b = world_b.entity();

    world_a.set(entity_a, SystemPosition { x: 1 });
    world_a.set(entity_a, SystemVelocity { x: 2 });
    world_b.set(entity_b, SystemPosition { x: 10 });
    world_b.set(entity_b, SystemVelocity { x: 20 });

    world_a.system("MoveA").each(multi_world_move_system);
    world_b.system("MoveB").each(multi_world_move_system);

    world_a.progress();
    assert_eq!(world_a.get::<SystemPosition>(entity_a).unwrap().x, 3);
    assert_eq!(world_b.get::<SystemPosition>(entity_b).unwrap().x, 10);

    world_b.progress();
    assert_eq!(world_b.get::<SystemPosition>(entity_b).unwrap().x, 30);
    assert_eq!(MULTI_WORLD_CALLS.load(Ordering::SeqCst), 2);
}

#[test]
fn query_multi_worlds_are_isolated() {
    let mut world_a = World::new();
    let mut world_b = World::new();

    let entity_a = world_a.entity();
    let entity_b = world_b.entity();
    world_a.set(entity_a, MultiWorldPosition { value: 7 });
    world_b.set(entity_b, MultiWorldPosition { value: 11 });

    let mut sum_a = 0;
    world_a.query().each(|position: &MultiWorldPosition| {
        sum_a += position.value;
    });

    let mut sum_b = 0;
    world_b.query().each(|position: &MultiWorldPosition| {
        sum_b += position.value;
    });

    assert_eq!(sum_a, 7);
    assert_eq!(sum_b, 11);
}

#[test]
fn query_callback_can_capture_state() {
    let mut world = World::new();
    for value in [1, 2, 3] {
        let entity = world.entity();
        world.set(entity, MultiWorldPosition { value });
    }

    let mut values = Vec::new();
    world.query().each(|position: &MultiWorldPosition| {
        values.push(position.value);
    });
    values.sort();

    assert_eq!(values, [1, 2, 3]);
}

#[test]
#[should_panic(expected = "query callback cannot request the same component field more than once")]
fn query_rejects_duplicate_component_fields() {
    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, MultiWorldPosition { value: 1 });

    world.query().each(
        |left: &mut MultiWorldPosition, right: &MultiWorldPosition| {
            left.value += right.value;
        },
    );
}

#[test]
#[should_panic(
    expected = "callback cannot request mutable and immutable access to the same resource"
)]
fn query_rejects_duplicate_resource_access() {
    let mut world = World::new();
    world.set_resource(QueryScale { value: 1 });

    world
        .query()
        .each(|_read: Res<QueryScale>, _write: ResMut<QueryScale>| {});
}

#[test]
fn rust_multi_world_reuses_component_id_and_keeps_storage_local() {
    let mut world_a = World::new();
    let mut world_b = World::new();

    let id_a = MultiWorldPosition::id(&mut world_a);
    let id_b = MultiWorldPosition::id(&mut world_b);
    assert_eq!(id_a, id_b);

    let entity_a = world_a.entity();
    let entity_b = world_b.entity();

    world_a.set(entity_a, MultiWorldPosition { value: 10 });
    world_b.set(entity_b, MultiWorldPosition { value: 20 });

    assert_eq!(
        world_a.get::<MultiWorldPosition>(entity_a).unwrap().value,
        10
    );
    assert_eq!(
        world_b.get::<MultiWorldPosition>(entity_b).unwrap().value,
        20
    );

    world_a
        .get_mut::<MultiWorldPosition>(entity_a)
        .unwrap()
        .value = 11;
    world_b
        .get_mut::<MultiWorldPosition>(entity_b)
        .unwrap()
        .value = 21;

    assert_eq!(
        world_a.get::<MultiWorldPosition>(entity_a).unwrap().value,
        11
    );
    assert_eq!(
        world_b.get::<MultiWorldPosition>(entity_b).unwrap().value,
        21
    );
}

#[test]
fn rust_multi_world_queries_only_see_their_world_tables() {
    let mut world_a = World::new();
    let mut world_b = World::new();

    let entity_a = world_a.entity();
    let entity_b = world_b.entity();
    world_a.set(entity_a, MultiWorldPosition { value: 100 });
    world_b.set(entity_b, MultiWorldPosition { value: 200 });

    let component = MultiWorldPosition::id(&mut world_a);
    assert_eq!(component, MultiWorldPosition::id(&mut world_b));

    let mut desc = raw::QueryDesc::default();
    desc.terms[0] = raw::QueryTerm {
        id: component,
        access: raw::TermAccess::In,
    };

    let query_a = unsafe { raw::ecs_query_init(world_a.as_raw_mut(), &desc) as raw::QueryId };
    let query_b = unsafe { raw::ecs_query_init(world_b.as_raw_mut(), &desc) as raw::QueryId };

    let mut iter_a = unsafe { raw::ecs_query_iter(world_a.as_raw_mut(), query_a) };
    assert!(unsafe { raw::ecs_iter_next(&mut iter_a) });
    assert_eq!(iter_a.count, 1);
    let positions_a = unsafe { raw::ecs_field(&mut iter_a, 0).cast::<MultiWorldPosition>() };
    assert_eq!(unsafe { (*positions_a).value }, 100);
    assert!(!unsafe { raw::ecs_iter_next(&mut iter_a) });

    let mut iter_b = unsafe { raw::ecs_query_iter(world_b.as_raw_mut(), query_b) };
    assert!(unsafe { raw::ecs_iter_next(&mut iter_b) });
    assert_eq!(iter_b.count, 1);
    let positions_b = unsafe { raw::ecs_field(&mut iter_b, 0).cast::<MultiWorldPosition>() };
    assert_eq!(unsafe { (*positions_b).value }, 200);
    assert!(!unsafe { raw::ecs_iter_next(&mut iter_b) });

    unsafe {
        raw::ecs_query_fini(world_a.as_raw_mut(), query_a);
        raw::ecs_query_fini(world_b.as_raw_mut(), query_b);
    }
}

#[test]
fn rust_multi_world_drop_one_world_keeps_other_valid() {
    let mut world_a = World::new();
    let mut world_b = World::new();

    let entity_a = world_a.entity();
    let entity_b = world_b.entity();
    world_a.set(entity_a, MultiWorldPosition { value: 1 });
    world_b.set(entity_b, MultiWorldPosition { value: 2 });

    drop(world_a);

    world_b.set(entity_b, MultiWorldPosition { value: 3 });
    assert_eq!(
        world_b.get::<MultiWorldPosition>(entity_b).unwrap().value,
        3
    );
}
