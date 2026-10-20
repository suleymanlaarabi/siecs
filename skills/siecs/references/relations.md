# Relations and hierarchy

A relation kind connects each source to one target. Retargeting replaces that
edge; it does not add another pair. Register it in the separate relation
namespace with storage, target-deletion policy, and acyclic policy.

| Storage | Intended access |
| --- | --- |
| `EcsRelationDense` | Per-row targets with low retargeting overhead |
| `EcsRelationByDepth` | Per-row targets plus maintained depth |
| `EcsRelationByTarget` | Grouping by a shared target; ancestor lookup |

Choose storage before first use. Exact-target query filters require `ByTarget`;
depth filters require `ByDepth` and `.acyclic = true`. Use the corresponding
storage for meaningful target/depth ordering.
Ancestor reads require acyclic `ByTarget`. Source views are borrowed until
relation mutation; `ByTarget` and virtual built-ins return empty source views.
Batch views distinguish per-row targets from shared targets.

`EcsRemoveRelation` removes links when a target dies. `EcsDeleteSources` deletes
the sources, expressing ownership. `ChildOf` gives parent/child ownership and
naming scope; killing a parent deletes its owned subtree. `IsA` is inheritance,
not parent ownership. The same target can participate in both independently.

Use acyclic relations for ancestor/depth traversals; cycles violate those
contracts. Install component-triggered default relations before table use.
Generic relation APIs also support the built-ins, including their special
semantics. Use [inheritance](inheritance.md) for template instantiation.
