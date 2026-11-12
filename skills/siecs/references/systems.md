# Systems and scheduling

Use systems for recurring transformations. Query terms are both selection and
scheduler access contracts. Read/read can overlap; writes and overlapping table
access constrain scheduling. Resource dependencies matter even without entity
terms. Hidden global access is outside this analysis.

`progress` uses this phase order:

| Scheduling | Phases in order |
| --- | --- |
| Once, on the first progress | `EcsPreStart`, `EcsStart`, `EcsPostStart` |
| Each recurring frame | `EcsOnLoad`, `EcsPostLoad`, `EcsPreUpdate`, `EcsOnUpdate`, `EcsPostUpdate`, `EcsPreRender`, `EcsOnRender`, `EcsPostRender` |

Custom phases declare optional before/after anchors in the same startup/recurring
region. `ECS_PHASE_NONE` is zero: an omitted anchor or system phase never names
a real phase, and builtin phase ids start at one. A system without a phase runs
in `EcsOnUpdate`; a custom phase without anchors runs between `EcsOnUpdate` and
`EcsPostUpdate`. Same-phase system
`after` dependencies establish ordering and barriers. Dependencies must refer to
already registered systems in that phase; module import order is not a barrier.

Ready systems can run concurrently when workers are enabled, without a
happens-before relationship. A dependency waits for all portions of its
predecessors and for their deferred commands. Commands publish on the calling
thread in system registration order, then query table/portion order. Use
`main_thread_only` for thread-affine or global APIs; it runs exclusively on the
calling thread.

A system without `parallel` has at most one active callback. `parallel` opts in
to concurrent callbacks over disjoint portions of one table, at most `chunk_size`
rows each (zero selects 1024). It requires an unordered entity query and
read-only resources, and excludes `main_thread_only` and `no_defer`. Captures,
user data, lifecycle callbacks and external state must then support concurrent
invocation. Global registration, query/observer configuration, serialization
and nested scheduling are forbidden inside parallel callbacks. An entity created
there has its handle reserved immediately but becomes query-visible only when
the scheduler publishes commands; reserved handles can vary between runs.
Component conflicts need overlapping matching tables; resource conflicts are
world-wide. Filters alone do not declare component data access. Observer/nested
helper accesses must be declared on the enclosing system, including resources.
`no_defer` is for tasks without entity query iteration;
it is not a shortcut for mutating tables during iteration.

Entity systems process matching batches/rows. Resource-only and empty tasks run
once. The world owns registrations and callback state until finalization;
enable/disable preserves registration.

An interval of zero runs each recurring frame. Positive intervals accumulate
frame time, execute once when due, then reset without catch-up. Startup phases
ignore intervals. Forced phase/system runs ignore intervals without changing
accumulation. For interval callbacks the iterator delta is accumulated time;
`DeltaTime` remains the current frame delta. There are no rate/tick-source APIs.
The first progress has zero frame delta. Forced runs reuse existing time; they
neither measure a new frame nor perform frame pacing. Changing enabled state
resets interval accumulation. A disabled system remains disabled even in a
forced phase/system run.
