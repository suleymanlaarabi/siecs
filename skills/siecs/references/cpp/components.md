# C++: components

Ordinary structs register through typed use or `ecs::component<T>()`.

```cpp
struct Position { float x; };
struct Velocity { float x; };
struct Moving {};

ecs::component<Moving>().with<Position, Velocity>();
auto entity = ecs::spawn().add<Moving>();
entity.set(Position{1});
```

Empty structs serve as presence markers but still use `sizeof(T)` storage;
`ECS_TAG` declarations provide zero-sized tags. `component_requires<C, R>()`
declares a requirement. `component_ref<T>::id()` exposes its id.

Native non-trivial types infer the supported lifecycle operations. Trivially
copyable types use zero/memcpy storage; do not rely on default member initializers
being run by `add<T>()`.
Install `component_hooks<T>` or `component_options<T>` before first use; options
include inheritance and the debug-only mutation field. Do not replace those
options by registering the same type again later.

For reflection, declare fields with `reflected(...)` inside the struct; keep
owned data lifecycle correct. C-shared types use the macro declarations from
the [interop guide](interop.md). Read through `get/try_get`, mutate through
`edit/try_edit` or `set`. Explicit change notification uses
`ecs_modified_cid(entity.id(), ecs::component<T>().id())`.

## Native hooks and options

```cpp
ecs::component<Health>(ecs::component_hooks<Health>{
    .on_set = [](ecs_entity_t entity, const Health &incoming, Health &current) {
        /* Validate/react before the copy; incoming and current are borrowed. */
        (void)entity; (void)incoming; (void)current;
    },
});
```

Hook callbacks are function pointers, so use non-capturing lambdas:
`on_add/on_remove` have signature `void(ecs_entity_t, T&)`, and `on_set` has
`void(ecs_entity_t, const T&, T&)`. C-declared types retain their C descriptor
hooks; typed options cannot replace them. Native options contain `.hooks`,
`.inheritance` and debug-only `.mutation`. Install them before any entity/query
or requirement implicitly registers T. `ECS_SET_ONLY` can populate `.mutation`
portably inside a descriptor/options initializer; it expands away in release.

## Entity data operations

| Operation | Contract |
| --- | --- |
| `entity.add<A, B...>()` | Add missing values/requirements; existing values remain |
| `entity.set(a, b, ...)` | Add/replace concrete values; lvalues copy, rvalues move |
| `entity.remove<A, B...>()` | Remove owned values if present |
| `entity.has<A, B...>()` | All requested effective components are available |
| `entity.get<T>()` / `try_get<T>()` | Const reference / nullable const pointer |
| `entity.edit<T>()` / `try_edit<T>()` | Mutable reference / nullable pointer; use on owned storage |
| `ecs_has_cid_owned(entity.id(), ecs::component<T>().id())` | Explicit ownership test; no typed owned-presence overload |

Presence/try-access handles missing components, not dead entities; validate a
handle before touching its data. `get/edit` require presence. For shared inherited
values use const access; create a local value with `set` before writing.
Variadic mutations defer as one group; do not observe each argument as a separate
immediate transition. Direct reference writes need explicit `ecs_modified_cid`
if observers must be notified. SetOnly requires a setter rather than direct edits.

For runtime-created types and low-level C descriptor operations, use
[C registration](../c/registration.md); for metadata, [introspection](../introspection.md).

[Lifecycle, requirements, and metadata](../components.md) · [API](../../api/cpp-components.md)
