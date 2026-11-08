# Native typed pairs

Status: **native core and Script integration implemented**.

The Script runtime currently supports exclusive native relations. A relation
retains one target per source; assigning another target replaces the edge.
`ChildOf` and `IsA` use their existing native storage and invariants.

The core and Script support independent payloads for `(Position, Target)` and
`(Target, Position)`. Script infers the payload from the first component element,
or from the second when the first is not a component. Entity/entity pairs are tags.
`Source[Position, Target]` reads a payload; `Source?[Position, Target]` tests presence.

## Core contract

`ecs_pair_init` interns an `ecs_pair_desc_t` in a world-owned numeric index.
`ecs_pair_find` searches that identity without allocating or registering it.
Elements carry an explicit namespace: entity handle, component ID, or relation
ID. Entity elements retain their generation. Pair identities have their own
ID space and do not allocate component IDs per target. Deleting either entity
element removes its pair rows and invalidates the identity. World restarts do
not reuse pair IDs.

A descriptor selects a native payload component. Its lifecycle and inheritance
policy apply to the pair column. Conflicting payload registrations return zero.
Builtin exclusive relations keep one authoritative edge representation and
cannot be pair elements in the relation namespace.

The API provides add/remove/has/get/set/modified. `ecs_pair_query_iter` returns
dense owned-row batches and supports exact identities, element wildcards,
payload filters, change ticks, and joins against ordinary source queries.
`ecs_pair_get` also resolves shared values along `IsA`; shared descendants do
not produce additional owned rows in pair iteration. Parallel systems enqueue
pair writes in their native worker command buffer.

`EcsOnPairSet` and `EcsOnPairRemove` carry `ecs_pair_event_t`. Batched events
retain owning payload snapshots. Removal is delivered after removing the row.
Live dynamic payload schema replacement resets retained rows and invalidates
borrowed value views. Native scene serialization includes pair identities and
values, remapping entity elements and reflected entity fields on load. The
current scene format is version 2; destination component/relation IDs and
schemas must match, as for ordinary scene columns.

Script transactions stage pair identities and payloads without publishing them
during preparation. Dependency reads, inherited reads, async waits, member
ownership, and instance cleanup accept the same pair keys. One-shot Script
evaluations also publish only after successful evaluation; failed preparation
releases staged owning values and declarations. Native host effects and allocator
exhaustion during publication are outside this rollback contract.

Pair access is not a term in the ordinary component query descriptor; pair
iteration uses its own descriptor and can join an ordinary source query.

## Verification scope

Core tests must verify multiple targets, both typed positions, exact values,
copy/move/destruction, inherited reads, notifications, deferred ownership,
target generation reuse, save/load, and repeated world lifetimes.

Measure ordinary component/relation access, creation/deletion, queries, and
deferred batches with identical compilers, dependencies, workloads, and repeated
samples before changing table layouts. Additional pair indices belong only to
tables that contain pairs. A Script test alone does not validate this extension.
