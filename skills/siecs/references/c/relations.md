# C: relations and hierarchy

```c
ECS_RELATION(GroupOf, {
    .storage = EcsRelationByTarget,
    .on_delete_target = EcsRemoveRelation,
});

/* During setup, before creating the edge: */
ECS_RELATION_REGISTER(GroupOf);
ecs_relate(member, GroupOf, group);
ecs_entity_t target = ecs_target(member, GroupOf);
```

Shared headers use `ECS_RELATION_DECLARE`, with one `DEFINE` in a source file.
For dynamic relations use `ecs_relation_init/register`; lookup/count/info expose
their registry. `ecs_rid` differs from a component id.

`ecs_has_relation/to`, `ecs_unrelate`, and their `_id` forms inspect/mutate
edges. `ecs_relation_sources(target, rid)` borrows indexed incoming sources.
`ecs_relation_batch` selects per-row or shared batch targets;
`ecs_targets` and `ecs_target_shared` expose the corresponding view.

Use `ecs_relate(child, ChildOf, parent)` for hierarchy and ownership.
`ecs_with_relation` installs a component's add-time default edge.

## Targets in query batches

After a successful `ecs_iter_next`, use a batch view to support every storage:

```c
ecs_relation_batch_t batch = ecs_relation_batch(&it, GroupOf);
for (uint32_t row = 0; row < it.count; row++) {
    ecs_entity_t target = batch.shared ? batch.shared_target
                                     : (batch.targets ? batch.targets[row].entity : 0);
    if (target) { /* Use the target for it.entities[row]. */ }
}
```

The query should contain `ecs_rel(GroupOf)` or `ecs_rel_opt(GroupOf)`.
For optional relations, zero/NULL means absent. `ecs_targets` is specifically
for Dense/ByDepth arrays; `ecs_target_shared` is for ByTarget. Neither consumes
a component field index. Outside query batches, use `ecs_target(entity, R)`.
In observers, use that entity API because there is no query iterator.

For indexed incoming sources, iterate `ecs_relation_sources(target, rid).entities`
up to `.count`. These views are borrowed and unavailable for ByTarget/virtual
built-ins. Defer structural edits while traversing a borrowed source view.
To find ByTarget members, query `ecs_to(R, target)` instead.

`ecs_with_relation(Moving, GroupOf, group)` declares a default before Moving
participates in tables. It preserves existing/explicit targets when Moving is
added. Target-deletion cleanup is governed by the relation's policy, not by the
component that installed the default. Removing the component does not undo the edge.

[Storage, query constraints, and cleanup](../relations.md) · [API](../../api/c-relations.md)
