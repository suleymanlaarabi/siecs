# C: selection and access terms

The same `ecs_query_desc_t` is used by standalone queries, `ecs_system_desc_t.query`
and `ecs_observer_desc_t.query`. Initialize it with `{0}` or designated fields.
Types and relation policies must already be registered.

## Component terms: `.components`

| Term | Match | Returned field / access |
| --- | --- | --- |
| `ecs_in(T)` | T present | Read; owned array or one shared inherited value |
| `ecs_out(T)` | T owned | Write; existing storage, not implicit creation |
| `ecs_inout(T)` | T owned | Read/write |
| `ecs_in_optional(T)` | No presence requirement | Nullable read; shared allowed |
| `ecs_inout_optional(T)` | No presence requirement | Nullable owned read/write |
| `ecs_filter(T)` | T present | No field and no declared data access |
| `ecs_not(T)` | T absent | No field |
| `ecs_up(T, R)` | An ancestor supplies T | Read the nearest effective T through R |
| `ecs_up_optional(T, R)` | Ancestor value optional | Nullable ancestor read |

`up` starts at the relation target, not the entity itself; R must be acyclic
`EcsRelationByTarget`. Read/write terms do not mutate shared base data.
Use filter terms for zero-sized tags, not data fields. `Disabled` and `Abstract`
are excluded by default; explicitly mention the desired tag to change that
exclusion, e.g. `ecs_filter(Disabled)` selects disabled entities.

Terms combine with AND. Declare each component once; direct and ancestor reads
of the same component cannot be two distinct fields in one descriptor. Value
predicates belong in the callback. For OR, use separate queries and account for
entities matching both; there is no OR/predicate-expression API.

Dynamic terms use `{ .id = cid, .access = EcsIn }`. Ancestor access encodes
`ECS_QUERY_UP_ACCESS(EcsInUp, rid)` or `EcsInUpOptional` in `.access`.

## Resources: `.resources`

Use `ecs_in(Resource)`, `ecs_out(Resource)` or `ecs_inout(Resource)` in this
separate array. Only these required access modes are valid. They describe
scheduler dependencies, not entity filters or iterator fields. Initialize
resource storage before callbacks read it; test expected absence explicitly
with resource APIs. There is no optional-resource query term.

## Relations: `.relations`

| Term | Selection | Storage constraint |
| --- | --- | --- |
| `ecs_rel(R)` | Has an R edge | Any supported relation |
| `ecs_rel_opt(R)` | Edge optional | Does not exclude entities without R |
| `ecs_not_rel(R)` | No R edge | Any supported relation |
| `ecs_to(R, target)` | Exact target, including generation | `ByTarget` |
| `ecs_depth(R, n)` | Maintained depth equals n | Acyclic `ByDepth` |

Choose one term per relation. Terms do not consume component fields; read targets
with the [batch/entity relation APIs](relations.md). An optional edge can be
absent; check the returned batch view/target. Raw terms use
`{ .id = rid, .target = target, .kind = EcsRelationTarget }`; the other kinds are
`EcsRelationRequired/Optional/Excluded/Depth`, with depth in `.target`.

## Ancestry, ordering and limits

| Descriptor field | Use |
| --- | --- |
| `.is_a = base` | Select `IsA` descendants of a live base |
| `.order_by = ecs_order_by_target(R)` | Order tables by a `ByTarget` target |
| `.order_by = ecs_order_by_depth(R)` | Order tables by a `ByDepth` depth |
| `.order_by = { compare, data }` | Custom table comparator and `uint64_t` context |

`compare(const ecs_table_t *a, const ecs_table_t *b, uint64_t data)` returns
negative/zero/positive. `ecs_table_has_id` inspects table presence and
`ecs_table_target(table, R)` reads a `ByTarget` target. Comparators must not mutate
the world or depend on changing row values: ordering is table-based, not a
per-frame sort of component values. Observers do not iterate this order.

Capacities are 16 component terms, 16 resource terms, 8 relation terms. Zero-fill
unused entries; id zero ends a list. Auto-excluded tags add no user-array entries.

[Iteration and fields](queries.md) · [Systems](systems.md) · [Events](events.md) · [Signatures](../../api/c-queries.md)
