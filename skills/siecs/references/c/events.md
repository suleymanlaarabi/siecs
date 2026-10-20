# C: observers and events

## Custom event, filtered observer, synchronous emission

```c
typedef struct { int amount; } Damage;
static ecs_event_t damage_event = UINT16_MAX;

static void on_damage(ecs_observer_event_t *event) {
    const Damage *damage = event->trigger_data;
    if (!damage) return;
    apply_damage(event->entity, damage->amount);
}

/* During setup, after registering Health and Invulnerable: */
ecs_event_register(&damage_event);
ecs_observer_id_t observer = ecs_observer({
    .on = damage_event,
    .query.components = { ecs_filter(Health), ecs_not(Invulnerable) },
    .callback = on_damage,
});
/* Later, for a live entity: */
Damage damage = { 7 };
ecs_observer_trigger(entity, damage_event, &damage);
```

`ecs_event()` allocates a fresh custom id without retained id storage.
`ecs_event_register(&id)` allocates when id is `UINT16_MAX`, or reserves/reuses
an existing id; zero is builtin `EcsOnAdd`. C provides no runtime payload-type
checking: all producers and observers of one id must agree on the payload.
Custom emission is synchronous, including inside a defer scope.

## Every observer descriptor field

| Field | Meaning |
| --- | --- |
| `.on` | One builtin/custom event id; zero means `EcsOnAdd` |
| `.entity` | Live target owner; omitted/zero means global |
| `.query` | Full entity filter descriptor, see [all filters](filters.md) |
| `.callback` | Required `void(ecs_observer_event_t*)` |
| `.user_data` | `uintptr_t` state, forwarded to the event |
| `.user_data_dtor` | Optional `void(uintptr_t)`, called once on removal/teardown |

`ecs_observer_init(&desc)` is the non-macro constructor. The event contains
`.entity` (concrete emitter), `.event` (event id), `.component` (triggering
component or zero), `.trigger_data` and `.user_data`. It is not an iterator:
read entity components/resources/targets through their APIs, not `ecs_field`.
Declare actual access on any enclosing scheduled system as well.

## Component lifecycle events

Builtin ids are `EcsOnAdd`, `EcsOnSet`, `EcsOnRemove`, `EcsOnRelationSet` and
`EcsOnRelationRemove`. Their [payload/timing table](../events.md) distinguishes
incoming values, current storage and removal state. Example for exact Health
changes (assuming Health has an `int value` field):

```c
static void on_health_set(ecs_observer_event_t *event) {
    if (event->component != ecs_id(Health) || !event->trigger_data) return;
    const Health *incoming = event->trigger_data;
    consume_health(event->entity, incoming->value);
}
/* Setup: */
ecs_observer({
    .on = EcsOnSet,
    .query.components = { ecs_filter(Health) },
    .callback = on_health_set,
});
```

The Health filter selects entity tables, **not** which component triggered the
event. Always check `.component` before casting. The same check handles tag
lifecycle, but tags have no payload storage: react to the id, not a value.
For `OnSet`, fetching Health from the entity can still return its previous
value; the payload supplies the incoming value. `ecs_modified` instead emits
current storage without calling the component's on_set hook.

## Relation lifecycle events

```c
static void on_group_change(ecs_observer_event_t *event) {
    const ecs_relation_event_t *edge = event->trigger_data;
    if (!edge || edge->relation != ecs_rid(GroupOf)) return;
    consume_edge(event->entity, edge->old_target, edge->new_target);
}
/* Setup; register a second observer for EcsOnRelationRemove if needed. */
ecs_observer({ .on = EcsOnRelationSet, .callback = on_group_change });
```

The relation payload is `.relation`, `.old_target`, `.new_target`; `.component`
is zero. RelationSet matches the new table/target; RelationRemove matches the
pre-removal state. To track leaving a group by retargeting, inspect old/new in
the payload rather than requiring the old target in the observer's query.

## Targeting, filters and lifetime

All component/tag, relation and `.is_a` filters can be supplied in `.query`.
They are ANDed against the emitter's event-time table. Query ordering does not
order delivery. A raw C ancestor term does not expose an ancestor field or
perform iterator-time ancestor presence resolution; resolve that value explicitly.

`.entity = owner` also receives events from its `IsA` descendants, not arbitrary
ChildOf children. The callback still identifies the concrete emitter. An
unfiltered target receives disabled/abstract emitters too; ordinary global or
entity-filtered observers apply default query exclusions.

`ecs_observer_enable/disable(id)` toggles registration;
`ecs_observer_fini(id)` removes it. Target-owner deletion removes its observers;
world teardown removes the rest. Do not use handles after removal. Borrowed
state/payloads must remain valid through callbacks; copy values for later work.
Callbacks run on the emitter's thread. Register observers/events in setup, and
schedule registration/removal outside active event delivery. Use an outer defer
scope around mutation-heavy producers; avoid recursive writes to the same event
source. For expensive reactions, record intent and let a system process it.

[Event semantics](../events.md) · [API](../../api/c-events.md)
