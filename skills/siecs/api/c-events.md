# C17 API: events

[Usage and contracts](../references/c/events.md). Signatures only; consult the guide for behavior.

Every `.query` option uses the shared [C filter table](../references/c/filters.md).

## Functions

- `ecs_event` — `ecs_event_t ecs_event(void)`
- `ecs_event_register` — `ecs_event_t ecs_event_register(ecs_event_t *id)`
- `ecs_observer_disable` — `void ecs_observer_disable(ecs_observer_id_t id)`
- `ecs_observer_enable` — `void ecs_observer_enable(ecs_observer_id_t id)`
- `ecs_observer_fini` — `void ecs_observer_fini(ecs_observer_id_t id)`
- `ecs_observer_init` — `ecs_observer_id_t ecs_observer_init(const ecs_observer_desc_t *desc)`
- `ecs_observer_trigger` — `void ecs_observer_trigger(ecs_entity_t entity, ecs_event_t event, const void *trigger_data)`

## Types and members

- `ecs_event_t` — `typedef uint16_t ecs_event_t`
- `ecs_observer_callback_t` — `typedef void (*ecs_observer_callback_t)(ecs_observer_event_t *event)`
- `ecs_observer_desc_t` — `typedef struct { /* fields below */ } ecs_observer_desc_t`
- `ecs_observer_desc_t.callback` — `ecs_observer_callback_t callback`
- `ecs_observer_desc_t.entity` — `ecs_entity_t entity`
- `ecs_observer_desc_t.on` — `ecs_event_t on`
- `ecs_observer_desc_t.query` — `ecs_query_desc_t query`
- `ecs_observer_desc_t.user_data` — `uintptr_t user_data`
- `ecs_observer_desc_t.user_data_dtor` — `void (*user_data_dtor)(uintptr_t user_data)`
- `ecs_observer_event_t` — `typedef struct { /* fields below */ } ecs_observer_event_t`
- `ecs_observer_event_t.component` — `ecs_component_t component`
- `ecs_observer_event_t.entity` — `ecs_entity_t entity`
- `ecs_observer_event_t.event` — `ecs_event_t event`
- `ecs_observer_event_t.trigger_data` — `const void *trigger_data`
- `ecs_observer_event_t.user_data` — `uintptr_t user_data`
- `ecs_observer_id_t` — `typedef uint32_t ecs_observer_id_t`
- `ecs_relation_event_t` — `typedef struct { /* fields below */ } ecs_relation_event_t`
- `ecs_relation_event_t.new_target` — `ecs_entity_t new_target`
- `ecs_relation_event_t.old_target` — `ecs_entity_t old_target`
- `ecs_relation_event_t.relation` — `ecs_relation_id_t relation`

## Macros

Function-like macros show invocation shape, not their implementation.

- `EcsOnAdd` — `EcsOnAdd 0`
- `EcsOnRelationRemove` — `EcsOnRelationRemove 4`
- `EcsOnRelationSet` — `EcsOnRelationSet 3`
- `EcsOnRemove` — `EcsOnRemove 1`
- `EcsOnSet` — `EcsOnSet 2`
- `ecs_observer` — `ecs_observer(...)`
