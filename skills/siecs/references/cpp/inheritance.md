# C++: templates and inheritance

```cpp
auto base = ecs::spawn().set(Position{10});
auto instance = ecs::instantiate(base);
```

`entity::instantiate(base)`, `spawn().is_a(base)`, and the free `instantiate`
function create instances with the same semantics. `is_a<T>()` targets a typed
singleton. `is(base)`/`is<T>()` checks ancestry for live handles; compare `id()`
values for identity.

For shared components, call `ecs::component<Material>(ecs::component_options<Material>{
.inheritance = EcsInheritShared })` before first use. Read shared data with
`const Material&` in query callbacks. `set(Material{...})` creates a local
override; writable queries cannot mutate shared base storage.

Use query/system/observer `.is_a(base)` filtering for ancestry. `abstract()`
marks a template subtree. The shared C API supplies direct-base inspection and
recursive component propagation when no typed facade exists.

[Copy/share policies and child snapshots](../inheritance.md) · [API](../../api/cpp-inheritance.md)
