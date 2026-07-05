use siecs::{Commands, Component, Entity, Phase, Query, World};

#[derive(Component)]
struct Position {
    value: i32,
}

fn mark_with_commands(mut query: Query<(Entity, &Position)>, commands: Commands) {
    for (entity, position) in &mut query {
        commands.set(
            entity,
            Position {
                value: position.value + 1,
            },
        );
    }
}

#[test]
fn system_commands_can_mutate_entity() {
    let mut world = World::new();
    let entity = world.entity();
    world.set(entity, Position { value: 1 });

    world.system(Phase::OnUpdate, mark_with_commands);
    world.progress();

    assert_eq!(world.get::<Position>(entity).unwrap().value, 2);
}
