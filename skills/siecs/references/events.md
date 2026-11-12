# Observers and events

Observers react synchronously to matching lifecycle/custom events. Query filters
select the emitting entity, not the component that triggered a lifecycle event.
Global/filtered observers apply query exclusions; unfiltered targeted observers
also receive events from disabled/abstract entities.
Use systems for recurring bulk processing rather than issuing an event per row.

| Event | Payload and timing |
| --- | --- |
| `OnAdd` | Added component storage after addition |
| `OnSet` | Incoming value for set; current storage for modified |
| `OnRemove` | Borrowed previous component data for the removal reaction |
| `OnRelationSet` | Relation, old target, new target; after add/retarget |
| `OnRelationRemove` | Relation, old target, zero new target; before removal |
| `OnPairSet` | Pair id, payload component and value; after a pair add/set/modified |
| `OnPairRemove` | Pair id, payload component and removed value; after the row is removed |
| Custom | Caller-supplied payload, borrowed for the callback |

Component hooks and observers differ: `on_set` hooks run before the final copy;
an observer reading existing component storage can see its previous value.
Read the incoming trigger payload when that distinction matters. Removal hooks
run before removal; do not assume removed storage remains available afterward.
Before casting a lifecycle payload, check the C event's `component` against the
expected id; tags have no data. Relation payloads also need a `relation` id check.

A targeted observer also receives events from direct/indirect `IsA` instances;
the event entity remains the concrete emitter. A targeted observer is removed
when its owner dies. Disable keeps registration; finalization removes it and
releases owned callback state once.

Deferred structural/set mutations emit at flush and can coalesce; explicit custom
emission and `modified` notify synchronously even inside a defer scope.
An observer batch delays every callback until the outermost batch ends. Builtin
event payloads are then owned snapshots, observers are matched against the
emitter's table at emission time, and callbacks read the final world when
delivered. Custom event payloads stay borrowed and must outlive the batch. An
observer removed before delivery receives nothing. A batch groups notifications
only; it is not a transaction and provides no rollback. A component observed in
a batch needs a copy operation: move-only owning types supply a dedicated
`snapshot` type operation. Pair events are described in [pairs](pairs.md).

Observer order is not a system pipeline. Keep callback data and captures alive,
avoid recursive event loops, and copy payloads if they must outlive a callback.
