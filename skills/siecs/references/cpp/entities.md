# C++: entities

```cpp
auto scene = ecs::ensure("Scene");
auto actor = ecs::ensure(scene, "Actor");
actor.set(Position{1}, Velocity{2});
const auto &position = actor.get<Position>();
actor.edit<Velocity>().x = position.x;
actor.disable();
actor.enable();
```

`spawn()` creates fresh entities; `spawn(parent)` adds `ChildOf`.
`spawn_no_reuse()` avoids recycled indices. `ensure` finds/creates a path;
`entity::lookup` only finds. `singleton<T>()` returns a named entity for a type,
distinct from a resource, and resets its cache at shutdown.

Handles are non-owning. `entity::from(id)` wraps without validation;
`is_alive()` checks liveness. Boolean conversion only checks nonzero.
`id()`, `name()`, `path()`, `set_name()`, and `kill()` cover identity/lifetime.
Use `try_get/try_edit` for absent data. Variadic `set/add/remove` group mutations.
`children`, `child_of`, `is_a`, and relation methods compose structure.

`has<A, B...>()` requires all listed components. Enabled-state accessors
`is_enabled()/is_disabled()` test the Disabled tag, not system/module state;
an enabled entity can still be Abstract and excluded from ordinary queries.
`ensure("A::B")` can create missing parents and reuses existing named entities;
to create fresh named content use `spawn()` then `set_name()`/`child_of()` with
the documented collision policy. `set_name(nullptr)` removes a name; check its
boolean result; an empty name also removes naming. Leading `::` paths resolve
from the root even with a parent.

`singleton<T>()` is a named entity anchor, not automatic storage for a component
T. It can be a typed relation/template target. Do not confuse it with the
single world resource value returned by `resource<T>()`.

[Identity, collisions, and deferred names](../entities.md) · [API](../../api/cpp-entities.md)
