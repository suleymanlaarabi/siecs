# C++: queries

Use the [complete filter table](filters.md) for `with`, `without`, `optional`,
relation presence/absence/targets/depth, `is_a`, ancestor reads and ordering.
The [parameter table](parameters.md) covers all callback arguments and custom
SystemParam bindings. Those same filters/parameters work with systems/observers.

## Temporary or persistent

```cpp
ecs::query().with<Moving>().without<Frozen>()
    .each([](ecs::entity entity, Position &p, const Velocity &v,
             ecs::optional<const Mass> mass) {
        p.x += v.x * (mass ? mass->value : 1.0f);
        (void)entity;
    }); // Query is destroyed after this call.

{
    auto moving = ecs::query<Position&, const Velocity&>()
        .with<Moving>().without<Frozen>().build();
    moving.each([](Position &p, const Velocity &v) { p.x += v.x; });
    moving.each([](auto &p, const auto &v) { p.x += v.x; });
} // Destroy before finalizing the world.
```

`query().each(...)` infers access from a typed callback. `query<Args...>` fixes
the callback signature and supports generic lambdas. `build()` returns a move-only
`query_handle<Args...>`. Keep it for recurring work; no match is not an invalid
handle. `each()` obtains current matching batches, including newly created ones.

An untyped `ecs::query().build()` infers/rebuilds its component/resource/relation
terms on `each()` when the callback signature changes. Prefer a fixed signature
for recurring work and whenever using the id before the first `each()`.
For raw constructor `query_handle<Args...>(id)`, supply a query whose returned
fields already match Args in order; it adopts ownership, not a new schema.

The C runtime can allocate query id zero, while the current C++ owner uses zero
as its empty sentinel. If a newly built handle has id zero, it can iterate but
its destructor skips freeing that query. Use `release()` then explicitly
`ecs_query_fini(id)` for that case; never iterate a released/moved-from handle.

## Find, count, expose a raw query

| Operation | Contract |
| --- | --- |
| `builder.first()` | First matching entity or `ecs::entity::null()`; no stable row-order promise |
| `handle.id()` | Borrow the C query id; do not free it separately |
| `ecs_query_count(handle.id())` | Count current entities; no typed `count()` method |
| `builder.build_raw()` | Create a raw id; caller must `ecs_query_fini(id)` |
| `handle.release()` | Transfer the id; handle becomes empty and no longer frees it |
| `query_handle<Args...>(id)` | Adopt an existing compatible raw query |

`first()` uses the builder's filters and explicitly declared signature; it cannot
infer requirements from a future callback. Use `query<const Position&>()` or
`.with<Position>()` when searching for Position.

```cpp
auto raw = ecs::query().with<Moving>().optional<Mass>().build_raw();
ecs_iter_t it = ecs_query_iter(raw);
while (ecs_iter_next(&it)) {
    auto *mass = static_cast<Mass *>(ecs_field(&it, 0));
    if (mass) { /* mass[row] is writable owned storage. */ }
}
ecs_query_fini(raw);
```

The builder's raw `.optional<T>()` is distinct from a callback's
`ecs::optional<T>`; do not add both for the same T. Raw iteration follows the
[C field/defer rules](../c/queries.md#fields).

## Iteration behavior

Callback references and optional values automatically handle shared versus owned
storage. `const T&` can read shared inheritance; `T&` requires owned storage.
Wrappers use value syntax: `ecs::entity`, `ecs::target<R>`, `ecs::res<T>`,
`ecs::optional<T>`. `res<T>` is a resource dependency, not an entity filter.

`query().each([](ecs::entity e) { ... })` visits ordinary entities even without
component criteria. For filter-only iteration, include entity identity if work
must happen once per row; a callback with no row parameter runs per table batch.
`each()` provides a defer scope for structural mutations until the iteration
ends. Direct component/resource writes are immediate and emit no change event.
Callback `return` skips only that invocation; it does not stop the query.

Do not retain references/views, create queries during a parallel system wave,
or let owners outlive the world. Use `first()` or manual C iteration for early
termination. Initialize all resources used by callbacks first.

[Query contracts](../queries.md) · [API](../../api/cpp-queries.md)
