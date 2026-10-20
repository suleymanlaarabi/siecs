# C++: observers and events

## Custom events, filters and data access

```cpp
struct Damage { int amount; };
auto damage_observer = ecs::observe<Damage>()
    .with<Damageable>().without<Invulnerable>()
    .each([](ecs::event<Damage> event, Health &health) {
        health.value -= event->amount;
    });
ecs::emit(entity, Damage{7});
```

`ecs::event<Damage>` is a borrowed const payload with `*`, `->` and `target()`
(the concrete emitter). `ecs::emit(entity, payload)` dispatches synchronously;
the payload type and `observe<Event>` type must agree. Empty custom payloads
are valid; callbacks need not take a payload parameter.
`ecs::event_id<Event>()` gets/registers the type id; pre-register during setup
before emitting from worker systems.

Observers accept all [shared filters](filters.md):

| Need | Methods |
| --- | --- |
| Components/tags | `.with<T...>()`, `.without<T...>()` |
| Optional component data | Callback `ecs::optional<T>` / `ecs::optional<const T>` |
| Relation presence/absence | `.with_relation<R>()`, `.optional_relation<R>()`, `.without_relation<R>()` |
| Exact target/depth | `.with<R>(target)`, `.depth<R>(n)` |
| Ancestry/ancestor reads | `.is_a(base)`, `.up<T, R>()` |
| Exposed query ordering/raw fields | `.order_by*`, `.optional<T...>()`; ordering does not order delivery, raw fields need matching binding |

Callbacks also accept `ecs::entity`, `ecs::target<R>`, resource wrappers and
custom SystemParams; see [all parameter forms](parameters.md). Use typed lambdas:
observers have no explicit-signature generic-lambda overload. One event invokes
the observer for its emitter, not for every entity matching the query.

## Builder options and returned handle

| Operation | Meaning |
| --- | --- |
| `ecs::observe<Event>()` | New global observer builder |
| `.target(owner)` | Target live owner and its IsA instances; null selects global |
| `owner.observe<Event>(callback)` | Targeted registration shortcut |
| `.user_data(U*)` | Borrow state; retrieve with event context |
| `.each(callback)` | Register; returns non-owning `observer_ref` |
| `ref.id()/enable()/disable()/remove()` | Inspect/control; remove destroys registration and owned captures |

Captures belong to the world/targeted registration. Handle destruction does not
remove the observer; `remove`, owner death or world teardown releases its state
once. Borrowed captures/user data must outlive that registration.
Observers have no `phase`, `interval`, `after`, `main_thread` or independent
scheduler access boundary. They run on the emitter's thread. An enclosing
system must declare every component/resource access made by its observers.

## Component lifecycle events

Tags are `ecs::OnAdd`, `OnSet`, `OnRemove`, `OnRelationSet`, `OnRelationRemove`.
Do not emit builtin tags manually. Builtin component observers require at least
one component/optional field argument; a `.with<T>()` filter alone does not
satisfy that requirement. For a zero-sized tag's lifecycle with no data fields,
use the [C observer API](../c/events.md#component-lifecycle-events).

`ecs::observer_event` exposes `target()`, `id()`, `user_data()`, typed
`user_data<U>()` and unchecked `trigger_data<T>()`. It has no component-id
accessor. Query filters select an entity, not the triggering component; a cast
to Health is unsafe just because the observer has `.with<Health>()`.
For exact incoming-value handling, use C or this public SystemParam hook:

```cpp
struct LifecycleContext {
    ecs_component_t component = 0;
    const void *data = nullptr;
    static LifecycleContext init() noexcept { return {}; }
    void event(ecs_observer_event_t *event) noexcept {
        component = event->component;
        data = event->trigger_data;
    }
};
ecs::observe<ecs::OnSet>().each([](LifecycleContext event, const Health &current) {
    if (event.component != ecs::component<Health>().id() || !event.data) return;
    const auto &incoming = *static_cast<const Health *>(event.data);
    consume_health(incoming.value);
    (void)current; // For set, this can still be the previous value.
});
```

`OnAdd` supplies initialized storage; `OnRemove` supplies storage before removal.
`OnSet` supplies incoming set data, while `modified` supplies current data.
Direct reference edits emit nothing. A callback reading current fields can see
pre-copy values; choose a component hook for per-component storage lifecycle,
or an observer for a broader reaction.

## Relation events and external event ids

```cpp
ecs::observe<ecs::OnRelationSet>().with_relation<GroupOf>()
    .each([](ecs::observer_event event) {
        const auto *edge = event.trigger_data<ecs_relation_event_t>();
        if (!edge || edge->relation != ecs::relation<GroupOf>()) return;
        consume_edge(event.target(), edge->old_target, edge->new_target);
    });
```

RelationSet is after transition; RelationRemove is before removal with zero new
target. Check payload `.relation` even with relation presence filters. For
retarget-away reactions, inspect `.old_target` rather than filtering on the old
target, because RelationSet matches new state.

To reuse an externally registered custom id, a payload type can expose
`static ecs_event_t event_id()` returning that id. `ecs::event_id<T>()`,
`observe<T>()` and `emit(entity, T{...})` then use it. C producers must emit a
payload with the exact T-compatible layout; registration of an id does not
validate payload bytes.

## Delivery and mutation boundaries

Targeted observers include IsA descendants, not ChildOf descendants; emitter
identity remains concrete. Unfiltered targets receive disabled/abstract entities
too; global/filtered observers use normal query exclusions. Ordering filters
do not define observer execution order.

Deferred structural/set changes emit during flush and can coalesce; custom
`emit` and `modified` are immediate even while deferring. Registration/removal
should happen outside active dispatch. Callbacks do not install a new defer
scope: use a producer's outer scope for structural reactions and avoid recursive
updates to the same source. Copy payloads for later work; for costly reactions,
record intent in ECS data for a later system.

[Event timing and lifetime](../events.md) · [API](../../api/cpp-events.md)
