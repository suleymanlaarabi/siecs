# C17 API: relations

[Usage and contracts](../references/c/relations.md). Signatures only; consult the guide for behavior.

## Functions

- `ecs_has_relation_id` — `bool ecs_has_relation_id(ecs_entity_t entity, ecs_relation_id_t relation)`
- `ecs_has_relation_to_id` — `bool ecs_has_relation_to_id(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target)`
- `ecs_relate_id` — `void ecs_relate_id(ecs_entity_t entity, ecs_relation_id_t relation, ecs_entity_t target)`
- `ecs_relation_batch_id` — `ecs_relation_batch_t ecs_relation_batch_id(const ecs_iter_t *it, ecs_relation_id_t relation)`
- `ecs_relation_count` — `uint32_t ecs_relation_count(void)`
- `ecs_relation_info` — `const ecs_relation_info_t *ecs_relation_info(ecs_relation_id_t relation)`
- `ecs_relation_init` — `ecs_relation_id_t ecs_relation_init(const char *name, const ecs_relation_desc_t *desc)`
- `ecs_relation_lookup` — `ecs_relation_id_t ecs_relation_lookup(const char *name)`
- `ecs_relation_register` — `ecs_relation_id_t ecs_relation_register(ecs_relation_id_t *id, const char *name, const ecs_relation_desc_t *desc)`
- `ecs_relation_sources` — `ecs_relation_sources_t ecs_relation_sources(ecs_entity_t target, ecs_relation_id_t relation)`
- `ecs_target_id` — `ecs_entity_t ecs_target_id(ecs_entity_t entity, ecs_relation_id_t relation)`
- `ecs_target_shared_id` — `ecs_entity_t ecs_target_shared_id(const ecs_iter_t *it, ecs_relation_id_t relation)`
- `ecs_targets_id` — `const ecs_relation_target_t * ecs_targets_id(const ecs_iter_t *it, ecs_relation_id_t relation)`
- `ecs_unrelate_id` — `void ecs_unrelate_id(ecs_entity_t entity, ecs_relation_id_t relation)`
- `ecs_with_relation_id` — `void ecs_with_relation_id(ecs_component_t component, ecs_relation_id_t relation, ecs_entity_t target)`

## Types and members

- `ecs_delete_target_t` — `typedef enum { /* values below */ } ecs_delete_target_t`
- `EcsDeleteSources` — `EcsDeleteSources`
- `EcsRemoveRelation` — `EcsRemoveRelation`
- `ecs_relation_batch_t` — `typedef struct { /* fields below */ } ecs_relation_batch_t`
- `ecs_relation_batch_t.shared` — `bool shared`
- `ecs_relation_batch_t.shared_target` — `ecs_entity_t shared_target`
- `ecs_relation_batch_t.targets` — `const ecs_relation_target_t *targets`
- `ecs_relation_desc_t` — `typedef struct { /* fields below */ } ecs_relation_desc_t`
- `ecs_relation_desc_t.acyclic` — `bool acyclic`
- `ecs_relation_desc_t.on_delete_target` — `ecs_delete_target_t on_delete_target`
- `ecs_relation_desc_t.storage` — `ecs_relation_storage_t storage`
- `ecs_relation_id_t` — `typedef uint16_t ecs_relation_id_t`
- `ecs_relation_info_t` — `typedef struct { /* fields below */ } ecs_relation_info_t`
- `ecs_relation_info_t.desc` — `ecs_relation_desc_t desc`
- `ecs_relation_info_t.name` — `const char *name`
- `ecs_relation_sources_t` — `typedef struct { /* fields below */ } ecs_relation_sources_t`
- `ecs_relation_sources_t.count` — `uint32_t count`
- `ecs_relation_sources_t.entities` — `const ecs_entity_t *entities`
- `ecs_relation_storage_t` — `typedef enum { /* values below */ } ecs_relation_storage_t`
- `EcsRelationByDepth` — `EcsRelationByDepth`
- `EcsRelationByTarget` — `EcsRelationByTarget`
- `EcsRelationDense` — `EcsRelationDense`
- `ecs_relation_target_t` — `typedef struct { /* fields below */ } ecs_relation_target_t`
- `ecs_relation_target_t.entity` — `ecs_entity_t entity`
- `ecs_relation_target_t.source_index` — `uint32_t source_index`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_RELATION` — `ECS_RELATION(name,...)`
- `ECS_RELATION_DECLARE` — `ECS_RELATION_DECLARE(name)`
- `ECS_RELATION_DEFINE` — `ECS_RELATION_DEFINE(name,...)`
- `ECS_RELATION_REGISTER` — `ECS_RELATION_REGISTER(...)`
- `ECS_RELATION_REGISTER_ONE` — `ECS_RELATION_REGISTER_ONE(cname)`
- `ecs_has_relation` — `ecs_has_relation(entity,relation)`
- `ecs_has_relation_to` — `ecs_has_relation_to(entity,relation,target)`
- `ecs_relate` — `ecs_relate(entity,relation,target)`
- `ecs_relation_batch` — `ecs_relation_batch(it,relation)`
- `ecs_target` — `ecs_target(entity,relation)`
- `ecs_target_shared` — `ecs_target_shared(it,relation)`
- `ecs_targets` — `ecs_targets(it,relation)`
- `ecs_unrelate` — `ecs_unrelate(entity,relation)`
- `ecs_with_relation` — `ecs_with_relation(component,relation,target)`
