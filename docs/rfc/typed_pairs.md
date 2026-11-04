# Native typed pairs for Flecs Script parity

Status: **draft, not implemented**. This records a confirmed host limitation
before any world evaluator is ported. It is not an API stability or performance
claim. The complete upstream pair-test extraction and benchmark gate remain
open; this RFC does not complete Phase 1.

## Evidence at the frozen baseline

SiECS: `c41a4a220e2d6fabfa1c3cf7c0461647c5c765fd`.

Flecs: `fa788c60c87b4effc1124027db86ef0f30a778d4`.

The oracle exercises both typed positions successfully in AST and IR modes:

```flecs
struct Position(x: f32, y: f32)
Target {}
e {
  Position: {1, 2}
  (Position, Target): {3, 4}
  (Target, Position): {5, 6}
}
```

It also retains both `(Likes, Pizza)` and `(Likes, Salad)` on the same entity.
The automated oracle checks assert the exact target and payload of each pair.

Upstream examples, not an exhaustive feature list:

| Upstream testcase | Required capability | Current native representation |
| --- | --- | --- |
| `Eval.multiple_pairs_single_line` | Several targets for the same first element | BLOCKED_CORE: all three ordinary relation storages retarget a single edge |
| `Eval.assign_pair_component` | Component in first position, independent pair payload | BLOCKED_CORE: components and relations have distinct IDs; no pair value API |
| `Eval.with_pair_component_in_scope` | Pair data authored through a scope | BLOCKED_CORE: application payload storage is missing |
| `Eval.pair_w_rel_var` | First element selected by an expression | Native runtime relation IDs work; resolving script entity/component roles is still TODO |
| `Eval.pair_w_tgt_var` | Target selected by an expression | Native entity targets work; full language semantics are still TODO |
| `Eval.partial_assign_to_existing_pair` | Preserve unassigned pair members | BLOCKED_CORE: pair payload get/set/modified is missing |
| `Reactivity.conditional_pair_w_named_initializer_toggles` | Pair lifecycle and reactive ownership | BLOCKED_CORE: payload lifecycle/events are missing |

The exact testcase definitions, parameters, source locations and suite blobs
are in `test/compat/flecs_manifest.json`. The table above must not replace
the required complete pair audit or mark any compatibility testcase PASS.

The direct C probe `script_host.single_target_relations_and_dynamic_ids`
registers Dense, ByDepth and ByTarget relations and invokes the same public
operations for each. Setting Pizza then Salad removes the Pizza edge in every
case. Each operation checks presence, absence, exact target and cleanup.
This confirms the current single-target contract; it is not a workaround for
Flecs multi-target pairs.

Relevant native implementation:

- `include/siecs.h`: `ecs_relation_desc_t` has storage, deletion policy and
  acyclic fields; no payload descriptor. `ecs_relate_id` explicitly adds or
  retargets one edge. `ecs_target_id` returns one entity.
- `src/relation.c`: `ecs_relation_default_set_now` replaces the current target
  for Dense, ByDepth and ByTarget.
- `src/type.c` and `src/type.h`: existing archetype key/value pairs encode
  exclusive relation targets or depth. They do not describe application data
  for an arbitrary `(first, second)` identity.
- `src/command_buffer.h`: queued relation operations hold one target per
  relation ID. Extending only immediate mutations would be insufficient.

## Proposed native contract

Keep `ecs_relate_id`, `ecs_target_id`, ChildOf, IsA and their existing exclusive
relation storages and semantics. Add a native pair facility for identities and
values that those APIs cannot represent. Do not put pair values in the Script
addon or allocate one ordinary component ID per concrete target.

Use an explicitly typed pair element: entity handle, component ID or relation
ID. Entity elements retain the full generation. Component and relation elements
retain their own namespace. Never infer a namespace from the numeric value of
an ID. This is needed for `(Start, Position)` as well as `(Position, Target)`.

Intern canonical native pair descriptors in a world-owned core index:

```text
typed first + typed second -> native pair ID
native pair ID -> typed elements + optional payload component/type
```

Use a separate pair ID space large enough for 100k+ instantiated pairs; the
ordinary component namespace is 16-bit. The payload type follows the upstream
type-selection rules, including a sized first element, a sized second element
when the first has no payload, and tag-only overrides. Confirm those rules
against all upstream pair tests before freezing public signatures. Contradictory
payload registration for an already interned identity must fail, never create
two interpretations of the same pair.

Expose generic add/remove/has/get/set/modified operations for a native pair ID,
and typed query/observer terms for exact IDs and required wildcard forms.
Scripts resolve static pairs once and retain IDs in AST/IR bindings. Dynamic
expressions use the same native index; they do not create a private registry.

The existing exclusive relation APIs remain the path for ordinary ChildOf,
IsA and application relations. If a new pair operation addresses one of these
builtins, it must route to its existing native storage and invariants rather
than create duplicate edges. This is storage integration, not a deprecated API
adapter. The backend must have one authoritative representation for each edge.

## Storage, lifecycle and change detection

Concrete data pairs need ordinary native archetype columns and native posting
indices for queries. Pair metadata must not consume an ordinary component ID
per target or overflow existing 16-bit type keys. Introduce pair column/type
metadata only on tables that contain pairs. The precise table layout must be
chosen after measuring memory and ordinary component/relation hot paths.

Reuse component type metadata, `src/value.h` lifecycle transfer and column
change ticks. A pair column invokes the selected payload component's
ctor/copy/move/dtor operations through the same native value machinery. Pair
metadata owns a stable reference to its payload description; borrowed pointers
must survive registry growth or be resolved by ID before use.

Required behavior:

- Independent values for different targets and different first elements.
- Add, set and remove events with the concrete entity and pair identity.
- `modified` notification for direct writes, updating the normal change clock.
- Queries and observers see the same bytes returned by native pair get.
- Deferred commands retain typed identity, payload ownership and lifecycle
  operations; flush must not collapse independent pairs into one relation.
- Entity deletion, target deletion and generation reuse invalidate only
  relevant pairs. Define and test each deletion policy explicitly.
- Save/load uses the reflected payload codec and typed identity metadata.
  File format changes can replace the current format directly; no legacy path.
- World teardown destroys values exactly once and clears all pair indices.

Do not add allocations or pair-index lookups to marking/enqueuing a steady
state reactive event. Pair observers must dispatch through native exact indices
and the Script dependency buckets.

## Remaining acceptance work before core changes

1. Extract every pair-related upstream case, including cases whose names do
   not contain `pair`; classify tag, target, typed first, value and dynamic
   forms from actual source and setup. Keep unclassified cases visible.
2. Specify the native public descriptor, wildcard rules, type-selection rules,
   inherited access, lifecycle, deletion, defer, query and observer contracts.
3. Establish reproducible core benchmarks. This checkout's Makefile advertises
   `bench` targets, but there is no tracked `bench/project.json` or benchmark
   source in the baseline; those targets do not establish a runnable baseline.
4. Record compiler, dependency revisions, CPU/workload and repeated samples.
   Compare ordinary relations, component get/set, create/delete, queries and
   deferred batches before/after, without the Script addon. Investigate any
   persistent regression above roughly 2%.
5. Add core tests for exact payload values, multiple targets, both type
   positions, copy/move/destruction, inherited access, observers, ticks,
   deferred operations, target reuse and save/load. No addon is required for
   these tests.
6. Rebuild dependents after layout changes, regenerate distribution artifacts
   and run C/C++, standalone distribution and WASM checks.

## Related host audit

These observations determine Phase 1 work and avoid duplicate implementations.

| Capability | Finding at the inspected checkout | Next action |
| --- | --- | --- |
| Component/relation lookup | Already hashed with `sicore_map_t` in `src/storage/component_index.c` and `src/relation.c` | Reuse; do not add another name index |
| Explicit modification | `ecs_modified_cid` already exists and uses normal notifications/ticks | Reuse; no new script-specific version clock |
| Dynamic struct | Native `ecs_component_dynamic_init` calls sireflect runtime registration | Direct C probe checks the real component/type and typed bytes |
| Dynamic enum | Installed sireflect exposes `sireflect_try_register_enum` and constant lookups | Direct C probe verifies runtime descriptor and both lookup directions |
| Bitmask | The inspected public kinds have enum, no bitmask category; enum validation rejects undeclared combinations | Audit/extend sireflect semantics, not a Script enum store |
| Registry lifecycle | No registration generation or lifecycle event in the component/relation indices | Add a generic core notification contract after auditing reentrant registration |
| Targeted observers | Native entity/event index and owned callback destructor exist | Reuse; insertion/removal shifts sorted vectors, so measure subscription churn |
| Naming | Name set/remove and ChildOf events are present | Still test unresolved-path wakeups for new anonymous entities, rename, reparent and delete; no completeness claim yet |
| Deferred flush | `ecs_defer_end` flushes queued commands; no public post-batch reaction hook | Specify generic safe flush integration before reactive execution |

Installed dependency sources resolved through `bake env` and package metadata:

| Dependency | Revision | Working tree |
| --- | --- | --- |
| sicore | `5204b921e9985d1a8e67fe1db6c0195c34cd8d2c` | clean |
| sireflect | `e2a0bd27c9e783159b7dfe71fc92eff4663fcedb` | production files clean; local untracked AGENTS.md preserved |
| sijson | `1d72fc9170d9dac04112d4f5149a6b7b47e0a7d2` | production files clean; local untracked AGENTS.md preserved |

The installed sireflect checkout has no local `v0.6.0` tag. These findings are
about the resolved revision above, not a claim that a tag was verified. Its
enum implementation is `src/sireflect_enum.c`; the suggested `src/runtime.c`
and `src/meta.c` paths from the plan do not exist in this checkout.
