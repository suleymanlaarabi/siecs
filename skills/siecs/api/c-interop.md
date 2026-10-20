# C17 API: interop

[Usage and contracts](../references/c/interop.md). Signatures only; consult the guide for behavior.

## Types and members

- `__DeltaTime_desc` — `__DeltaTime_desc: const sireflect_struct_desc_t`
- `__Name_desc` — `__Name_desc: const sireflect_struct_desc_t`
- `__sijson__DeltaTime__handle` — `__sijson__DeltaTime__handle: sireflect_handle_t`
- `__sijson__Name__handle` — `__sijson__Name__handle: sireflect_handle_t`
- `_ecs_id_Abstract__` — `_ecs_id_Abstract__: ecs_component_t`
- `_ecs_id_Abstract_desc__` — `_ecs_id_Abstract_desc__: ecs_component_desc_t`
- `_ecs_id_DeltaTime__` — `_ecs_id_DeltaTime__: ecs_resource_t`
- `_ecs_id_DeltaTime_desc__` — `_ecs_id_DeltaTime_desc__: ecs_resource_desc_t`
- `_ecs_id_Disabled__` — `_ecs_id_Disabled__: ecs_component_t`
- `_ecs_id_Disabled_desc__` — `_ecs_id_Disabled_desc__: ecs_component_desc_t`
- `_ecs_id_Name__` — `_ecs_id_Name__: ecs_component_t`
- `_ecs_id_Name_desc__` — `_ecs_id_Name_desc__: ecs_component_desc_t`
- `_ecs_rid_ChildOf__` — `_ecs_rid_ChildOf__: ecs_relation_id_t`
- `_ecs_rid_ChildOf_desc__` — `_ecs_rid_ChildOf_desc__: ecs_relation_desc_t`
- `_ecs_rid_IsA__` — `_ecs_rid_IsA__: ecs_relation_id_t`
- `_ecs_rid_IsA_desc__` — `_ecs_rid_IsA_desc__: ecs_relation_desc_t`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_CPP_FIELDS` — `ECS_CPP_FIELDS(...)`
- `ECS_CPP_METHODS` — `ECS_CPP_METHODS(...)`
- `PP_DEFER` — `PP_DEFER(id)`
- `PP_EMPTY` — `PP_EMPTY()`
- `PP_EVAL` — `PP_EVAL(...)`
- `PP_EVAL0` — `PP_EVAL0(...)`
- `PP_EVAL1` — `PP_EVAL1(...)`
- `PP_EVAL2` — `PP_EVAL2(...)`
- `PP_EVAL3` — `PP_EVAL3(...)`
- `PP_FOR_EACH` — `PP_FOR_EACH(F,...)`
- `PP_FOR_EACH_I` — `PP_FOR_EACH_I(F,first,...)`
- `PP_FOR_EACH_INDIRECT` — `PP_FOR_EACH_INDIRECT()`
- `PP_OBSTRUCT` — `PP_OBSTRUCT(...)`
- `SICORE_VEC` — `SICORE_VEC 1`
- `SIECS_API` — `SIECS_API`
- `SIECS_COMPONENT_META_DEFINE` — `SIECS_COMPONENT_META_DEFINE(cname)`
- `SIECS_COMPONENT_META_INIT` — `SIECS_COMPONENT_META_INIT(cname)`
- `SIECS_HAS_META` — `SIECS_HAS_META 1`
- `SIECS_LITERAL` — `SIECS_LITERAL(type,...)`
- `SIECS_PUBLIC_API` — platform-dependent public symbol visibility/export annotation
- `SIECS_TAG_META_DEFINE` — `SIECS_TAG_META_DEFINE(cname)`
- `ecs_id_comma` — `ecs_id_comma(x)`
