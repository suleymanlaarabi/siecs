# C17 API: systems

[Usage and contracts](../references/c/systems.md). Signatures only; consult the guide for behavior.

Every `.query` option uses the shared [C filter table](../references/c/filters.md).

## Functions

- `ecs_phase_init` — `ecs_phase_t ecs_phase_init(const ecs_phase_desc_t *desc)`
- `ecs_phase_name` — `const char *ecs_phase_name(ecs_phase_t phase)`
- `ecs_run_phase` — `void ecs_run_phase(ecs_phase_t phase)`
- `ecs_run_system` — `void ecs_run_system(ecs_system_id_t system)`
- `ecs_system_disable` — `void ecs_system_disable(ecs_system_id_t system)`
- `ecs_system_enable` — `void ecs_system_enable(ecs_system_id_t system)`
- `ecs_system_init` — `ecs_system_id_t ecs_system_init(const ecs_system_desc_t *desc)`
- `ecs_system_name` — `const char *ecs_system_name(ecs_system_id_t system)`

## Types and members

- `EcsOnLoad` — `EcsOnLoad = 3`
- `EcsOnRender` — `EcsOnRender = 9`
- `EcsOnUpdate` — `EcsOnUpdate = 6`
- `EcsPhaseCount` — `EcsPhaseCount = 11`
- `EcsPostLoad` — `EcsPostLoad = 4`
- `EcsPostRender` — `EcsPostRender = 10`
- `EcsPostStart` — `EcsPostStart = 2`
- `EcsPostUpdate` — `EcsPostUpdate = 7`
- `EcsPreRender` — `EcsPreRender = 8`
- `EcsPreStart` — `EcsPreStart = 0`
- `EcsPreUpdate` — `EcsPreUpdate = 5`
- `EcsStart` — `EcsStart = 1`
- `ecs_phase_desc_t` — `typedef struct { /* fields below */ } ecs_phase_desc_t`
- `ecs_phase_desc_t.after` — `ecs_phase_t after`
- `ecs_phase_desc_t.before` — `ecs_phase_t before`
- `ecs_phase_desc_t.name` — `const char *name`
- `ecs_phase_t` — `typedef uint32_t ecs_phase_t`
- `ecs_system_desc_t` — `typedef struct { /* fields below */ } ecs_system_desc_t`
- `ecs_system_desc_t.after` — `ecs_system_id_t after[ECS_SYSTEM_AFTER_CAPACITY]`
- `ecs_system_desc_t.callback` — `void (*callback)(ecs_iter_t *)`
- `ecs_system_desc_t.callback_iterates_query` — `bool callback_iterates_query`
- `ecs_system_desc_t.disabled` — `bool disabled`
- `ecs_system_desc_t.interval` — `double interval`
- `ecs_system_desc_t.main_thread_only` — `bool main_thread_only`
- `ecs_system_desc_t.name` — `const char *name`
- `ecs_system_desc_t.no_defer` — `bool no_defer`
- `ecs_system_desc_t.phase` — `ecs_phase_t phase`
- `ecs_system_desc_t.query` — `ecs_query_desc_t query`
- `ecs_system_desc_t.user_data` — `uintptr_t user_data`
- `ecs_system_desc_t.user_data_dtor` — `void (*user_data_dtor)(uintptr_t user_data)`
- `ecs_system_id_t` — `typedef uint16_t ecs_system_id_t`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_PHASE_NONE` — `ECS_PHASE_NONE ((ecs_phase_t) - 1)`
- `ECS_SYSTEM_AFTER_CAPACITY` — `ECS_SYSTEM_AFTER_CAPACITY 16`
- `ecs_phase` — `ecs_phase(...)`
- `ecs_system` — `ecs_system(...)`
