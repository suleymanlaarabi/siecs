use core::{
    mem::{align_of, size_of},
    ops::ControlFlow,
};
use std::sync::atomic::{AtomicUsize, Ordering};

use siecs::{
    raw, Abstract, Commands, Component, Entity, ParamError, Phase, Query, Res, ResMut, Resource,
    SystemDescBuilder, SystemParam, With, Without, World,
};

#[derive(Component)]
struct Position {
    x: f32,
    y: f32,
}

#[derive(Component)]
struct Velocity {
    x: f32,
    y: f32,
}

#[derive(Component)]
struct Marker;

#[derive(Component)]
struct Player;

#[derive(Component)]
struct Sleeping;

#[derive(Component)]
struct DropComponent {
    value: String,
}

static DROP_COMPONENT_DROPS: AtomicUsize = AtomicUsize::new(0);

impl Drop for DropComponent {
    fn drop(&mut self) {
        DROP_COMPONENT_DROPS.fetch_add(1, Ordering::SeqCst);
    }
}

#[derive(Resource)]
struct DeltaTime(f32);

#[derive(Resource, Default)]
struct Stats {
    moves: usize,
    custom: usize,
}

#[test]
fn raw_query_layout_matches_c() {
    assert_eq!(size_of::<raw::TermAccess>(), 4);
    assert_eq!(align_of::<raw::TermAccess>(), 4);
    assert_eq!(size_of::<raw::QueryTerm>(), 8);
    assert_eq!(size_of::<raw::QueryDesc>(), 520);
    assert_eq!(size_of::<raw::Iter>(), 72);
}

#[test]
fn drop_component_is_moved_and_destroyed_by_runtime() {
    DROP_COMPONENT_DROPS.store(0, Ordering::SeqCst);

    let mut world = World::new();
    let entity = world.entity();

    world.set(
        entity,
        DropComponent {
            value: "first".to_string(),
        },
    );
    world.set(entity, Position { x: 1.0, y: 2.0 });
    assert_eq!(world.get::<DropComponent>(entity).unwrap().value, "first");
    assert_eq!(DROP_COMPONENT_DROPS.load(Ordering::SeqCst), 0);

    world.set(
        entity,
        DropComponent {
            value: "second".to_string(),
        },
    );
    assert_eq!(DROP_COMPONENT_DROPS.load(Ordering::SeqCst), 1);

    world.remove::<DropComponent>(entity);
    assert_eq!(DROP_COMPONENT_DROPS.load(Ordering::SeqCst), 2);
}

#[test]
fn direct_query_reads_and_writes_components() {
    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, Position { x: 1.0, y: 2.0 });
    world.set(entity, Velocity { x: 3.0, y: 5.0 });

    let mut query = world.query::<(&mut Position, &Velocity)>();
    query.each(|(position, velocity)| {
        position.x += velocity.x;
        position.y += velocity.y;
    });

    let position = world.get::<Position>(entity).unwrap();
    assert_eq!(position.x, 4.0);
    assert_eq!(position.y, 7.0);
}

#[test]
fn direct_query_optional_and_entity_items() {
    let mut world = World::new();
    let with_velocity = world.entity();
    let without_velocity = world.entity();

    world.set(with_velocity, Position { x: 1.0, y: 0.0 });
    world.set(with_velocity, Velocity { x: 2.0, y: 0.0 });
    world.set(without_velocity, Position { x: 10.0, y: 0.0 });

    let mut seen = Vec::new();
    let mut query = world.query::<(Entity, &Position, Option<&Velocity>)>();
    query.each(|(entity, position, velocity)| {
        seen.push((entity, position.x as i32, velocity.map(|v| v.x as i32)));
    });
    seen.sort_by_key(|(_, x, _)| *x);

    assert_eq!(
        seen,
        vec![(with_velocity, 1, Some(2)), (without_velocity, 10, None)]
    );
}

#[test]
fn direct_query_with_filter_requires_marker() {
    let mut world = World::new();
    let player = world.entity();
    let non_player = world.entity();

    world.set(player, Position { x: 1.0, y: 0.0 });
    world.add::<Player>(player);
    world.set(non_player, Position { x: 10.0, y: 0.0 });

    let mut seen = Vec::new();
    let mut query = world.query_filtered::<(Entity, &Position), With<Player>>();
    query.each(|(entity, position)| {
        seen.push((entity, position.x as i32));
    });

    assert_eq!(seen, vec![(player, 1)]);
}

#[test]
fn direct_query_without_filter_excludes_marker_without_returning_a_field() {
    let mut world = World::new();
    let awake = world.entity();
    let sleeping = world.entity();

    world.set(awake, Position { x: 1.0, y: 0.0 });
    world.set(sleeping, Position { x: 2.0, y: 0.0 });
    world.add::<Sleeping>(sleeping);

    let mut seen = Vec::new();
    let mut query = world.query_filtered::<(Entity, &Position), Without<Sleeping>>();
    query.each(|(entity, position)| {
        seen.push((entity, position.x as i32));
    });

    assert_eq!(seen, vec![(awake, 1)]);

    let mut without_only = world.query_filtered::<Entity, Without<Sleeping>>();
    assert!(without_only
        .try_each(|entity| if entity == awake {
            ControlFlow::Break(())
        } else {
            ControlFlow::Continue(())
        })
        .is_break());
}

#[test]
fn direct_query_tuple_filter_combines_with_and_without() {
    let mut world = World::new();
    let awake_player = world.entity();
    let sleeping_player = world.entity();
    let awake_non_player = world.entity();

    world.set(awake_player, Position { x: 1.0, y: 0.0 });
    world.add::<Player>(awake_player);

    world.set(sleeping_player, Position { x: 2.0, y: 0.0 });
    world.add::<Player>(sleeping_player);
    world.add::<Sleeping>(sleeping_player);

    world.set(awake_non_player, Position { x: 3.0, y: 0.0 });

    let mut seen = Vec::new();
    let mut query =
        world.query_filtered::<(Entity, &Position), (With<Player>, Without<Sleeping>)>();
    query.each(|(entity, position)| {
        seen.push((entity, position.x as i32));
    });

    assert_eq!(seen, vec![(awake_player, 1)]);
}

#[test]
fn filter_on_same_component_as_read_field_is_allowed() {
    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, Position { x: 1.0, y: 0.0 });

    let mut query = world
        .try_query_filtered::<&Position, With<Position>>()
        .expect("filter term must not count as a returned field");
    let mut positions = 0;
    query.each(|_| positions += 1);

    assert_eq!(positions, 1);
}

#[test]
fn direct_query_reads_owned_and_shared_transparently() {
    let mut world = World::new();
    let base = world.entity();
    let child = world.entity();
    let owned = world.entity();

    world.set(base, Position { x: 9.0, y: 0.0 });
    world.add::<Abstract>(base);
    world.is_a(child, base);
    world.set(owned, Position { x: 10.0, y: 0.0 });

    let mut values = Vec::new();
    let mut query = world.query::<&Position>();
    query.each(|position| values.push(position.x as i32));
    values.sort_unstable();

    assert_eq!(values, vec![9, 10]);
}

#[test]
fn query_try_each_stops_immediately() {
    let mut world = World::new();
    for x in 0..3 {
        let entity = world.entity();
        world.set(
            entity,
            Position {
                x: x as f32,
                y: 0.0,
            },
        );
    }

    let mut visited = 0;
    let mut query = world.query::<&Position>();
    let result = query.try_each(|_| {
        visited += 1;
        ControlFlow::Break("done")
    });

    assert_eq!(result, ControlFlow::Break("done"));
    assert_eq!(visited, 1);
}

#[test]
fn query_state_is_reusable() {
    let mut world = World::new();
    let first = world.entity();
    world.set(first, Position { x: 1.0, y: 0.0 });

    let mut state = world.query_state::<&Position>();
    let mut sum = 0.0;
    state.each(&mut world, |position| sum += position.x);
    assert_eq!(sum, 1.0);

    let second = world.entity();
    world.set(second, Position { x: 2.0, y: 0.0 });

    let mut sum = 0.0;
    state.each(&mut world, |position| sum += position.x);
    assert_eq!(sum, 3.0);
}

#[test]
fn filtered_query_state_is_reusable() {
    let mut world = World::new();
    let first = world.entity();
    let second = world.entity();

    world.set(first, Position { x: 1.0, y: 0.0 });
    world.add::<Player>(first);
    world.set(second, Position { x: 2.0, y: 0.0 });

    let mut state = world.query_state_filtered::<&Position, With<Player>>();
    let mut sum = 0.0;
    state.each(&mut world, |position| sum += position.x);
    assert_eq!(sum, 1.0);

    world.add::<Player>(second);

    let mut sum = 0.0;
    state.each(&mut world, |position| sum += position.x);
    assert_eq!(sum, 3.0);
}

fn move_system(
    mut query: Query<(&mut Position, &Velocity)>,
    time: Res<DeltaTime>,
    mut stats: ResMut<Stats>,
) {
    query.each(|(position, velocity)| {
        position.x += velocity.x * time.0;
        position.y += velocity.y * time.0;
        stats.moves += 1;
    });
}

fn plain_move_system(mut query: Query<(&mut Position, &Velocity)>) {
    query.each(|(position, velocity)| {
        position.x += velocity.x;
        position.y += velocity.y;
    });
}

fn filtered_player_move_system(mut query: Query<(&mut Position, &Velocity), With<Player>>) {
    query.each(|(position, velocity)| {
        position.x += velocity.x;
        position.y += velocity.y;
    });
}

fn multi_query_system(
    mut moving: Query<(&mut Position, &Velocity), With<Player>>,
    mut sleeping: Query<Entity, With<Sleeping>>,
    mut stats: ResMut<Stats>,
) {
    moving.each(|(position, velocity)| {
        position.x += velocity.x;
        position.y += velocity.y;
        stats.moves += 1;
    });

    sleeping.each(|_| stats.custom += 1);
}

#[test]
fn system_accepts_bevy_style_query_function() {
    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, Position { x: 1.0, y: 2.0 });
    world.set(entity, Velocity { x: 3.0, y: 5.0 });

    world.system(Phase::OnUpdate, plain_move_system);
    world.progress();

    let position = world.get::<Position>(entity).unwrap();
    assert_eq!(position.x, 4.0);
    assert_eq!(position.y, 7.0);
}

#[test]
fn system_query_supports_filters() {
    let mut world = World::new();
    let player = world.entity();
    let non_player = world.entity();

    world.set(player, Position { x: 1.0, y: 2.0 });
    world.set(player, Velocity { x: 3.0, y: 5.0 });
    world.add::<Player>(player);

    world.set(non_player, Position { x: 10.0, y: 20.0 });
    world.set(non_player, Velocity { x: 30.0, y: 50.0 });

    world.system(Phase::OnUpdate, filtered_player_move_system);
    world.progress();

    let position = world.get::<Position>(player).unwrap();
    assert_eq!(position.x, 4.0);
    assert_eq!(position.y, 7.0);

    let position = world.get::<Position>(non_player).unwrap();
    assert_eq!(position.x, 10.0);
    assert_eq!(position.y, 20.0);
}

#[test]
fn system_supports_multiple_queries() {
    let mut world = World::new();
    let player = world.entity();
    let sleeping = world.entity();

    world.set(player, Position { x: 1.0, y: 2.0 });
    world.set(player, Velocity { x: 3.0, y: 5.0 });
    world.add::<Player>(player);

    world.set(sleeping, Position { x: 10.0, y: 20.0 });
    world.add::<Sleeping>(sleeping);
    world.set_resource(Stats::default());

    world.system(Phase::OnUpdate, multi_query_system);
    world.progress();

    let position = world.get::<Position>(player).unwrap();
    assert_eq!(position.x, 4.0);
    assert_eq!(position.y, 7.0);
    assert_eq!(world.resource::<Stats>().moves, 1);
    assert_eq!(world.resource::<Stats>().custom, 1);
}

#[test]
fn system_query_resources_and_progress() {
    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, Position { x: 1.0, y: 2.0 });
    world.set(entity, Velocity { x: 10.0, y: 20.0 });
    world.set_resource(DeltaTime(0.5));
    world.set_resource(Stats::default());

    world.system(Phase::OnUpdate, move_system);
    world.progress();

    let position = world.get::<Position>(entity).unwrap();
    assert_eq!(position.x, 6.0);
    assert_eq!(position.y, 12.0);
    assert_eq!(world.resource::<Stats>().moves, 1);
}

fn command_system(mut query: Query<(Entity, &Position)>, commands: Commands) {
    query.each(|(entity, position)| {
        commands.set(
            entity,
            Velocity {
                x: position.x + 1.0,
                y: position.y + 1.0,
            },
        );
    });
}

#[test]
fn system_commands_defer_to_c_buffer() {
    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, Position { x: 4.0, y: 8.0 });

    world.system(Phase::OnUpdate, command_system);
    world.progress();

    let velocity = world.get::<Velocity>(entity).unwrap();
    assert_eq!(velocity.x, 5.0);
    assert_eq!(velocity.y, 9.0);
}

#[derive(Clone, Copy)]
struct CustomValue(usize);

unsafe impl SystemParam for CustomValue {
    type State = ();
    type Item<'world> = CustomValue;

    fn init_state(
        _world: &mut World,
        _desc: &mut SystemDescBuilder,
    ) -> Result<Self::State, ParamError> {
        Ok(())
    }

    unsafe fn get_param<'world>(
        _state: &'world mut Self::State,
        _ctx: siecs::system::SystemContext<'world>,
    ) -> Self::Item<'world> {
        CustomValue(42)
    }
}

fn custom_system(value: CustomValue, mut stats: ResMut<Stats>) {
    stats.custom = value.0;
}

#[test]
fn custom_system_param_runs() {
    let mut world = World::new();
    world.set_resource(Stats::default());
    world.system(Phase::OnUpdate, custom_system);
    world.progress();
    assert_eq!(world.resource::<Stats>().custom, 42);
}

fn conflicting_resources(_read: Res<Stats>, _write: ResMut<Stats>) {}

#[test]
fn access_conflicts_are_reported_once_at_registration() {
    let mut world = World::new();
    world.set_resource(Stats::default());

    let err = match world.try_query::<(&mut Position, &Position)>() {
        Ok(_) => panic!("duplicate component access should be rejected"),
        Err(err) => err,
    };
    assert_eq!(err, ParamError::DuplicateComponentField);

    let err = match world.try_query_filtered::<(&mut Position, &Position), With<Player>>() {
        Ok(_) => panic!("duplicate component access with a filter should be rejected"),
        Err(err) => err,
    };
    assert_eq!(err, ParamError::DuplicateComponentField);

    let err = world
        .try_system(Phase::OnUpdate, conflicting_resources)
        .expect_err("resource read/write conflict should be rejected");
    assert_eq!(err, ParamError::ResourceReadWriteConflict);
}

#[test]
fn world_creates_and_kills_entities() {
    let mut world = World::new();
    let entity = world.entity();
    assert!(world.is_alive(entity));
    world.kill(entity);
    assert!(!world.is_alive(entity));
}

#[test]
fn derived_component_add_remove() {
    let mut world = World::new();
    let entity = world.entity();

    world.add::<Marker>(entity);
    assert!(world.has::<Marker>(entity));

    world.remove::<Marker>(entity);
    assert!(!world.has::<Marker>(entity));
}
