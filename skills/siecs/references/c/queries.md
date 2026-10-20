# C: queries

For every component/resource/relation term, ancestry and ordering option, use
the [filter table](filters.md). The descriptor is also the `.query` field of
systems and observers; no separate system-filter API is needed.

## Create, count, iterate, free

```c
ecs_query_id_t query = ecs_query({
    .components = {
        ecs_filter(Moving), ecs_inout(Position), ecs_in(Velocity),
        ecs_in_optional(Mass), ecs_not(Frozen),
    },
});
ecs_iter_t it = ecs_query_iter(query);
while (ecs_iter_next(&it)) {
    Position *p = ecs_field(&it, 0);
    const Velocity *v = ecs_field(&it, 1);
    const Mass *mass = ecs_field(&it, 2);
    bool shared_v = ecs_field_is_shared(&it, 1);
    bool shared_mass = ecs_field_is_shared(&it, 2);
    for (uint32_t i = 0; i < it.count; i++) {
        const Velocity *velocity = &v[shared_v ? 0 : i];
        float scale = mass ? mass[shared_mass ? 0 : i].value : 1.0f;
        p[i].x += velocity->x * scale;
    }
}
ecs_query_fini(query);
```

The example assumes registered types (`Mass` has a float `value` field).
`ecs_query_init(&desc)` is the non-macro constructor. Retain the id for repeated
work; each new `ecs_query_iter(id)` restarts iteration over current matches.
`ecs_query_count(id)` counts entities, not tables. `ecs_query_fini(id)` destroys
the query; do not reuse the id afterward. Create/free queries outside parallel
system execution, and before world finalization.
Raw query id zero can be valid; do not test query ids as booleans.

## Fields

Field indices count only returned component terms in declaration order, starting
at zero. Filters, exclusions, resources and relation terms consume no fields.
`it.entities[i]` identifies a row and `it.count` is its batch size.

| `ecs_field_kind(&it, index)` | Pointer from `ecs_field` |
| --- | --- |
| `EcsFieldOwned` | Array of `it.count` values |
| `EcsFieldShared` | One read-only inherited/ancestor value, index zero |
| `EcsFieldNone` | NULL for an absent optional value |

Do not cast shared storage into writable memory. Required writable terms select
owned storage; optional writable terms can be absent despite a shared value.
`EcsOut` still requires presence and does not add a component. Field pointers,
entity arrays and relation views are borrowed for the current batch only.

## Structural edits and early exit

For an existing live query id, defer around the whole iteration:

```c
ecs_defer_begin();
ecs_iter_t it = ecs_query_iter(query);
while (ecs_iter_next(&it)) {
    for (uint32_t row = 0; row < it.count; row++) {
        ecs_remove(it.entities[row], Moving);
    }
}
ecs_defer_end(); /* Flush only after iteration has ended. */
```

Plain field writes need no defer scope, but they do not emit `OnSet`; use explicit
notification when required. Never flush, register new types or migrate storage
mid-iteration. A persistent query can be stopped early without destroying it:
leave the loop, close the defer scope, and later start a new iterator.

`ecs_query_each(it, row, terms...)` creates a temporary component-only query and
exposes fields plus row index. `ecs_query_entities(entity, terms...)` exposes
entity ids. Both are nested-loop macros: `break` exits only their innermost loop;
`return`/outward `goto` bypasses query cleanup. Prefer explicit iteration for
early exit, relation/resource descriptors or ownership-sensitive code.
The current macros also use zero as their loop sentinel, so a first query id
of zero skips their body; explicit iterator loops avoid that limitation.

Empty/filter-only queries are valid and provide `it.entities` without component
fields. A standalone query with only resource terms still visits ordinary entity
tables; it is not a task that runs once. For relation targets, use the
[storage-independent batch pattern](relations.md#targets-in-query-batches).

[Shared query contracts](../queries.md) · [API](../../api/c-queries.md)
