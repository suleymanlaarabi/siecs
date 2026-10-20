# C++: callback parameters

The same parameter binding serves queries, systems and observers. Components are
lvalue references; wrappers are passed **by value**. Nothing here transfers
ownership of storage to the callback.

| Parameter | Selection / access / value |
| --- | --- |
| `const T&` | Required component read, including shared inherited values |
| `T&` | Required owned component read/write |
| `ecs::optional<const T>` | Optional read; nullable, shared allowed |
| `ecs::optional<T>` | Optional owned read/write; test before dereferencing |
| `ecs::entity` | Current entity; no additional selection or data access |
| `ecs::target<R>` | Required R edge; entity-derived target, inferred relation term |
| `ecs::res<const T>` | Existing resource read; no field or entity filter |
| `ecs::res<T>` | Existing resource read/write; no field or entity filter |
| `ecs::observer_event` | Observer-only event id, emitter, user data and raw payload |
| `ecs::event<Event>` | Custom-observer-only typed const payload and emitter |

Use filter terms for tags. Optional fields expose `bool`, `get()`, `*` and `->`;
resource wrappers expose `get()`, `*` and `->`. `target<R>` exposes entity methods.
An absent optional component and a shared component unavailable for mutable
access both produce an empty mutable optional in queries/systems. In observers,
use const bindings for inherited data; verify owned presence before editing a
mutable optional, because event binding resolves effective component storage.

## Signature inference and call frequency

Use explicitly typed lambdas or a callable with one inferable signature. For a
generic query callback, supply the signature, including wrappers, explicitly:

```cpp
auto q = ecs::query<ecs::entity, Position&, const Velocity&>().build();
q.each([](auto entity, auto &p, const auto &v) { p.x += v.x; (void)entity; });
```

Systems/observers do not have the query signature template: wrap generic or
overloaded algorithms in a typed lambda. Inferred `noexcept`-qualified callable
signatures are not supported by the current traits; keep the wrapper unqualified.
Return values are ignored; `return` finishes one invocation, not the iteration.

For entity queries, a component/optional/entity/target or custom `row` parameter
makes the callback run once per row. With no row parameter, it runs once per
non-empty batch. A resource-only/empty system runs once per execution. Bare
`ecs::entity` does **not** turn a system into an entity query: add an entity
criterion such as `.with<Tag>()`, a component argument or a relation term.
Standalone queries without criteria still visit ordinary tables.

## Custom SystemParam protocol

A type qualifies as `ecs::SystemParam<T>` if `static T init()` returns T.
The parameter must be copyable into callback invocations. Optional hooks:

| Hook | When / purpose |
| --- | --- |
| `static void query(ecs::system_param_query&)` | Registration: declare resource/relation dependencies |
| `void world(ecs_world_t*)` | Once after init for each iteration/system call/event |
| `void table(ecs_iter_t*)` | Before each batch; observer iterators are synthetic, one entity |
| `void row(ecs_iter_t*, uint32_t)` | Before each row |
| `void field(uint16_t)` | Bind one returned field index; see constraint below |
| `void event(ecs_observer_event_t*)` | Observer-only; after table/row binding, before callback |

The public `system_param_query` offers `read_resource<T>()`,
`write_resource<T>()`, `require_relation<R>()`. It has no public arbitrary
component-field declaration helper: use normal component arguments for those.
A custom `field` hook consumes a field slot, so it needs a corresponding builder
field and correct ordering; see the [column/batch recipe](batches.md). Use built-in
optional fields for normal row callbacks. Custom parameters must not hide
undeclared component/resource access.

```cpp
struct SystemDelta {
    float value = 0;
    static SystemDelta init() noexcept { return {}; }
    void table(ecs_iter_t *it) noexcept { value = it->delta_time; }
};
ecs::system("Move").each([](Position &p, const Velocity &v, SystemDelta dt) {
    p.x += v.x * dt.value;
});
```

This reads system elapsed time, including accumulated interval time. It does not
declare a `DeltaTime` resource access because it only reads iterator metadata.
For external services, a custom parameter can declare `read_resource<Service>()`
and borrow it in `world`; the service resource must already exist.
Keep persistent application state in components/resources: parameter objects are
reinitialized per execution and copied per callback.

For a registered resource whose value may be absent, declare access without the
required-storage binding of `res<T>`:

```cpp
template <typename T> struct MaybeResource {
    const T *value = nullptr;
    static MaybeResource init() noexcept { return {}; }
    static void query(ecs::system_param_query &q) { q.read_resource<T>(); }
    void world(ecs_world_t *) noexcept {
        value = ecs::resource<const T>().try_get();
    }
};
ecs::system("OptionalSettings").each([](MaybeResource<Settings> settings) {
    if (settings.value) { /* Read configuration; absence is allowed. */ }
});
```

Registration ensures an id/dependency; it does not create Settings storage.
Do not replace this with `ecs::optional<ecs::res<T>>`, which is a component query.

[Filters](filters.md) · [Lifecycle context example](events.md#component-lifecycle-events) · [Protocol signatures](../../api/cpp-systems.md)
