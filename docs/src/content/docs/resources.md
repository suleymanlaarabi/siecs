---
title: Resources
description: Storing and reading unique world-level values from C.
---

Resources are component-typed values stored once per world. Use them for state
that is unique to the world, not attached to each entity: frame time, input
state, renderer handles, asset registries, shared config, or global simulation
settings.

Resources reuse the same ids as components, but they are stored in a separate
world index. They are not added to an entity, they are not query terms, and they
do not consume an `ecs_field()` slot.

## Declare A Resource

Declare the resource type like a component:

```c
ECS_RESOURCE_DECLARE(Time, {
    float dt;
    float elapsed;
});
```

Define the storage in exactly one C file:

```c
ECS_RESOURCE_DEFINE(Time);
```

Register it once per world before using it:

```c
ECS_RESOURCE_REGISTER(world, Time);
```

The resource macros intentionally mirror component macros. A resource has a
normal `ecs_component_t` id, so generic code can also use the `_cid` functions.

## Set And Read

`ecs_set_resource()` creates or replaces the resource value:

```c
ecs_set_resource(world, Time, {
    .dt = 0.016f,
    .elapsed = 0.0f,
});
```

Use `ecs_resource()` when the resource must exist:

```c
Time *time = ecs_resource(world, Time);
time->elapsed += time->dt;
```

Use `ecs_resource_read()` when the caller only needs read access:

```c
const Time *time = ecs_resource_read(world, Time);
```

If absence is valid, use the nullable helpers:

```c
Time *time = ecs_try_resource(world, Time);
if (time != NULL) {
    time->elapsed += time->dt;
}

const Time *read_time = ecs_try_resource_read(world, Time);
```

`ecs_resource()` and `ecs_resource_read()` assert when the resource does not
exist. The `try` variants return `NULL`.

## Presence And Removal

Check whether a resource exists with `ecs_has_resource()`:

```c
if (!ecs_has_resource(world, Time)) {
    ecs_set_resource(world, Time, {
        .dt = 0.0f,
        .elapsed = 0.0f,
    });
}
```

Remove it with `ecs_remove_resource()`:

```c
ecs_remove_resource(world, Time);
```

Removing a missing resource is a no-op.

## Use Resources In C Systems

In C, systems receive an `ecs_iter_t *`. Read resources from `it->world` before
the entity loop:

```c
static void move_system(ecs_iter_t *it) {
    const Time *time = ecs_resource_read(it->world, Time);
    Position *positions = ecs_field(it, 0);
    const Velocity *velocities = ecs_field(it, 1);

    for (uint32_t i = 0; i < it->count; i++) {
        positions[i].x += velocities[i].x * time->dt;
        positions[i].y += velocities[i].y * time->dt;
    }
}

ecs_system(world, {
    .name = "Move",
    .phase = EcsOnUpdate,
    .query = {
        .terms = { ecs_inout(Position), ecs_in(Velocity) },
    },
    .callback = move_system,
});
```

The resource is not part of the query. The system above matches entities with
`Position` and `Velocity`; it does not require entities to have `Time`.

## Id-Based API

Typed helpers are wrappers around the id-based API:

```c
void ecs_set_resource_cid(ecs_world_t *world, ecs_component_t id, const void *data);
void *ecs_resource_cid(ecs_world_t *world, ecs_component_t id);
void *ecs_try_resource_cid(ecs_world_t *world, ecs_component_t id);
bool ecs_has_resource_cid(const ecs_world_t *world, ecs_component_t id);
void ecs_remove_resource_cid(ecs_world_t *world, ecs_component_t id);
```

This is useful for generic module code:

```c
ecs_component_t time_id = ecs_id(Time);

if (ecs_has_resource_cid(world, time_id)) {
    const Time *time = ecs_resource_cid(world, time_id);
    /* use time */
}
```

## Hooks

Resources reuse component descriptors, so component hooks also apply to
resources:

- `on_set` runs when `ecs_set_resource()` writes a value.
- `on_remove` runs when `ecs_remove_resource()` removes the value.
- `on_remove` also runs during `ecs_fini()` for resources still present in the
  world.

Hooks receive entity id `0`, because resources are world-level values and are
not stored on public entities.

```c
static void on_time_set(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t id,
    void *data
) {
    (void)world;
    (void)entity; /* Always 0 for resources. */
    (void)id;

    Time *time = data;
    if (time->dt < 0.0f) {
        time->dt = 0.0f;
    }
}
```

## Performance Notes

Resource lookup is a direct index by `ecs_component_t`, so access is O(1). The
value is stored once in the world and is independent from table iteration.

For hot C systems, resolve the resource once before the loop over `it->count`,
then reuse the pointer for every entity in the batch.
