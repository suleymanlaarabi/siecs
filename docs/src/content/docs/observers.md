---
title: Observers And Events
description: Reacting to component and custom events.
---

Observers run callbacks when an event is emitted for an entity matching a query.

## Built-In Events

SIECS currently exposes three built-in events:

| Event | Trigger |
| --- | --- |
| `OnAdd` | A component is added to an entity. |
| `OnRemove` | A component is removed from an entity. |
| `OnSet` | A component value is set with `ecs_set()` or `ecs_set_cid()`. |

## Create An Observer

```c
static void on_position_set(ecs_observer_event_t *event) {
    const Position *value = event->trigger_data;
    (void)value;
}

ecs_observer(world, {
    .on = OnSet,
    .query = {
        .read = { ecs_id(Position) },
    },
    .callback = on_position_set,
});
```

`callback` is required.

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
| `OnAdd` | Pointer to the added component storage. |
| `OnRemove` | Pointer to the component value before removal. |
| `OnSet` | Pointer to the new value passed to `ecs_set()` or `ecs_set_cid()`. |
| Custom event | Pointer passed to `ecs_observer_trigger()`. |

For `OnSet`, the stored component value has not been overwritten yet when hooks
and observers run.

## Custom Events

Create custom event ids with `ecs_event()`:

```c
ecs_event_t Damaged = ecs_event(world);

ecs_observer(world, {
    .on = Damaged,
    .query = {
        .read = { ecs_id(Health) },
    },
    .callback = on_damaged,
});

Damage damage = { .amount = 10 };
ecs_observer_trigger(world, entity, Damaged, &damage);
```
