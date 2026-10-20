# C++: resources

```cpp
struct Settings { float speed; };
ecs::resource<Settings>().set(Settings{2});
ecs::system("Move").each([](Position &p, ecs::res<const Settings> settings) {
    p.x += settings->speed;
});
```

`resource<T>()` returns a non-owning handle. `get/try_get` read;
`edit/try_edit` write. `resource<const T>` cannot set, edit, or remove.
`has`, `try_get`, and removal of an absent resource do not register its type.
`id()` or storage creation can register it.

`set` copies lvalues or moves rvalues. Install `resource_hooks<T>` before the
first registration. Typed lifecycle operations support owned/non-trivial data.
`res<const T>` declares read access; `res<T>` declares write access and borrows
storage for the callback. Initialize the resource before the system runs.

## Handle operations and hooks

| Operation | Behavior |
| --- | --- |
| `.id()` | Register/return the resource id; does not create its value |
| `.has()` / `.try_get()` | Test storage / nullable const pointer; no registration on absence |
| `.get()` | Required const reference |
| `.try_edit()` / `.edit()` | Nullable / required mutable access |
| `.set(value)` | Construct or replace the world value |
| `.remove()` | Destroy/remove storage; registration remains |

```cpp
ecs::resource<Settings>(ecs::resource_hooks<Settings>{
    .on_set = [](const Settings &incoming) { (void)incoming; },
    .on_remove = [](const Settings &previous) { (void)previous; },
});
ecs::resource<Settings>().set(Settings{2});
```

Hooks are non-capturing function pointers `void(const T&)`. `on_set` sees input
before storage transfer; `on_remove` sees previous storage before destruction.
Direct mutable edits do not call on_set, and resources have no component OnSet
notification API. C-declared resources use their C hooks/ops instead.
`resource<const T>()` is the const view; requesting it does not create storage.

Resource mutations are immediate even during deferral; removal invalidates
active borrowed views. `res<T>` is required, not nullable. For optional service
access, use a [custom nullable resource parameter](parameters.md#custom-systemparam-protocol)
that declares the dependency and calls `try_get`; there is no `optional<res<T>>`
resource parameter. Metadata/registration
inspection uses [the C ABI](../introspection.md).

[Storage, lifecycle, and scheduling](../resources.md) · [API](../../api/cpp-resources.md)
