# C++20 API: modules

[Usage and contracts](../references/cpp/modules.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C modules reference](c-modules.md). Public base members also apply to derived builders.

## Functions

- `ecs::import` — `template <typename T, typename... Args> requires(!detail::c_declared_module<T>) && detail::module_importable<T, Args...> module_ref<T> import(Args &&...args)`
- `ecs::import` — `template <typename T> requires detail::c_declared_module<T> module_ref<T> import()`
- `ecs::import` — `template <typename T> requires detail::c_declared_module<T> module_ref<T> import(const typename T::props_t &props)`
- `ecs::import_module_callback` — `template <typename T, typename... Args> static void import_module_callback(const void *ptr)`
- `ecs::module` — `template <typename T> [[nodiscard]] module_ref<T> module() noexcept`

## Types and members

- `ecs::module_ref` — `template <typename T> class module_ref`
- `ecs::module_ref::disable` — `void disable() const noexcept`
- `ecs::module_ref::enable` — `void enable() const noexcept`
- `ecs::module_ref::id` — `[[nodiscard]] constexpr ecs_module_id_t id() const noexcept`
- `ecs::module_ref::is_enabled` — `[[nodiscard]] bool is_enabled() const noexcept`
- `ecs::module_ref::module_ref<T>` — `constexpr explicit module_ref(ecs_module_id_t id) noexcept`
- `ecs::module_ref::module_ref<T>` — `constexpr module_ref() noexcept = default`
- `ecs::module_ref::operator bool` — `[[nodiscard]] constexpr explicit operator bool() const noexcept`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_MODULE_CPP_DECLARE` — `ECS_MODULE_CPP_DECLARE(module_name)`
- `ECS_MODULE_DECLARE` — `ECS_MODULE_DECLARE(module_name,...)`
- `ECS_MODULE_DEFINE` — `ECS_MODULE_DEFINE(module_name)`
- `ECS_MODULE_IMPORT` — `ECS_MODULE_IMPORT(module_name,...)`
- `ecs_module` — `ecs_module(...)`
