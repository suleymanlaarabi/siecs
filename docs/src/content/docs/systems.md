---
title: Systems
description: Running query callbacks in a predictable phase order.
---

Systems are named callbacks attached to a query. They are the usual way to run
game or application logic over matching entities.

## Create A System

A system callback receives an `ecs_iter_t *`. Each call gives a batch of matching
entities for the system query.

```c
static void move_system(ecs_iter_t *it) {
    Position *positions = ecs_field(it, 0);
    Velocity *velocities = ecs_field(it, 1);

    for (uint32_t i = 0; i < it->count; i++) {
        positions[i].x += velocities[i].x;
        positions[i].y += velocities[i].y;
    }
}

ecs_system_id_t Move = ecs_system(world, {
    .name = "Move",
    .phase = EcsOnUpdate,
    .query = {
        .terms = { ecs_inout(Position), ecs_in(Velocity) },
    },
    .callback = move_system,
});
```

`callback` is required. `name` is optional, but recommended because it makes
debugging and traces easier to read.

The query follows the same rules as queries created with `ecs_query()`: `ecs_in`,
`ecs_out`, and `ecs_inout` terms are returned through `ecs_field()`,
`ecs_filter` terms must exist but are not returned, and `ecs_not` terms must not
exist.

## Run Systems

Use `ecs_progress()` to run all enabled systems:

```c
ecs_progress(world);
```

`ecs_progress()` runs phases in this order:

| Phase | Typical use |
| --- | --- |
| `EcsOnLoad` | One-frame load or bootstrap work. |
| `EcsPostLoad` | Work that must run after load systems. |
| `EcsPreUpdate` | Prepare state before simulation. |
| `EcsOnUpdate` | Main simulation logic. |
| `EcsPostUpdate` | Cleanup or derived state after simulation. |
| `EcsPreRender` | Prepare render data. |
| `EcsOnRender` | Render-facing systems. |
| `EcsPostRender` | Cleanup after render-facing systems. |

You can also run one phase or one system manually:

```c
ecs_run_phase(world, EcsOnUpdate);
ecs_run_system(world, Move);
```

`ecs_run_system()` ignores phase order and runs only the selected enabled
system. This is useful for tests or explicit one-off work.

`OnPreUpdate`, `OnUpdate`, `OnPostUpdate`, and `OnRender` are compatibility
aliases for the matching `Ecs*` phase names.

## Order Systems In A Phase

Systems in different phases are ordered by the phase list above. Inside one
phase, use `after` when a system depends on another system having run first:

```c
ecs_system_id_t Integrate = ecs_system(world, {
    .name = "Integrate",
    .phase = EcsOnUpdate,
    .query = { .terms = { ecs_inout(Position), ecs_in(Velocity) } },
    .callback = integrate_system,
});

ecs_system(world, {
    .name = "SyncTransform",
    .phase = EcsOnUpdate,
    .query = { .terms = { ecs_in(Position), ecs_inout(Transform) } },
    .callback = sync_transform_system,
    .after = { Integrate },
});
```

`after` accepts up to four system ids. System id `0` is reserved, so an omitted
or zero-filled `after` array means no dependency.

Dependencies must point to systems in the same phase. Dependency cycles and
cross-phase dependencies are debug assertion failures.

## Enable Or Disable A System

Systems are enabled by default. Create a disabled system with `.disabled = true`
or toggle it later:

```c
ecs_system_id_t Damage = ecs_system(world, {
    .name = "Damage",
    .phase = EcsOnUpdate,
    .query = { .terms = { ecs_inout(Health) } },
    .callback = damage_system,
    .disabled = true,
});

ecs_enable_system(world, Damage, true);
ecs_enable_system(world, Damage, false);
```

Disabled systems are skipped by `ecs_progress()`, `ecs_run_phase()`, and
`ecs_run_system()`.

## Mutation During Systems

System callbacks receive direct pointers to component arrays for the current
batch. Updating those component values in place is expected:

```c
Health *health = ecs_field(it, 0);
health[i].value -= 1;
```

Adding or removing components can move entities between tables and invalidate
the current batch pointers. Keep structural changes outside iteration unless the
API explicitly documents that the operation is safe for iteration.

## Descriptor Reference

```c
typedef struct {
    const char *name;
    ecs_query_desc_t query;
    void (*callback)(ecs_iter_t *);
    ecs_phase_t phase;
    ecs_system_id_t after[4];
    bool disabled;
} ecs_system_desc_t;
```

| Field | Meaning |
| --- | --- |
| `name` | Optional debug name. |
| `query` | Components matched by the system. |
| `callback` | Function called for each non-empty matching batch. |
| `phase` | Phase used by `ecs_progress()` and `ecs_run_phase()`. |
| `after` | Same-phase systems that must run before this one. |
| `disabled` | When true, the system is created but not run. |
