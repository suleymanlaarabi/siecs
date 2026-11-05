# Native typed pairs

Status: **draft, not implemented**.

The Script runtime currently supports exclusive native relations. A relation
retains one target per source; assigning another target replaces the edge.
`ChildOf` and `IsA` use their existing native storage and invariants.

Independent payloads for `(Position, Target)` and `(Target, Position)` require
a core extension. The evaluator reports an error for typed pair payloads.

## Proposed core contract

Pair elements carry an explicit namespace: entity handle, component ID, or
relation ID. Entity elements retain their generation. Intern a canonical pair
identity in a world-owned index; never allocate a component ID per target or
store pair values in the Script addon.

A pair descriptor selects one reflected native payload type. Conflicting type
registrations for the same identity must fail. Builtin exclusive relations
keep one authoritative edge representation.

The public contract must cover add/remove/has/get/set/modified, exact and
wildcard query terms, observers, inheritance, deferred operations, deletion
policies, and reflected scene serialization. Payload columns use native value
lifecycle operations and change ticks. Target reuse must invalidate stale
identities without affecting unrelated pairs.

## Required verification

Core tests must verify multiple targets, both typed positions, exact values,
copy/move/destruction, inherited reads, notifications, deferred ownership,
target generation reuse, save/load, and repeated world lifetimes.

Measure ordinary component/relation access, creation/deletion, queries, and
deferred batches with identical compilers, dependencies, workloads, and repeated
samples before changing table layouts. Additional pair indices belong only to
tables that contain pairs. A Script test alone does not validate this extension.
