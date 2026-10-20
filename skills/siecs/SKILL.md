---
name: siecs
description: Write and review application code using SIECS's C17 or C++20 API. Use for SIECS components, queries, systems, relations, modules, events, and scenes.
---

# SIECS

Use SIECS for application state and behavior: components hold data, systems
transform it, relations express connections, resources supply world dependencies,
and modules package reusable features. Pure functions and external libraries are
tools called by systems.

## Design rules

- Compose capabilities instead of building entity class hierarchies or one large
  component/system per feature. Split independently accessed data; keep fields
  together when systems use them together.
- Think in batches and contiguous columns. Keep repeated queries alive. Declare
  actual read/write access, including resources, so scheduling remains correct.
- Make ownership, ordering, and dependencies explicit. Use relation cleanup for
  entity ownership, phases/system dependencies for ordering, and small modules
  for reusable registration boundaries.
- Keep frequently changing values in component data unless queries need a
  structural distinction. Avoid repeated name lookup and unnecessary migrations.

## Read only what the task needs

Choose the project's language, then the relevant guide below. Each guide links
to its precise API catalogue; search that file for a symbol when needed.
Use the bundled guides for behavior and catalogues for exact signatures;
normal core usage must not require a SIECS source checkout.
Read shared semantics only when the guide points to a contract you need.
Do not load every guide or catalogue before coding.
For an unfamiliar symbol, search `api/` by name; its [index](api/README.md) groups topics.

| Need | C17 | C++20 |
| --- | --- | --- |
| Build, initialize, stop, defer | [world](references/c/world.md) | [world](references/cpp/world.md) |
| Entities, names, paths, enabled state | [entities](references/c/entities.md) | [entities](references/cpp/entities.md) |
| Components, tags, requirements, hooks, reflection | [components](references/c/components.md) | [components](references/cpp/components.md) |
| World resources | [resources](references/c/resources.md) | [resources](references/cpp/resources.md) |
| All selection filters and combinations | [filters](references/c/filters.md) | [filters](references/cpp/filters.md) |
| Queries, optional fields, ordering, ancestor access | [queries](references/c/queries.md) | [queries](references/cpp/queries.md) |
| Systems, phases, workers, intervals | [systems](references/c/systems.md) | [systems](references/cpp/systems.md) |
| Relations, hierarchy, targets, cleanup | [relations](references/c/relations.md) | [relations](references/cpp/relations.md) |
| Templates, IsA, shared/owned inheritance | [inheritance](references/c/inheritance.md) | [inheritance](references/cpp/inheritance.md) |
| Observers and custom events | [events](references/c/events.md) | [events](references/cpp/events.md) |
| Modules and dynamic imports | [modules](references/c/modules.md) | [modules](references/cpp/modules.md) |
| Scene serialization and introspection | [scenes](references/c/scenes.md) | [scenes](references/cpp/scenes.md) |
| Registry enumeration and reflected fields | [introspection](references/introspection.md) | [introspection](references/introspection.md) |
| Shared C/C++ declarations and library boundaries | [interop](references/c/interop.md) | [interop](references/cpp/interop.md) |

For feature decomposition, read [design](references/design.md). For a complete
small program, read the language's world guide.
For C++ callback arguments/custom bindings, read [parameters](references/cpp/parameters.md).

## SIECS contracts to remember

- There is **one active world**. Register schema during setup/module import;
  do not register types while parallel systems execute.
- Entity handles survive migration; component pointers and iterator views do
  not. Reacquire storage after structural mutation or a defer flush.
- In C++, `get()` reads; `edit()` writes. Query callback `const T&` reads and
  `T&` writes. Shared inherited fields are read-only.
- Defer structural mutation during iteration. C++ query callbacks and ordinary
  entity systems already defer; manual C query loops need an explicit scope.
- Use the API revision identified in `api/README.md`; keep the skill aligned
  with the application's SIECS version. Do not fill usage gaps by guessing APIs
  from another ECS or adding old-API compatibility layers.

The skill covers the core C/C++ APIs. Addons have separate contracts.
