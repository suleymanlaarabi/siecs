# C17 API: world

[Usage and contracts](../references/c/world.md). Signatures only; consult the guide for behavior.

## Functions

- `ecs_at_fini_init` — `void ecs_at_fini_init(const ecs_fini_desc_t *desc)`
- `ecs_defer_begin` — `void ecs_defer_begin(void)`
- `ecs_defer_end` — `void ecs_defer_end(void)`
- `ecs_fini` — `void ecs_fini(void)`
- `ecs_init` — `void ecs_init(void)`
- `ecs_init_w_features` — `void ecs_init_w_features(const ecs_world_feat_desc_t *features)`
- `ecs_progress` — `bool ecs_progress(void)`
- `ecs_quit` — `void ecs_quit(void)`
- `ecs_run` — `void ecs_run(void)`
- `ecs_world_current` — `ecs_world_t *ecs_world_current(void)`

## Types and members

- `ecs_fini_callback_t` — `typedef void (*ecs_fini_callback_t)(void *data)`
- `ecs_fini_desc_t` — `typedef struct { /* fields below */ } ecs_fini_desc_t`
- `ecs_fini_desc_t.callback` — `ecs_fini_callback_t callback`
- `ecs_fini_desc_t.data` — `void *data`
- `ecs_world_feat_desc_t` — `typedef struct { /* fields below */ } ecs_world_feat_desc_t`
- `ecs_world_feat_desc_t.target_fps` — `uint16_t target_fps`
- `ecs_world_feat_desc_t.worker_threads` — `uint16_t worker_threads`
- `ecs_world_t` — `typedef struct ecs_world_s ecs_world_t`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_WORKERS_AUTO` — `ECS_WORKERS_AUTO UINT16_MAX`
- `ecs_at_fini` — `ecs_at_fini(...)`
- `ecs_with_features` — `ecs_with_features(...)`
