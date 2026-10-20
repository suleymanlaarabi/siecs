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

Custom phases declare before/after anchors in the same startup/recurring region.
`ECS_PHASE_NONE` means no anchor; zero is a real startup phase. Same-phase system
`after` dependencies establish ordering and barriers. Dependencies must refer to
already registered systems in that phase; module import order is not a barrier.

Independent systems can run concurrently when workers are enabled. Their
structural commands flush at synchronization boundaries. Use `main_thread_only`
for thread-affine APIs; these systems also isolate their scheduler batch.
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
