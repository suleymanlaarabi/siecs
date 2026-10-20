# C17 API: components

[Usage and contracts](../references/c/components.md). Signatures only; consult the guide for behavior.

## Functions

- `ecs_add_cid` — `void ecs_add_cid(ecs_entity_t entity, ecs_component_t id)`
- `ecs_component_count` — `uint32_t ecs_component_count(void)`
- `ecs_component_dynamic_init` — `ecs_component_t ecs_component_dynamic_init(const ecs_dynamic_component_desc_t *desc)`
- `ecs_component_info` — `const ecs_component_info_t *ecs_component_info(ecs_component_t component)`
- `ecs_component_init` — `ecs_component_t ecs_component_init(const ecs_component_desc_t *desc)`
- `ecs_component_lookup` — `ecs_component_t ecs_component_lookup(const char *name)`
- `ecs_component_name` — `const char *ecs_component_name(ecs_component_t component)`
- `ecs_component_register` — `ecs_component_t ecs_component_register(ecs_component_t *id, const ecs_component_desc_t *desc)`
- `ecs_get_cid` — `void *ecs_get_cid(ecs_entity_t entity, ecs_component_t id)`
- `ecs_has_cid` — `bool ecs_has_cid(const ecs_entity_t entity, ecs_component_t id)`
- `ecs_has_cid_owned` — `bool ecs_has_cid_owned(const ecs_entity_t entity, ecs_component_t id)`
- `ecs_modified_cid` — `void ecs_modified_cid(ecs_entity_t entity, ecs_component_t id)`
- `ecs_move_cid` — `void ecs_move_cid(ecs_entity_t entity, ecs_component_t id, void *data)`
- `ecs_remove_cid` — `void ecs_remove_cid(ecs_entity_t entity, ecs_component_t id)`
- `ecs_set_cid` — `void ecs_set_cid(ecs_entity_t entity, ecs_component_t id, const void *data)`
- `ecs_set_plain_many` — `bool ecs_set_plain_many(ecs_entity_t entity, uint32_t count, const ecs_component_t *ids, const void *const *values)`
- `ecs_tag_init` — `ecs_component_t ecs_tag_init(const char *name)`
- `ecs_try_get_cid` — `void *ecs_try_get_cid(ecs_entity_t entity, ecs_component_t cid)`
- `ecs_type_slot` — `uint16_t *ecs_type_slot(const char *key, uint16_t initial)`
- `ecs_with_many` — `void ecs_with_many(ecs_component_t component, ...)`

## Types and members

- `ecs_component_desc_t` — `typedef struct { /* fields below */ } ecs_component_desc_t`
- `ecs_component_desc_t.inheritance` — `ecs_component_inheritance_t inheritance`
- `ecs_component_desc_t.mutation` — `ecs_component_mutation_t mutation` (debug only)
- `ecs_component_desc_t.name` — `const char *name`
- `ecs_component_desc_t.on_add` — `ecs_component_on_add_t on_add`
- `ecs_component_desc_t.on_remove` — `ecs_component_on_remove_t on_remove`
- `ecs_component_desc_t.on_set` — `ecs_component_on_set_t on_set`
- `ecs_component_desc_t.ops` — `ecs_type_ops_t ops`
- `ecs_component_desc_t.size` — `uint64_t size`
- `ecs_component_desc_t.struct_desc` — `const sireflect_struct_desc_t *struct_desc`
- `ecs_component_info_t` — `typedef struct { /* fields below */ } ecs_component_info_t`
- `ecs_component_info_t.inheritance` — `ecs_component_inheritance_t inheritance`
- `ecs_component_info_t.mutation` — `ecs_component_mutation_t mutation` (debug only)
- `ecs_component_info_t.name` — `const char *name`
- `ecs_component_info_t.reflection` — `const sireflect_struct_desc_t *reflection`
- `ecs_component_info_t.size` — `uint64_t size`
- `ecs_component_info_t.type` — `sireflect_handle_t type`
- `ecs_component_mutation_t` — `typedef enum { /* values below */ } ecs_component_mutation_t` (debug only)
- `EcsMutable` — `EcsMutable = 0` (debug only)
- `EcsSetOnly` — `EcsSetOnly = 1` (debug only)
- `ecs_component_on_add_t` — `typedef void (*ecs_component_on_add_t)(ecs_entity_t entity, ecs_component_t component, void *value)`
- `ecs_component_on_remove_t` — `typedef void (*ecs_component_on_remove_t)( ecs_entity_t entity, ecs_component_t component, void *value )`
- `ecs_component_on_set_t` — `typedef void (*ecs_component_on_set_t)( ecs_entity_t entity, ecs_component_t component, const void *new_value, void *current_value )`
- `ecs_component_t` — `typedef uint16_t ecs_component_t`
- `ecs_dynamic_component_desc_t` — `typedef struct { /* fields below */ } ecs_dynamic_component_desc_t`
- `ecs_dynamic_component_desc_t.fields` — `const char *fields`
- `ecs_dynamic_component_desc_t.inheritance` — `ecs_component_inheritance_t inheritance`
- `ecs_dynamic_component_desc_t.mutation` — `ecs_component_mutation_t mutation` (debug only)
- `ecs_dynamic_component_desc_t.name` — `const char *name`
- `ecs_type_copy_t` — `typedef void (*ecs_type_copy_t)(void *dst, const void *src, uint32_t count)`
- `ecs_type_ctor_t` — `typedef void (*ecs_type_ctor_t)(void *dst, uint32_t count)`
- `ecs_type_dtor_t` — `typedef void (*ecs_type_dtor_t)(void *ptr, uint32_t count)`
- `ecs_type_move_t` — `typedef void (*ecs_type_move_t)(void *dst, void *src, uint32_t count)`
- `ecs_type_ops_t` — `typedef struct { /* fields below */ } ecs_type_ops_t`
- `ecs_type_ops_t.copy` — `ecs_type_copy_t copy`
- `ecs_type_ops_t.copy_ctor` — `ecs_type_copy_t copy_ctor`
- `ecs_type_ops_t.ctor` — `ecs_type_ctor_t ctor`
- `ecs_type_ops_t.dtor` — `ecs_type_dtor_t dtor`
- `ecs_type_ops_t.move` — `ecs_type_move_t move`
- `ecs_type_ops_t.move_ctor` — `ecs_type_move_t move_ctor`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_COMPONENT` — `ECS_COMPONENT(cname,...)`
- `ECS_COMPONENT_DECLARE` — `ECS_COMPONENT_DECLARE(cname,...)`
- `ECS_COMPONENT_DECLARE_CPP` — `ECS_COMPONENT_DECLARE_CPP(cname,fields,methods)`
- `ECS_COMPONENT_DECLARE_CPP_REFLECTED` — `ECS_COMPONENT_DECLARE_CPP_REFLECTED(cname,fields,reflection_fields,methods)`
- `ECS_COMPONENT_DEFINE` — `ECS_COMPONENT_DEFINE(cname,...)`
- `ECS_COMPONENT_REGISTER` — `ECS_COMPONENT_REGISTER(...)`
- `ECS_COMPONENT_REGISTER_ONE` — `ECS_COMPONENT_REGISTER_ONE(cname)`
- `ECS_CTOR` — `ECS_CTOR(cname,...)`
- `ECS_SET_ONLY` — `ECS_SET_ONLY .mutation = EcsSetOnly,` (debug only)
- `ECS_SET_ONLY` — `ECS_SET_ONLY` (expands to nothing in release)
- `ECS_TAG` — `ECS_TAG(cname)`
- `ECS_TAG_DECLARE` — `ECS_TAG_DECLARE(cname)`
- `ECS_TAG_DEFINE` — `ECS_TAG_DEFINE(cname,...)`
- `ecs_add` — `ecs_add(entity,cname)`
- `ecs_component` — `ecs_component(...)`
- `ecs_ctor_id` — `ecs_ctor_id(cname)`
- `ecs_get` — `ecs_get(entity,cname)`
- `ecs_has` — `ecs_has(entity,cname)`
- `ecs_id` — `ecs_id(name)`
- `ecs_modified` — `ecs_modified(entity,cname)`
- `ecs_remove` — `ecs_remove(entity,cname)`
- `ecs_rid` — `ecs_rid(name)`
- `ecs_set` — `ecs_set(entity,cname,...)`
- `ecs_try_get` — `ecs_try_get(entity,cname)`
- `ecs_with` — `ecs_with(first,...)`
