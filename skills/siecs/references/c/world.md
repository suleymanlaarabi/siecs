# C: build and world

With a source installation, include `<siecs.h>` and link the SIECS C runtime
and its public dependencies. Bake projects declare `siecs` in `value.use`;
run `bake build <project>` or `bake run <project>` from the checkout root.

For standalone use, copy `distr/siecs.h` and `distr/siecs.c` into the application.
Compile on Linux with `cc -std=c17 -I. main.c siecs.c -pthread -ldl -o app`.

```c
#include <siecs.h>

ECS_COMPONENT(Position, { float x; });
ECS_COMPONENT(Velocity, { float x; });

static void move(ecs_iter_t *it) {
    Position *p = ecs_field(it, 0);
    const Velocity *v = ecs_field(it, 1);
    for (uint32_t i = 0; i < it->count; i++) {
        p[i].x += v[i].x;
    }
}

int main(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(Position, Velocity);
    ecs_system({
        .name = "Move",
        .query.components = { ecs_inout(Position), ecs_in(Velocity) },
        .callback = move,
        .phase = EcsOnUpdate,
    });
    ecs_entity_t entity = ecs_new();
    ecs_set(entity, Position, { 0 });
    ecs_set(entity, Velocity, { 1 });
    ecs_run_phase(EcsOnUpdate);
    ecs_fini();
}
```

Use `ecs_init_w_features`/`ecs_with_features` for frame pacing and workers,
`ecs_at_fini` for owned teardown, and `ecs_defer_begin/end` for manual deferral.
Use `ecs_progress` in the host loop, then `ecs_fini`; request exit with `ecs_quit`.
Alternatively, `ecs_run` owns the loop and finalizes on exit; do not call
`ecs_fini` afterward.

## Features, host loops and teardown

```c
ecs_with_features({ .target_fps = 60, .worker_threads = ECS_WORKERS_AUTO });
/* Register/import schema, resources and systems. */
while (ecs_progress()) {
    /* Host work; request exit with ecs_quit(). */
}
ecs_fini();
```

This is an alternative to `ecs_init`, not another initialization in the same
world. `target_fps = 0` disables pacing; `worker_threads = 0` disables workers.
A numeric worker count adds that many workers beside the calling thread;
`ECS_WORKERS_AUTO` requests logical CPUs minus one. For non-macro initialization,
pass an `ecs_world_feat_desc_t*` to `ecs_init_w_features`.

`ecs_at_fini({ .callback = cleanup, .data = state })` registers a
`void cleanup(void*)` callback, or use `ecs_at_fini_init(&desc)`. The data pointer
is borrowed until the call; cleanup can release owned external state. Callbacks
run in reverse registration order after workers stop and before entity/resource
teardown. Do not register another fini callback from one. `ecs_world_current()`
returns the active opaque world; it is not a multi-world creation API.

[World semantics](../world.md) · [API](../../api/c-world.md)
