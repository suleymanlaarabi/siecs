# C17 API: pairs

[Usage and contracts](../references/pairs.md). Signatures only; consult the guide for behavior.

C++ uses this C ABI; there is no typed pair facade.

## Functions

- `ecs_pair_add` — `bool ecs_pair_add(ecs_entity_t entity, ecs_pair_id_t pair)`
- `ecs_pair_entity_in_use` — `bool ecs_pair_entity_in_use(ecs_entity_t entity)`
- `ecs_pair_find` — `ecs_pair_id_t ecs_pair_find(const ecs_pair_desc_t *desc)`
- `ecs_pair_get` — `const void *ecs_pair_get(ecs_entity_t entity, ecs_pair_id_t pair)`
- `ecs_pair_has` — `bool ecs_pair_has(ecs_entity_t entity, ecs_pair_id_t pair)`
- `ecs_pair_has_owned` — `bool ecs_pair_has_owned(ecs_entity_t entity, ecs_pair_id_t pair)`
- `ecs_pair_info` — `const ecs_pair_desc_t *ecs_pair_info(ecs_pair_id_t pair)`
- `ecs_pair_init` — `ecs_pair_id_t ecs_pair_init(const ecs_pair_desc_t *desc)`
- `ecs_pair_modified` — `bool ecs_pair_modified(ecs_entity_t entity, ecs_pair_id_t pair)`
- `ecs_pair_next` — `bool ecs_pair_next(ecs_pair_iter_t *it)`
- `ecs_pair_query_iter` — `ecs_pair_iter_t ecs_pair_query_iter(const ecs_pair_query_desc_t *desc)`
- `ecs_pair_remove` — `bool ecs_pair_remove(ecs_entity_t entity, ecs_pair_id_t pair)`
- `ecs_pair_set` — `bool ecs_pair_set(ecs_entity_t entity, ecs_pair_id_t pair, const void *value)`

## Types and members

- `ecs_pair_desc_t` — `typedef struct { /* fields below */ } ecs_pair_desc_t`
- `ecs_pair_desc_t.first` — `ecs_pair_element_t first`
- `ecs_pair_desc_t.payload` — `ecs_component_t payload`
- `ecs_pair_desc_t.second` — `ecs_pair_element_t second`
- `ecs_pair_element_t` — `typedef struct { /* fields below */ } ecs_pair_element_t`
- `ecs_pair_element_t.id` — `uint64_t id`
- `ecs_pair_element_t.kind` — `ecs_pair_kind_t kind`
- `ecs_pair_event_t` — `typedef struct { /* fields below */ } ecs_pair_event_t`
- `ecs_pair_event_t.pair` — `ecs_pair_id_t pair`
- `ecs_pair_event_t.payload` — `ecs_component_t payload`
- `ecs_pair_event_t.value` — `const void *value`
- `ecs_pair_id_t` — `typedef uint32_t ecs_pair_id_t`
- `ecs_pair_iter_t` — `typedef struct { /* fields below */ } ecs_pair_iter_t`
- `ecs_pair_iter_t.count` — `uint32_t count`
- `ecs_pair_iter_t.entities` — `const ecs_entity_t *entities`
- `ecs_pair_iter_t.pair` — `ecs_pair_id_t pair`
- `ecs_pair_iter_t.pair_cursor` — `uint32_t pair_cursor`
- `ecs_pair_iter_t.query` — `ecs_pair_query_desc_t query`
- `ecs_pair_iter_t.row_cursor` — `uint32_t row_cursor`
- `ecs_pair_iter_t.stride` — `size_t stride`
- `ecs_pair_iter_t.tick` — `uint64_t tick`
- `ecs_pair_iter_t.values` — `const void *values`
- `ecs_pair_kind_t` — `typedef enum { /* values below */ } ecs_pair_kind_t`
- `EcsPairAny` — `EcsPairAny`
- `EcsPairComponent` — `EcsPairComponent`
- `EcsPairEntity` — `EcsPairEntity`
- `EcsPairRelation` — `EcsPairRelation`
- `ecs_pair_query_desc_t` — `typedef struct { /* fields below */ } ecs_pair_query_desc_t`
- `ecs_pair_query_desc_t.after_tick` — `uint64_t after_tick`
- `ecs_pair_query_desc_t.filter_sources` — `bool filter_sources`
- `ecs_pair_query_desc_t.first` — `ecs_pair_element_t first`
- `ecs_pair_query_desc_t.pair` — `ecs_pair_id_t pair`
- `ecs_pair_query_desc_t.payload` — `ecs_component_t payload`
- `ecs_pair_query_desc_t.second` — `ecs_pair_element_t second`
- `ecs_pair_query_desc_t.sources` — `ecs_query_id_t sources`

## Macros

- `EcsOnPairRemove` — `EcsOnPairRemove 6`
- `EcsOnPairSet` — `EcsOnPairSet 5`
