# C: systems and phases

Systems use the entire [query/filter descriptor](filters.md): component access,
filter/exclusion/optional terms, resources, relation presence/absence/targets/depth,
ancestry, ancestor reads and table ordering. A tag filter does not declare access
to other component data fetched manually in the callback.

## Register entity work

```c
static void move(ecs_iter_t *it) {
    Position *p = ecs_field(it, 0);
    const Velocity *v = ecs_field(it, 1);
    bool shared_v = ecs_field_is_shared(it, 1);
    for (uint32_t row = 0; row < it->count; row++) {
        p[row].x += v[shared_v ? 0 : row].x * it->delta_time;
    }
}

/* After schema registration: */
ecs_system_id_t move_id = ecs_system({
    .name = "Move",
    .query = {
        .components = {
            ecs_filter(Moving), ecs_inout(Position), ecs_in(Velocity),
            ecs_not(Frozen),
        },
    },
    .callback = move,
    .phase = EcsOnUpdate,
});
```

`ecs_system_init(&desc)` is the non-macro form. The world owns the registration
until finalization; there is no per-system unregister function. Keep the name
string and borrowed user state alive for that lifetime.

## Every system descriptor option

| Field | Use / default when omitted |
| --- | --- |
| `.name` | Diagnostic string, borrowed |
| `.query` | Full descriptor; empty means one task invocation |
| `.callback` | Required `void(ecs_iter_t*)` |
| `.phase` | **C zero defaults to `EcsPreStart`**; specify `EcsOnUpdate` for recurring work |
| `.after = { id1, id2 }` | Up to 16 already registered systems in the same phase |
| `.interval` | Seconds, nonnegative; zero means every recurring frame |
| `.disabled` | Initially disabled if true; false by default |
| `.main_thread_only` | Thread-affine work; false by default |
| `.no_defer` | Immediate task-only mutations; false by default |
| `.callback_iterates_query` | Callback advances the iterator itself; false by default |
| `.user_data` | `uintptr_t` context, available as `it->user_data` |
| `.user_data_dtor` | Optional `void(uintptr_t)`, called once at world teardown |

Read declared resources through resource APIs; they never shift field indices.
For example `.query.resources = { ecs_in(Settings) }` pairs with
`ecs_get_resource_read(Settings)`; ensure storage exists first. For optional
components and targets, use the [C query field/batch rules](queries.md).

## Callback frequency and iterator ownership

With entity criteria (components, relations, `.is_a` or a comparator), the default
callback runs for each non-empty batch, already advanced: do **not** call
`ecs_iter_next` in it. Setting `.callback_iterates_query = true` instead passes
the initial iterator; that callback must loop `while (ecs_iter_next(it))` itself.
Use this for whole-query processing or early termination.

Empty/resource-only systems invoke the callback once with `.count = 1` and no
entity/field arrays. `callback_iterates_query` does not give those tasks an
iterable cache. Use resource APIs or task state, not `it->entities`/`ecs_field`.
Ordinary systems already defer structural edits for the whole invocation;
`no_defer` is rejected for entity queries.

## Phases, control and timing

```c
ecs_phase_t physics = ecs_phase({
    .name = "Physics", .after = EcsPreUpdate, .before = EcsOnUpdate,
});
```

Set **both** phase anchors. For a missing anchor use `ECS_PHASE_NONE`, not zero
(zero is `EcsPreStart`). Both NONE selects the default recurring insertion after
OnUpdate/before PostUpdate. Startup-anchored phases are one-shot; recurring
anchors must not cross that boundary.

`ecs_system_enable/disable(id)` preserves registration; changing enabled state
resets the interval accumulator. `ecs_run_system(id)` and `ecs_run_phase(phase)`
run enabled work now, ignoring intervals without resetting them. They do not
advance frame time or perform frame pacing. `ecs_system_name/ecs_phase_name`
inspect diagnostic names. `ecs_progress()` advances a frame; `ecs_run()` owns
the loop and finalizes afterward.

Positive intervals run once when accumulated frame time reaches the period,
then discard the remainder; no catch-up. `it->delta_time` is the accumulated
interval delta, while the `DeltaTime` resource is current frame time. Startup
and forced executions ignore interval gates. For explicit barriers, workers
and lifecycle details, use [scheduling contracts](../systems.md).

[API](../../api/c-systems.md)
