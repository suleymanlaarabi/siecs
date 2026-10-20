# C++20 API: world

[Usage and contracts](../references/cpp/world.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C world reference](c-world.md). Public base members also apply to derived builders.

## Functions

- `ecs::at_fini` — `template <typename F> inline void at_fini(F &&func)`
- `ecs::fini` — `inline void fini()`
- `ecs::init` — `inline void init()`
- `ecs::init` — `inline void init(const ecs_world_feat_desc_t &features)`
- `ecs::init_cpp_state` — `inline void init_cpp_state()`
- `ecs::progress` — `inline bool progress()`
- `ecs::quit` — `inline void quit()`
- `ecs::run` — `inline void run()`
- `ecs::run_phase` — `inline void run_phase(ecs_phase_t phase)`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_WORKERS_AUTO` — `ECS_WORKERS_AUTO UINT16_MAX`
- `ecs_at_fini` — `ecs_at_fini(...)`
- `ecs_with_features` — `ecs_with_features(...)`
