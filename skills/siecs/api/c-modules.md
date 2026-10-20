# C17 API: modules

[Usage and contracts](../references/c/modules.md). Signatures only; consult the guide for behavior.

## Functions

- `ecs_module_disable` — `void ecs_module_disable(ecs_module_id_t module)`
- `ecs_module_enable` — `void ecs_module_enable(ecs_module_id_t module)`
- `ecs_module_find` — `ecs_module_id_t ecs_module_find(const ecs_module_id_t *id)`
- `ecs_module_init` — `ecs_module_id_t ecs_module_init(const ecs_module_desc_t *desc)`
- `ecs_module_is_enabled` — `bool ecs_module_is_enabled(ecs_module_id_t module)`
- `ecs_module_load` — `ecs_module_id_t ecs_module_load(const char *path)`
- `ecs_module_name` — `const char *ecs_module_name(ecs_module_id_t module)`

## Types and members

- `ecs_module_desc_t` — `typedef struct { /* fields below */ } ecs_module_desc_t`
- `ecs_module_desc_t.desc` — `const void *desc`
- `ecs_module_desc_t.desc_size` — `uint32_t desc_size`
- `ecs_module_desc_t.disabled` — `bool disabled`
- `ecs_module_desc_t.id` — `ecs_module_id_t *id`
- `ecs_module_desc_t.import` — `ecs_module_import_t import`
- `ecs_module_desc_t.name` — `const char *name`
- `ecs_module_dynamic_import_t` — `typedef void (*ecs_module_dynamic_import_t)(void)`
- `ecs_module_id_t` — `typedef uint16_t ecs_module_id_t`
- `ecs_module_import_t` — `typedef void (*ecs_module_import_t)(const void *desc)`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_MODULE_CPP_DECLARE` — `ECS_MODULE_CPP_DECLARE(...)`
- `ECS_MODULE_DECLARE` — `ECS_MODULE_DECLARE(module_name,...)`
- `ECS_MODULE_DEFINE` — `ECS_MODULE_DEFINE(module_name)`
- `ECS_MODULE_IMPORT` — `ECS_MODULE_IMPORT(module_name,...)`
- `ecs_module` — `ecs_module(...)`
