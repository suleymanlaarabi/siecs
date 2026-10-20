# World and execution

SIECS has one active world. Initialize before registering or using ECS objects;
finalize once after application work stops. Handles, queries, and borrowed
metadata expire at finalization. Native C++ type-id caches are not all reset at
shutdown: do not assume a later world automatically re-registers those types.
Finalization callbacks and present resources are released in reverse registration
order. Do not initialize/finalize a second world to perform nested work.

`target_fps` controls frame pacing; `worker_threads` adds persistent workers
beside the calling thread. `ECS_WORKERS_AUTO` selects logical CPUs minus one.
Register schema before parallel execution. Worker callbacks must declare all
component/resource access and avoid unsynchronized external state.

`progress()` runs one frame, applies `target_fps` pacing, and returns false after
a quit request; the caller still owns finalization. `run()` loops on `progress()`
until quit, then calls `fini()` itself. Do not finalize again or keep owned queries
alive across `run()`. Forced phase/system execution ignores interval gating.
See [systems](systems.md) for ordering and barriers.

Deferral buffers component, naming, relation and destruction commands; the
outermost end flushes. Allocating an entity identity, direct field writes,
resource operations, `modified` and custom events remain immediate. A queued
component set is not an immediate storage update. Do not keep table pointers
across a flush. Deferred
commands can coalesce to a final state, so callbacks are not an operation log.
Manual C iteration needs a defer scope for structural changes; normal systems
and C++ query callbacks provide one.

Use nullable lookups for expected absence. APIs requiring registered types,
live handles, or present data have programmer contracts, often debug assertions;
release mode does not make violations valid.
