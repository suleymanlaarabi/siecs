use criterion::{black_box, criterion_group, criterion_main, Criterion, Throughput};
use siecs::{Abstract, Component, World};
use std::time::Duration;

const ENTITIES: usize = 10_000;

#[derive(Component)]
struct Position(f32);

#[derive(Component)]
struct Velocity(f32);

#[derive(Component)]
struct Acceleration(f32);

#[derive(Component)]
struct Mass(f32);

fn owned_world() -> World {
    let mut world = World::new();
    for index in 0..ENTITIES {
        let entity = world.entity();
        world.set(entity, Position(index as f32));
        world.set(entity, Velocity(1.0));
        world.set(entity, Acceleration(0.5));
        world.set(entity, Mass(2.0));
    }
    world
}

fn mixed_world() -> World {
    let mut world = World::new();
    let base = world.entity();
    world.set(base, Position(1.0));
    world.add::<Abstract>(base);

    for _ in 0..ENTITIES {
        let entity = world.entity();
        world.is_a(entity, base);
        world.set(entity, Velocity(1.0));
    }
    world
}

fn query_benchmarks(criterion: &mut Criterion) {
    let mut group = criterion.benchmark_group("query");
    group.throughput(Throughput::Elements(ENTITIES as u64));

    {
        let mut world = owned_world();
        let mut query = world.query_state::<&mut Position>();
        group.bench_function("owned_1", |bencher| {
            bencher.iter(|| {
                query.each(&mut world, |position| {
                    position.0 = black_box(position.0 + 1.0);
                });
            });
        });
    }

    {
        let mut world = owned_world();
        let mut query = world.query_state::<(&mut Position, &Velocity, &Acceleration, &Mass)>();
        group.bench_function("owned_4", |bencher| {
            bencher.iter(|| {
                query.each(&mut world, |(position, velocity, acceleration, mass)| {
                    position.0 = black_box(position.0 + velocity.0 + acceleration.0 * mass.0);
                });
            });
        });
    }

    {
        let mut world = mixed_world();
        let mut query = world.query_state::<(&Position, &mut Velocity)>();
        group.bench_function("mixed_shared_owned", |bencher| {
            bencher.iter(|| {
                query.each(&mut world, |(position, velocity)| {
                    velocity.0 = black_box(velocity.0 + position.0);
                });
            });
        });
    }

    {
        let mut world = owned_world();
        let mut query = world.query_state::<(&Position, Option<&Velocity>)>();
        group.bench_function("optional_present", |bencher| {
            bencher.iter(|| {
                query.each(&mut world, |(position, velocity)| {
                    black_box(position.0 + velocity.map_or(0.0, |value| value.0));
                });
            });
        });
    }

    {
        let mut world = World::new();
        for index in 0..ENTITIES {
            let entity = world.entity();
            world.set(entity, Position(index as f32));
        }
        let mut query = world.query_state::<(&Position, Option<&Velocity>)>();
        group.bench_function("optional_absent", |bencher| {
            bencher.iter(|| {
                query.each(&mut world, |(position, velocity)| {
                    black_box(position.0 + velocity.map_or(0.0, |value| value.0));
                });
            });
        });
    }

    group.finish();
}

criterion_group! {
    name = benches;
    config = Criterion::default()
        .warm_up_time(Duration::from_secs(1))
        .measurement_time(Duration::from_secs(2))
        .sample_size(20);
    targets = query_benchmarks
}
criterion_main!(benches);
