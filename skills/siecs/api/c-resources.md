# C17 API: resources

[Usage and contracts](../references/c/resources.md). Signatures only; consult the guide for behavior.

## Functions

- `ecs_has_resource_rid` — `bool ecs_has_resource_rid(const ecs_resource_t id)`
- `ecs_move_resource_rid` — `void ecs_move_resource_rid(ecs_resource_t id, void *data)`
- `ecs_remove_resource_rid` — `void ecs_remove_resource_rid(ecs_resource_t id)`
- `ecs_resource_count` — `uint32_t ecs_resource_count(void)`
- `ecs_resource_find` — `ecs_resource_t ecs_resource_find(const char *name)`
- `ecs_resource_info` — `const ecs_resource_info_t *ecs_resource_info(ecs_resource_t resource)`
- `ecs_resource_init` — `ecs_resource_t ecs_resource_init(const ecs_resource_desc_t *desc)`
- `ecs_resource_is_registered_rid` — `bool ecs_resource_is_registered_rid(ecs_resource_t id)`
- `ecs_resource_name` — `const char *ecs_resource_name(ecs_resource_t resource)`
- `ecs_resource_register` — `ecs_resource_t ecs_resource_register(ecs_resource_t *id, const ecs_resource_desc_t *desc)`
- `ecs_resource_rid` — `void *ecs_resource_rid(ecs_resource_t id)`
- `ecs_set_resource_rid` — `void ecs_set_resource_rid(ecs_resource_t id, const void *data)`
- `ecs_try_resource_rid` — `void *ecs_try_resource_rid(ecs_resource_t id)`

## Types and members

- `DeltaTime` — `typedef struct { /* fields below */ } DeltaTime`
- `DeltaTime.value` — `float value`
- `ecs_resource_desc_t` — `typedef struct { /* fields below */ } ecs_resource_desc_t`
- `ecs_resource_desc_t.name` — `const char *name`
- `ecs_resource_desc_t.on_remove` — `ecs_resource_hook_t on_remove`
- `ecs_resource_desc_t.on_set` — `ecs_resource_hook_t on_set`
- `ecs_resource_desc_t.ops` — `ecs_type_ops_t ops`
- `ecs_resource_desc_t.size` — `uint64_t size`
- `ecs_resource_desc_t.struct_desc` — `const sireflect_struct_desc_t *struct_desc`
- `ecs_resource_hook_t` — `typedef void (*ecs_resource_hook_t)(const void *ptr)`
- `ecs_resource_info_t` — `typedef struct { /* fields below */ } ecs_resource_info_t`
- `ecs_resource_info_t.name` — `const char *name`
- `ecs_resource_info_t.reflection` — `const sireflect_struct_desc_t *reflection`
- `ecs_resource_info_t.size` — `uint64_t size`
- `ecs_resource_info_t.type` — `sireflect_handle_t type`
- `ecs_resource_t` — `typedef uint16_t ecs_resource_t`
- `ecs_resource_term_t` — `typedef ecs_access_term_t ecs_resource_term_t`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_QUERY_RESOURCE_CAPACITY` — `ECS_QUERY_RESOURCE_CAPACITY 16`
- `ECS_RESOURCE` — `ECS_RESOURCE(rname,...)`
- `ECS_RESOURCE_DECLARE` — `ECS_RESOURCE_DECLARE(rname,...)`
- `ECS_RESOURCE_DECLARE_CPP` — `ECS_RESOURCE_DECLARE_CPP(rname,fields,methods)`
- `ECS_RESOURCE_DEFINE` — `ECS_RESOURCE_DEFINE(rname,...)`
- `ECS_RESOURCE_DEFINE_UNREFLECTED` — `ECS_RESOURCE_DEFINE_UNREFLECTED(rname,...)`
- `ECS_RESOURCE_REGISTER` — `ECS_RESOURCE_REGISTER(rname)`
- `ecs_get_resource` — `ecs_get_resource(rname)`
- `ecs_get_resource_read` — `ecs_get_resource_read(rname)`
- `ecs_has_resource` — `ecs_has_resource(rname)`
- `ecs_remove_resource` — `ecs_remove_resource(rname)`
- `ecs_set_resource` — `ecs_set_resource(rname,...)`
- `ecs_try_get_resource` — `ecs_try_get_resource(rname)`
- `ecs_try_get_resource_read` — `ecs_try_get_resource_read(rname)`
