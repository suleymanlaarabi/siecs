# Inheritance and templates

`IsA` shares reusable defaults rather than creating an object class hierarchy.
Adding an `IsA` target makes that base abstract. Ordinary queries skip abstract
bases and focus on instances. `is` traverses `IsA` ancestry; compare entity ids
for identity. Pass live, nonzero handles: the current implementation does not
provide the identity/invalid-handle behavior claimed by its header comment.
The direct base accessor answers a different question.

Choose each component's policy at registration:

- `EcsInheritOwned` copies missing effective base components into instance-owned
  storage, preserving existing local values. Later base changes do not synchronize
  those copies.
- `EcsInheritShared` resolves read-only data from the base. A local override
  gives the instance owned storage. Mutable queries do not write shared storage.

On a new `IsA` target, SIECS recursively snapshots the base's owned `ChildOf`
subtree. Each generated child belongs to the generated parent and inherits from
its corresponding source child. Reapplying the same target is a no-op.
Later child-tree changes are not synchronized. Removing or retargeting `IsA`
does not delete descendants created by an earlier snapshot.

Explicit abstraction marks the entity and its owned subtree. Finish template
configuration before making it a base; application mutation of abstract bases is
restricted. To add a component through a hierarchy, use propagation only on an
acyclic relation that maintains a source index.
