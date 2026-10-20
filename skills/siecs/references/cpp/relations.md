# C++: relations and hierarchy

```cpp
struct GroupOf {};
ecs::relation<GroupOf>({
    .storage = EcsRelationByTarget,
    .on_delete_target = EcsRemoveRelation,
    .acyclic = false,
});
member.relate<GroupOf>(group);
ecs::query().with<GroupOf>(group)
    .each([](ecs::entity member, ecs::target<GroupOf> target) {
        use_membership(member, target);
    });
```

Register custom storage/policy before any typed access; otherwise lazy
registration selects `Dense`, and later options do not replace it.
`entity::target/has_relation/unrelate<Relation>()` access an edge.
`relate<Relation, Target>()` targets a typed singleton. `child_of(parent)` and
`parent.children(...)` compose owned hierarchy; `is_a` composes templates.

`target<Relation>` is an entity-derived callback parameter that handles per-row
and shared targets automatically. Relation filters, exact targets, depth, and
ordering belong to `query_filter`. Low-level source/batch inspection and default
relation installation use the shared C APIs with ids returned by
`ecs::relation<Relation>()` and `ecs::component<T>().id()`.

## Hierarchy, optional edges and defaults

| Entity operation | Meaning |
| --- | --- |
| `child.child_of(parent)` | Set/reparent ChildOf; target deletion cascades to children |
| `parent.children(a, b, ...)` | Attach the supplied existing entities; not a child iterator |
| `entity.relate<R>(target)` | Set/retarget one edge |
| `entity.relate<R, Target>()` | Target the named `singleton<Target>()` entity |
| `entity.target<R>()` | Non-owning target entity, zero if absent |
| `entity.has_relation<R>()` | Test edge presence |
| `entity.unrelate<R>()` | Remove the edge, preserving the source entity |

For exact target testing, compare target ids or call
`ecs_has_relation_to_id(entity.id(), ecs::relation<R>(), target.id())`.
For optional relation queries, use `.optional_relation<R>()` with an
`ecs::entity` callback, then test `entity.target<R>()`; `ecs::target<R>` itself
requires an edge.

```cpp
ecs_with_relation_id(ecs::component<Moving>().id(),
                     ecs::relation<GroupOf>(), group.id());
```

Install defaults before Moving participates in tables. Adding Moving applies
the default only without an existing/explicit edge; removing Moving does not
undo it. For child enumeration, use a required ChildOf query and inspect its
target, or `ecs_relation_sources(parent.id(), ecs::relation<ecs::ChildOf>())`.
ChildOf is ByDepth, so `.with<ecs::ChildOf>(parent)` is not a valid exact-target
filter. Shared C batch/source views follow [these lifetime rules](../c/relations.md#targets-in-query-batches).

[Storage, query constraints, and cleanup](../relations.md) · [API](../../api/cpp-relations.md)
