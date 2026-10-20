# C17 API: queries

[Usage and contracts](../references/c/queries.md). Signatures only; consult the guide for behavior.

## Functions

- `ecs_field` — `static inline void *ecs_field(ecs_iter_t *it, uint16_t field_index)`
- `ecs_field_is_shared` — `static inline bool ecs_field_is_shared(ecs_iter_t *it, uint16_t field_index)`
- `ecs_field_kind` — `static inline ecs_field_kind_t ecs_field_kind(const ecs_iter_t *it, uint16_t field_index)`
- `ecs_iter_next` — `bool ecs_iter_next(ecs_iter_t *it)`
- `ecs_order_by_depth_id` — `ecs_query_order_t ecs_order_by_depth_id(ecs_relation_id_t relation)`
- `ecs_order_by_target_id` — `ecs_query_order_t ecs_order_by_target_id(ecs_relation_id_t relation)`
- `ecs_query_count` — `uint32_t ecs_query_count(ecs_query_id_t query_id)`
- `ecs_query_fini` — `void ecs_query_fini(ecs_query_id_t qid)`
- `ecs_query_init` — `ecs_query_id_t ecs_query_init(const ecs_query_desc_t *query)`
- `ecs_query_iter` — `ecs_iter_t ecs_query_iter(ecs_query_id_t query_id)`
- `ecs_table_has_id` — `bool ecs_table_has_id(const ecs_table_t *table, ecs_component_t component)`
- `ecs_table_target_id` — `ecs_entity_t ecs_table_target_id(const ecs_table_t *table, ecs_relation_id_t relation)`

## Types and members

Iterator bookkeeping fields are listed because the struct is exposed. Use the
iterator/field helpers; do not mutate its cache or internal row/table state.

- `ecs_access_t` — `typedef enum { /* values below */ } ecs_access_t`
- `EcsFilter` — `EcsFilter`
- `EcsIn` — `EcsIn`
- `EcsInOptional` — `EcsInOptional`
- `EcsInOut` — `EcsInOut`
- `EcsInOutOptional` — `EcsInOutOptional`
- `EcsInUp` — `EcsInUp`
- `EcsInUpOptional` — `EcsInUpOptional`
- `EcsNot` — `EcsNot`
- `EcsOut` — `EcsOut`
- `ecs_access_term_t` — `typedef struct { /* fields below */ } ecs_access_term_t`
- `ecs_access_term_t.access` — `uint32_t access`
- `ecs_access_term_t.id` — `uint16_t id`
- `ecs_component_term_t` — `typedef ecs_access_term_t ecs_component_term_t`
- `ecs_field_kind_t` — `typedef enum { /* values below */ } ecs_field_kind_t`
- `EcsFieldNone` — `EcsFieldNone`
- `EcsFieldOwned` — `EcsFieldOwned`
- `EcsFieldShared` — `EcsFieldShared`
- `ecs_iter_t` — `typedef struct { /* fields below */ } ecs_iter_t`
- `ecs_iter_t.cache` — `struct ecs_query_cache_s *cache`
- `ecs_iter_t.count` — `uint32_t count`
- `ecs_iter_t.delta_time` — `float delta_time`
- `ecs_iter_t.entities` — `ecs_entity_t *entities`
- `ecs_iter_t.field_kind_bits` — `uint32_t field_kind_bits`
- `ecs_iter_t.ptrs` — `void **ptrs`
- `ecs_iter_t.table_count` — `uint16_t table_count`
- `ecs_iter_t.table_idx` — `uint16_t table_idx`
- `ecs_iter_t.user_data` — `uintptr_t user_data`
- `ecs_query_desc_t` — `typedef struct { /* fields below */ } ecs_query_desc_t`
- `ecs_query_desc_t.components` — `ecs_component_term_t components[ECS_QUERY_TERM_CAPACITY]`
- `ecs_query_desc_t.is_a` — `ecs_entity_t is_a`
- `ecs_query_desc_t.order_by` — `ecs_query_order_t order_by`
- `ecs_query_desc_t.relations` — `ecs_query_relation_term_t relations[ECS_QUERY_RELATION_CAPACITY]`
- `ecs_query_desc_t.resources` — `ecs_resource_term_t resources[ECS_QUERY_RESOURCE_CAPACITY]`
- `ecs_query_id_t` — `typedef uint16_t ecs_query_id_t`
- `ecs_query_order_func_t` — `typedef int (*ecs_query_order_func_t)(const ecs_table_t *a, const ecs_table_t *b, uint64_t data)`
- `ecs_query_order_t` — `typedef struct { /* fields below */ } ecs_query_order_t`
- `ecs_query_order_t.data` — `uint64_t data`
- `ecs_query_order_t.func` — `ecs_query_order_func_t func`
- `ecs_query_relation_kind_t` — `typedef enum { /* values below */ } ecs_query_relation_kind_t`
- `EcsRelationDepth` — `EcsRelationDepth`
- `EcsRelationExcluded` — `EcsRelationExcluded`
- `EcsRelationOptional` — `EcsRelationOptional`
- `EcsRelationRequired` — `EcsRelationRequired`
- `EcsRelationTarget` — `EcsRelationTarget`
- `ecs_query_relation_term_t` — `typedef struct { /* fields below */ } ecs_query_relation_term_t`
- `ecs_query_relation_term_t.id` — `ecs_relation_id_t id`
- `ecs_query_relation_term_t.kind` — `ecs_query_relation_kind_t kind`
- `ecs_query_relation_term_t.target` — `ecs_entity_t target`
- `ecs_table_t` — `typedef struct ecs_table_s ecs_table_t`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_QUERY_RELATION_CAPACITY` — `ECS_QUERY_RELATION_CAPACITY 8`
- `ECS_QUERY_TERM_CAPACITY` — `ECS_QUERY_TERM_CAPACITY 16`
- `ECS_QUERY_UP_ACCESS` — `ECS_QUERY_UP_ACCESS(access,relation)`
- `ecs_depth` — `ecs_depth(name,value)`
- `ecs_filter` — `ecs_filter(cname)`
- `ecs_in` — `ecs_in(cname)`
- `ecs_in_optional` — `ecs_in_optional(cname)`
- `ecs_inout` — `ecs_inout(cname)`
- `ecs_inout_optional` — `ecs_inout_optional(cname)`
- `ecs_not` — `ecs_not(cname)`
- `ecs_not_rel` — `ecs_not_rel(name)`
- `ecs_order_by_depth` — `ecs_order_by_depth(name)`
- `ecs_order_by_target` — `ecs_order_by_target(name)`
- `ecs_out` — `ecs_out(cname)`
- `ecs_query` — `ecs_query(...)`
- `ecs_query_each` — `ecs_query_each(it,i,...)`
- `ecs_query_entities` — `ecs_query_entities(entity,...)`
- `ecs_rel` — `ecs_rel(name)`
- `ecs_rel_opt` — `ecs_rel_opt(name)`
- `ecs_table_target` — `ecs_table_target(table,relation)`
- `ecs_to` — `ecs_to(name,entity)`
- `ecs_up` — `ecs_up(cname,relation)`
- `ecs_up_optional` — `ecs_up_optional(cname,relation)`
