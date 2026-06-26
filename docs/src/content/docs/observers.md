---
title: Observers And Events
description: Reacting to component and custom events.
---

Observers run callbacks when an event is emitted for an entity matching a query.

## Built-In Events

SIECS currently exposes three built-in events:

| Event | Trigger |
| --- | --- |
| `EcsOnAdd` | A component is added to an entity. |
| `EcsOnRemove` | A component is removed from an entity. |
| `EcsOnSet` | A component value is set with `ecs_set()` or `ecs_set_cid()`. |

## Create An Observer

```c
static void on_position_set(ecs_observer_event_t *event) {
    const Position *value = event->trigger_data;
    (void)value;
}

ecs_observer(world, {
    .on = EcsOnSet,
    .query = {
        .terms = { ecs_in(Position) },
    },
    .callback = on_position_set,
});
```

`callback` is required.

Observer queries follow the same matching rules as normal queries. Entities with
the built-in `Disabled` component do not trigger matching observers unless the
observer query mentions `Disabled` explicitly:

```c
ecs_observer(world, {
    .on = EcsOnSet,
    .query = {
        .terms = { ecs_in(Position), ecs_filter(Disabled) },
    },
    .callback = on_disabled_position_set,
});
```

Observers are enabled by default. Use `ecs_observer_disable()` and
`ecs_observer_enable()` to toggle an observer without unregistering it.

Pass small callback context through `user_data` when needed:

```c
ecs_observer(world, {
    .on = EcsOnSet,
    .query.terms = { ecs_in(Position) },
    .callback = on_position_set,
    .user_data = (uintptr_t)counter_ptr,
});
```

## Event Payload

The callback receives `ecs_observer_event_t`:

```c
typedef struct {
    ecs_world_t *world;
    ecs_entity_t entity;
    ecs_event_t event;
    uintptr_t user_data;
    const void *trigger_data;
} ecs_observer_event_t;
```

`trigger_data` depends on the event:

| Event | `trigger_data` |
| --- | --- |
| `EcsOnAdd` | Pointer to the added component storage. |
| `EcsOnRemove` | Pointer to the component value before removal. |
| `EcsOnSet` | Pointer to the new value passed to `ecs_set()` or `ecs_set_cid()`. |
| Custom event | Pointer passed to `ecs_observer_trigger()`. |

For `OnSet`, the stored component value has not been overwritten yet when hooks
and observers run.

Component hooks run before matching observers for the same operation.

## Custom Events

Create custom event ids with `ecs_event()`:

```c
ecs_event_t Damaged = ecs_event(world);

ecs_observer(world, {
    .on = Damaged,
    .query = {
        .terms = { ecs_in(Health) },
    },
    .callback = on_damaged,
});

Damage damage = { .amount = 10 };
ecs_observer_trigger(world, entity, Damaged, &damage);
```
