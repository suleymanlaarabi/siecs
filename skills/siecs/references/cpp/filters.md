# C++: all shared builder filters

These methods work on `ecs::query`, `ecs::system` and `ecs::observer<Event>`.
They return the concrete builder, so filters and scheduling/event options can be
chained before `each()` or query `build()`. Register custom relation storage and
policies before any filter lazily registers that relation as `Dense`.

| Method | Meaning / constraint |
| --- | --- |
| `.with<A, B...>()` | Require component/tag presence without a returned field |
| `.without<A, B...>()` | Require component/tag absence |
| `.optional<T...>()` | Append raw optional writable fields; see raw-field caveat below |
| `.is_a(base)` | Require `IsA` ancestry; accepts `ecs::entity` or `ecs_entity_t` |
| `.with_relation<R>()` | Require an R edge, with any target |
| `.optional_relation<R>()` | Allow the edge to be absent; does not itself filter |
| `.without_relation<R>()` | Exclude entities with an R edge |
| `.with<R>(target)` | Exact `ByTarget` target; accepts entity or raw id |
| `.depth<R>(n)` | Exact depth; R must be acyclic `ByDepth` |
| `.order_by_target<R>()` | Table ordering by `ByTarget` target |
| `.order_by_depth<R>()` | Table ordering by `ByDepth` depth |
| `.order_by(ecs_query_order_t{compare, data})` | Custom table comparator |
| `.up<Component, R>()` | Read nearest effective ancestor component through acyclic `ByTarget` R |

`with<R>()` means a **component**, not a relation. Use `with_relation<R>()`
for edge presence, or `with<R>(target)` for a specific target. Mention `Disabled`
or `Abstract` explicitly to override default query exclusion:
`.with<Disabled>()` selects disabled entities.

All selection terms combine with AND; there is no OR or arbitrary value-predicate
builder. Use focused queries or a value check inside the callback. A filter does
not declare component data reads/writes; expose those through callback arguments.

## Combine filters with callback access

```cpp
ecs::system("MoveActive")
    .with<Moving>().without<Frozen>()
    .with<GroupOf>(group)
    .each([](Position &p, const Velocity &v, ecs::target<GroupOf> owner) {
        p.x += v.x;
        (void)owner;
    });
```

Here `Moving/Frozen` only select; Position is writable, Velocity readable, and
the target parameter uses the existing relation term. `GroupOf` must have been
registered with `ByTarget` storage. Filter-only callbacks need `ecs::entity` to
run per entity; otherwise callbacks with no row parameter run per table batch.

One component/relation may appear only once, except these deliberate merges:

- A callback component argument replaces a matching `.with<T>()` filter with its
  inferred access. In particular `optional<T>` makes that term optional.
- `ecs::target<R>` reuses a required/exact-target/depth term; it rejects an
  excluded or optional relation. For optional targets, take `ecs::entity` and
  call `entity.target<R>()`, checking for zero; do not wrap `target<R>` in
  `ecs::optional`, which represents a component field.
- `.up<T, R>()` binds a callback `const T&` to the ancestor; use
  `ecs::optional<const T>` for optional ancestor access. Do not add a direct T
  term or take writable T references for ancestor storage.

```cpp
ecs::query().up<Transform, ParentFrame>()
    .each([](ecs::entity child, ecs::optional<const Transform> parent) {
        if (parent) { /* Read parent->... for child. */ }
        (void)child;
    });
```

## Raw-field and ordering caveats

Prefer `ecs::optional<T>` callback parameters. Builder `.optional<T>()` appends
a field before inferred callback fields; combining it with typed field arguments
can duplicate terms or shift binding. For queries, use `build_raw()` and C
iterator access. Systems/observers need a deliberately matching
[custom field/batch binding](batches.md); ordinary callbacks should use typed
optionals. Similarly, consume each `.up<T, R>()` field through a matching
callback parameter, or use raw iteration.

Custom comparators have signature
`int(const ecs_table_t*, const ecs_table_t*, uint64_t)`. They order tables, not
rows or component values, and must not mutate the world or rely on changing row
values. Observers filter a single emitter; ordering does not order delivery.

Limits: 16 component, 16 resource and 8 relation terms, including callback-inferred
terms. There are no builder `read_resource`/`write_resource` methods: use
`ecs::res<const T>`/`ecs::res<T>` or a [custom SystemParam](parameters.md).

[Queries](queries.md) · [Systems](systems.md) · [Events](events.md) · [Signatures](../../api/cpp-queries.md)
