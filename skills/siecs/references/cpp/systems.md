# C++: systems and phases

## Select entities and declare access

```cpp
auto move = ecs::system("Move")
    .with<Moving>().without<Frozen>()
    .each([](Position &p, const Velocity &v, ecs::res<const DeltaTime> dt) {
        p.x += v.x * dt->value;
    });
ecs::system("AfterMove").with<Visible>().after(move).main_thread()
    .each([](ecs::entity entity, const Position &p) {
        /* Submit rendering for entity using p. */
        (void)entity; (void)p;
    });
```

These filters all work on systems:

| Need | Methods |
| --- | --- |
| Component/tag presence or absence | `.with<T...>()`, `.without<T...>()` |
| Optional data | Callback `ecs::optional<T>` / `ecs::optional<const T>` |
| Ancestry / ancestor data | `.is_a(base)`, `.up<T, R>()` |
| Relation presence or absence | `.with_relation<R>()`, `.optional_relation<R>()`, `.without_relation<R>()` |
| Exact relation target / depth | `.with<R>(target)`, `.depth<R>(n)` |
| Table ordering | `.order_by(...)`, `.order_by_target<R>()`, `.order_by_depth<R>()` |
| Custom column/batch work | `.optional<T...>()` with a matching [batch parameter](batches.md) |

The [shared filter guide](filters.md) gives storage constraints, AND composition,
duplicate-term rules, optional targets and examples. The [parameter guide](parameters.md)
covers entity, target, optional, resource and custom bindings. Filters only select:
callback component/resource parameters declare actual data access to the scheduler.

## Every system-specific method

Configure the builder before `.each(callback)`; the returned `system_ref` is a
control handle, not a builder. Registration/captures are world-owned.

| Builder method | Behavior / default |
| --- | --- |
| `ecs::system(name)` | Borrowed diagnostic string; default `"unnamed"` |
| `.phase(id)` | Phase, defaults to `EcsOnUpdate` |
| `.after(system_ref)` | Same-phase dependency on an already registered system; repeat for several, maximum 16 |
| `.interval(seconds)` | Nonnegative recurring period; default zero |
| `.disabled(bool = true)` | Initial enabled state; default enabled |
| `.main_thread()` | Keep work on the calling thread |
| `.no_defer()` | Immediate task-only mutation; forbidden with entity criteria |
| `.each(callback)` | Register once; explicitly typed callback, returns `system_ref` |

`system_ref` exposes `id()`, `enable()`, `disable()` and `run()`.
`run()` executes enabled work immediately, ignoring interval gating; it does not
advance frame time or finalize the world. There is no system `remove()` or
system-specific `user_data()` builder; use owned lambda captures. Borrowed
captures and name strings must remain alive until teardown.

## Tasks, filtered batches and row work

```cpp
ecs::system("AdvanceClock").each([](ecs::res<Clock> clock) { clock->ticks++; });
ecs::system("MarkedEntities").with<Moving>()
    .each([](ecs::entity entity) { /* One invocation per entity. */ (void)entity; });
```

Initialize Clock first. Empty/resource-only systems run once per execution;
they have no entity array. `ecs::entity` alone does not add entity criteria, so
use `.with<T>()` or other entity terms for entity-only callbacks.
With entity criteria, a row parameter makes `each` run per row; without one,
a resource-only/empty callback runs per matching table batch. `res<T>` does not
require an entity to have a component named T.

Structural edits inside ordinary systems are deferred automatically. Direct
field/resource edits are immediate and emit no `OnSet`. A callback that runs
nested queries or emits synchronous observers must still declare all accesses
on the enclosing system. Observer callbacks have no independent scheduler job.
Use `.after` when later work must observe earlier structural commands.

## Phases and elapsed time

```cpp
ecs_phase_t physics = ecs::phase("Physics")
    .after(EcsPreUpdate).before(EcsOnUpdate);
ecs::system("PhysicsStep").phase(physics).each([](Position &p) { /* ... */ (void)p; });
```

Set both anchors; an unused one must be `ECS_PHASE_NONE` because the builder's
zero-initialized anchors otherwise refer to `EcsPreStart`. `.id()` or conversion
registers the phase. Both NONE selects the default recurring insertion after
OnUpdate/before PostUpdate; startup anchors create one-shot phases.
`ecs::run_phase(physics)` forces enabled work; `ecs_phase_name(physics)` inspects
its name. Builtin phase order and worker/barrier rules are in [scheduling](../systems.md).

Positive intervals accumulate frame time, run once when due, reset without
catch-up, and are ignored by startup/forced runs. Changing enabled state resets
accumulation. `ecs::res<const DeltaTime>` reads frame delta, **not** accumulated
interval time. For the latter use the iterator-based
[SystemDelta parameter](parameters.md#custom-systemparam-protocol).
Do not add a fictional tick-source/rate API.

[API](../../api/cpp-systems.md)
