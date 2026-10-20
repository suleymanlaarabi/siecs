# Components, requirements, and reflection

Register data components and zero-sized tags before querying them. Components,
resources, and relations have distinct id spaces. Names and ids are metadata;
use typed access in normal application code and id-based APIs for tools.

`add` establishes presence; `set` adds/replaces a value. Required reads need a
present component; try-read handles absence. Mutable access does not itself emit
`OnSet`: use `modified` when explicit notification is needed. `modified` does
not call the component's `on_set` hook.

Declare requirements and default relations before the component participates
in tables. Adding a component also materializes its requirements; this is not a
dependency-ordered hook contract. Default relations do not overwrite an existing
or explicitly supplied edge. Keep requirement graphs small and acyclic.
Requirements establish add-time structure, not entity ownership.

Hooks operate on storage: `on_add` after initialization, `on_set` before the
copy with incoming/current values, `on_remove` before removal. Register correct
constructor, destructor, copy/move construction and assignment operations for
owned data. Missing C operations use plain zero/memcpy/no-destructor behavior.

Reflection is always included. Macro declarations carry metadata; dynamic
component descriptors derive layout from field declarations. Introspection
returns borrowed metadata. Native C++ types need reflected field declarations
when tooling/serialization needs structure. See [scenes](scenes.md).

Inheritance policy belongs to component registration. Debug-only `EcsSetOnly`
rejects mutable access; use `set` consistently in every configuration.
