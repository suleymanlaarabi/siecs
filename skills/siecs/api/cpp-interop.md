# C++20 API: interop

[Usage and contracts](../references/cpp/interop.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C interop reference](c-interop.md). Public base members also apply to derived builders.

## Functions

- `ecs::for_each_type` — `template <typename Tuple, typename Fn> constexpr void for_each_type(Fn &&fn)`
- `ecs::for_each_type_impl` — `template <typename Tuple, typename Fn, std::size_t... I> constexpr void for_each_type_impl(Fn &&fn, std::index_sequence<I...>)`
- `ecs::type_name` — `template <class T> consteval std::string_view type_name()`

## Types and members

- `ecs::function_traits` — `template <typename C, typename R, typename... Args> struct function_traits<R (C::*)(Args...) const>`
- `ecs::function_traits` — `template <typename C, typename R, typename... Args> struct function_traits<R (C::*)(Args...)>`
- `ecs::function_traits` — `template <typename F> struct function_traits<F, std::void_t<decltype(&F::operator())>> : function_traits<decltype(&F::operator())>`
- `ecs::function_traits` — `template <typename R, typename... Args> struct function_traits<R (&)(Args...)>`
- `ecs::function_traits` — `template <typename R, typename... Args> struct function_traits<R (*)(Args...)>`
- `ecs::function_traits` — `template <typename T, typename = void> struct function_traits`
- `ecs::function_traits::args_tuple` — `using args_tuple = std::tuple<>`
- `ecs::function_traits::args_tuple` — `using args_tuple = std::tuple<Args...>`
- `ecs::function_traits::return_type` — `using return_type = R`

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
- `SIECS_CPP_FIELD_SOURCE` — `SIECS_CPP_FIELD_SOURCE(...)`
- `SIECS_CPP_LAYOUT_TYPE` — `SIECS_CPP_LAYOUT_TYPE(cname)`
- `SIECS_CPP_LAYOUT_TYPE_INNER` — `SIECS_CPP_LAYOUT_TYPE_INNER(cname)`
- `SIECS_CPP_STRINGIFY` — `SIECS_CPP_STRINGIFY(...)`
- `SIECS_CPP_STRINGIFY_INNER` — `SIECS_CPP_STRINGIFY_INNER(...)`
- `SIECS_HAS_META` — `SIECS_HAS_META 1`
- `SIECS_LITERAL` — `SIECS_LITERAL(type,...)`
- `SIECS_PUBLIC_API` — platform-dependent public symbol visibility/export annotation
- `SIECS_TAG_META_DEFINE` — `SIECS_TAG_META_DEFINE(cname)`
- `ecs_id_comma` — `ecs_id_comma(x)`
- `fields_str` — `fields_str(...)`
- `reflected` — `reflected(...)`
