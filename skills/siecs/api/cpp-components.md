# C++20 API: components

[Usage and contracts](../references/cpp/components.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C components reference](c-components.md). Public base members also apply to derived builders.

## Functions

- `ecs::component` — `template <typename T> inline component_ref<T> component()`
- `ecs::component` — `template <typename T> requires(!detail::c_declared_component<T>) inline component_ref<T> component(const component_hooks<T> &hooks)`
- `ecs::component` — `template <typename T> requires(!detail::c_declared_component<T>) inline component_ref<T> component(const component_options<T> &options)`
- `ecs::component_requires` — `template <typename Component, typename Required> inline void component_requires()`

## Types and members

- `ecs::component_hooks` — `template <typename T> struct component_hooks`
- `ecs::component_hooks::on_add` — `on_add_t on_add = nullptr`
- `ecs::component_hooks::on_add_t` — `using on_add_t = void (*)(ecs_entity_t, T &)`
- `ecs::component_hooks::on_remove` — `on_remove_t on_remove = nullptr`
- `ecs::component_hooks::on_remove_t` — `using on_remove_t = void (*)(ecs_entity_t, T &)`
- `ecs::component_hooks::on_set` — `on_set_t on_set = nullptr`
- `ecs::component_hooks::on_set_t` — `using on_set_t = void (*)(ecs_entity_t, const T &, T &)`
- `ecs::component_options` — `template <typename T> struct component_options`
- `ecs::component_options::hooks` — `component_hooks<T> hooks{}`
- `ecs::component_options::inheritance` — `ecs_component_inheritance_t inheritance = EcsInheritOwned`
- `ecs::component_options::mutation` — `ecs_component_mutation_t mutation = EcsMutable` (debug only)
- `ecs::component_ref` — `template <typename T> class component_ref`
- `ecs::component_ref::component_ref<T>` — `explicit constexpr component_ref(ecs_component_t id) noexcept`
- `ecs::component_ref::id` — `[[nodiscard]] constexpr ecs_component_t id() const noexcept`
- `ecs::component_ref::operator ecs_component_t` — `constexpr operator ecs_component_t() const noexcept`
- `ecs::component_ref::with` — `template <typename... Required> requires(sizeof...(Required) > 0) component_ref with() const`
- `ecs::entity::add` — `template <typename... T> requires(sizeof...(T) > 0) entity add()`
- `ecs::entity::edit` — `template <typename T> [[nodiscard]] T &edit()`
- `ecs::entity::get` — `template <typename T> [[nodiscard]] const T &get() const`
- `ecs::entity::has` — `template <typename... T> requires(sizeof...(T) > 0) [[nodiscard]] bool has() const`
- `ecs::entity::remove` — `template <typename... T> requires(sizeof...(T) > 0) entity remove()`
- `ecs::entity::set` — `template <typename First, typename Second, typename... Rest> entity set(First &&first, Second &&second, Rest &&...rest)`
- `ecs::entity::set` — `template <typename T> entity set(T &&value)`
- `ecs::entity::try_edit` — `template <typename T> [[nodiscard]] T *try_edit()`
- `ecs::entity::try_get` — `template <typename T> [[nodiscard]] const T *try_get() const`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_COMPONENT` — `ECS_COMPONENT(cname,...)`
- `ECS_COMPONENT_DECLARE` — `ECS_COMPONENT_DECLARE(cname,...)`
- `ECS_COMPONENT_DECLARE_CPP` — `ECS_COMPONENT_DECLARE_CPP(cname,field_block,method_block)`
- `ECS_COMPONENT_DECLARE_CPP_REFLECTED` — `ECS_COMPONENT_DECLARE_CPP_REFLECTED(cname,field_block,reflection_field_block,method_block)`
- `ECS_COMPONENT_DEFINE` — `ECS_COMPONENT_DEFINE(cname,...)`
- `ECS_COMPONENT_REGISTER` — `ECS_COMPONENT_REGISTER(...)`
- `ECS_COMPONENT_REGISTER_ONE` — `ECS_COMPONENT_REGISTER_ONE(cname)`
- `ECS_CTOR` — `ECS_CTOR(cname,...)`
- `ECS_SET_ONLY` — `ECS_SET_ONLY` (release only)
- `ECS_SET_ONLY` — `ECS_SET_ONLY .mutation = EcsSetOnly,` (debug only)
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
