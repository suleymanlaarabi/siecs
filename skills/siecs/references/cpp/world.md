# C++: build and world

Source-layout applications include `<siecs/cpp.hpp>` and link the C runtime.
Bake projects use C++20 and declare `siecs` in `value.use`.

Standalone applications include `<siecs.h>` instead: it embeds the C++ facade.
Compile the runtime as C, then link with C++20 on Linux:

```sh
cc -std=c17 -I. -c siecs.c -o siecs.o -pthread
c++ -std=c++20 -I. main.cpp siecs.o -pthread -ldl -o app
```

This complete program uses the standalone header:

```cpp
#include <siecs.h>

struct Position { float x; };
struct Velocity { float x; };

int main() {
    ecs::init();
    ecs::system("Move").each([](Position &p, const Velocity &v) {
        p.x += v.x;
    });
    ecs::spawn().set(Position{0}, Velocity{1});
    ecs::run_phase(EcsOnUpdate);
    ecs::fini();
}
```

`ecs::init(features)` accepts `ecs_world_feat_desc_t`. `ecs::at_fini` owns its
callable. `ecs::progress()` runs one frame; a host loop must finalize afterward.
`ecs::run()` owns the loop and finalizes on quit; do not finalize again.
Scope owned queries so their destructors run **before** `ecs::fini()`;
use a host loop when local query owners need to survive across frames.
For explicit manual deferral, use the shared `ecs_defer_begin/end` C API;
ordinary C++ query callbacks and entity systems already defer.

## Configure workers, pacing and owned teardown

```cpp
ecs::init({ .target_fps = 60, .worker_threads = ECS_WORKERS_AUTO });
{
    auto positions = ecs::query<Position&>().build();
    while (ecs::progress()) {
        positions.each([](Position &p) { /* Host-side batch work. */ (void)p; });
        /* Request termination with ecs::quit() when the host is done. */
    }
} // Query destructor precedes finalization.
ecs::fini();
```

This replaces the earlier initialization example. Zero target FPS disables
pacing; zero workers disables worker dispatch. An explicit worker count is in
addition to the calling thread; AUTO requests logical CPUs minus one.
`ecs::at_fini(callable)` owns its callable/captures and invokes it after worker
shutdown, before entity/resource teardown, in reverse registration order.
This is suitable for early release of query owners stored in longer-lived
state: resource destructors run after query-registry teardown, so don't leave
live `query_handle` owners for those destructors.

Native type-id caches are not all reset by shutdown; use one native C++ world
lifetime rather than assuming reinitialization repeats every typed registration.
The C ABI provides an opaque `ecs_world_current()` for custom parameters, not
independent worlds. Avoid `ecs::run()` while local query owners are still alive:
it finalizes before those local destructors.

[World semantics](../world.md) · [API](../../api/cpp-world.md)
