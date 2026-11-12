# Typed pairs

A pair is an interned `(first, second)` identity that stores one payload value
per source entity. Unlike a [relation](relations.md), which gives a source a
single target per kind, an entity can own many pairs sharing the same first
element, for example `(Likes, alice)` and `(Likes, bob)`. Use a relation for
hierarchy, ownership, cleanup policy and query terms; use pairs for many-to-many
facts that carry data.

This is a C ABI; C++ code calls the same functions and obtains component ids
with `ecs::component<T>().id()`. There is no typed C++ pair builder.

## Register an identity and store values

```c
ECS_COMPONENT(Affinity, { int value; });

/* Setup, after ECS_COMPONENT_REGISTER(Affinity): */
ecs_entity_t alice = ecs_new_named(0, "Alice");
ecs_pair_id_t likes_alice = ecs_pair_init(&(ecs_pair_desc_t){
    .first = { .id = ecs_id(Affinity), .kind = EcsPairComponent },
    .second = { .id = alice, .kind = EcsPairEntity },
    .payload = ecs_id(Affinity),
});
if (!likes_alice) { /* Conflicting payload or unsupported element. */ }

ecs_entity_t bob = ecs_new();
ecs_pair_set(bob, likes_alice, &(Affinity){ 7 });
const Affinity *affinity = ecs_pair_get(bob, likes_alice);
```

| Element kind | `id` holds |
| --- | --- |
| `EcsPairEntity` | Entity handle, generation included |
| `EcsPairComponent` | Component id |
| `EcsPairRelation` | Relation id; builtin exclusive relations such as `ChildOf` are rejected |
| `EcsPairAny` | Query wildcard only; never valid in a registered pair |

`ecs_pair_init` returns the existing id for an identical descriptor and zero on
failure. Element order matters: `(A, B)` and `(B, A)` are distinct pairs. The
payload is a registered component; its lifecycle operations and inheritance
policy apply to every row of the pair. Registering the same elements with a
different payload fails. `ecs_pair_find` looks an identity up without creating
it and ignores `.payload`; `ecs_pair_info` returns the borrowed descriptor, or
NULL for an id that is no longer live.

## Operations

| Function | Contract |
| --- | --- |
| `ecs_pair_add(entity, pair)` | Add a constructed (or zeroed) row if the entity owns none; existing rows are kept |
| `ecs_pair_set(entity, pair, &value)` | Add or replace the value; copies through the payload's operations |
| `ecs_pair_get(entity, pair)` | Borrowed const pointer; NULL when absent or the payload is zero-sized |
| `ecs_pair_has(entity, pair)` | Present on the entity or on an `IsA` ancestor |
| `ecs_pair_has_owned(entity, pair)` | The entity itself owns a row |
| `ecs_pair_modified(entity, pair)` | Mark an owned row changed and emit `EcsOnPairSet`; false when not owned |
| `ecs_pair_remove(entity, pair)` | Remove the owned row |
| `ecs_pair_entity_in_use(entity)` | Entity owns pair rows or is an element of a nonempty pair |

Mutators return false when they cannot apply, for example on a dead entity or
a pair whose element entity died. Inside a defer scope they queue: the value is
copied at the call, `ecs_pair_has` stays false until the flush, and a queued set
followed by a queued remove or kill leaves nothing behind. Pair mutations from
system callbacks publish with the system's other deferred commands.

Killing an entity that is a pair element removes every row of that pair and
retires the identity: `ecs_pair_info` returns NULL and later mutations return
false. A recycled entity index is a different element; register a new pair for
it. Pair ids do not survive `ecs_fini`.

`ecs_pair_get` and `ecs_pair_has` resolve through `IsA` ancestors, so a value
found that way belongs to the base. On a new `IsA` target, payloads whose
component policy is `EcsInheritOwned` are copied into rows the instance does not
already own; later base changes are not synchronized. `EcsInheritShared`
payloads stay on the base. Redefining a dynamic payload with
`ecs_component_dynamic_redefine` keeps rows present and zero-initializes them.

## Iterate rows

```c
ecs_pair_iter_t it = ecs_pair_query_iter(&(ecs_pair_query_desc_t){
    .first = { .id = ecs_id(Affinity), .kind = EcsPairComponent },
});
while (ecs_pair_next(&it)) {
    const Affinity *values = it.values;
    for (uint32_t row = 0; row < it.count; row++) {
        consume(it.entities[row], it.pair, values[row].value);
    }
}
```

| Descriptor field | Selection |
| --- | --- |
| `.first`, `.second` | Element filters; a zero id matches any id, `EcsPairAny` any kind |
| `.payload` | Only pairs with this payload component |
| `.pair` | One exact pair id |
| `.sources` + `.filter_sources = true` | Only source entities matching this query id |
| `.after_tick` | Only pairs changed after this tick |

All filters combine with AND; a NULL descriptor matches every pair. Each batch
is one pair with contiguous `entities` and `values` of `it.stride` bytes;
`values` is NULL for a zero-sized payload. Set `.filter_sources` whenever
`.sources` is used, because query id zero is valid. Read `it.pair` and
`ecs_pair_info` to learn which identity a wildcard batch belongs to.

For incremental processing, keep the `.tick` of the previous iterator and pass
it as `.after_tick` next time. The tick is tracked per pair, not per row: a
changed pair returns all of its matching rows. Any structural pair edit
invalidates iterators and value pointers; collect work first, then mutate.

## Observe

Register observers on `EcsOnPairSet` and `EcsOnPairRemove`. `trigger_data` is an
`ecs_pair_event_t` (`.pair`, `.payload`, `.value`) and `event->component` is
zero, so check `.pair` or `.payload` before casting `.value`.

```c
static void on_affinity(ecs_observer_event_t *event) {
    const ecs_pair_event_t *change = event->trigger_data;
    if (change->payload != ecs_id(Affinity)) return;
    const Affinity *value = change->value;
    consume(event->entity, change->pair, value->value);
}
/* Setup: */
ecs_observer({ .on = EcsOnPairSet, .callback = on_affinity });
```

Removal is delivered after the owned row is gone: read the removed value from
the event, not from the entity. Inside an
[observer batch](events.md) the value is an owning snapshot.

Scenes save and load pair rows. After loading, register the payload components
and call `ecs_pair_init` again with the loaded entities to obtain the pair ids.

[Relations](relations.md) · [Events](events.md) · [API](../api/c-pairs.md)
