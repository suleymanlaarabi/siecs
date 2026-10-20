# C++20 API: resources

[Usage and contracts](../references/cpp/resources.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C resources reference](c-resources.md). Public base members also apply to derived builders.

## Functions

- `ecs::ecs_cpp_resource_id` — `template <typename T> static ecs_resource_t ecs_cpp_resource_id(const resource_hooks<std::remove_cv_t<T>> *hooks = nullptr)`
- `ecs::ecs_cpp_try_resource_id` — `template <typename T> static ecs_resource_t ecs_cpp_try_resource_id()`
- `ecs::resource` — `template <typename T> requires(!detail::c_declared_resource<T>) resource_ref<T> resource(const resource_hooks<std::remove_cv_t<T>> &hooks)`
- `ecs::resource` — `template <typename T> resource_ref<T> resource()`

## Types and members

- `ecs::res` — `template <typename T> class res`
- `ecs::res::get` — `[[nodiscard]] T *get() const noexcept`
- `ecs::res::init` — `static res init() noexcept`
- `ecs::res::operator*` — `[[nodiscard]] T &operator*() const noexcept`
- `ecs::res::operator->` — `[[nodiscard]] T *operator->() const noexcept`
- `ecs::res::query` — `static void query(system_param_query &query)`
- `ecs::res::res<T>` — `explicit res(T *ptr) noexcept`
- `ecs::res::res<T>` — `res() noexcept = default`
- `ecs::res::world` — `void world(ecs_world_t *world) noexcept`
- `ecs::resource_hooks` — `template <typename T> struct resource_hooks`
- `ecs::resource_hooks::on_remove` — `on_remove_t on_remove = nullptr`
- `ecs::resource_hooks::on_remove_t` — `using on_remove_t = void (*)(const T &)`
- `ecs::resource_hooks::on_set` — `on_set_t on_set = nullptr`
- `ecs::resource_hooks::on_set_t` — `using on_set_t = void (*)(const T &)`
- `ecs::resource_ref` — `template <typename T> class resource_ref`
- `ecs::resource_ref::edit` — `[[nodiscard]] value_type &edit() const requires(!std::is_const_v<T>)`
- `ecs::resource_ref::get` — `[[nodiscard]] const value_type &get() const`
- `ecs::resource_ref::has` — `[[nodiscard]] bool has() const`
- `ecs::resource_ref::id` — `[[nodiscard]] ecs_resource_t id() const`
- `ecs::resource_ref::remove` — `void remove() const requires(!std::is_const_v<T>)`
- `ecs::resource_ref::set` — `template <typename U> requires(!std::is_const_v<T> && std::is_same_v<std::remove_cvref_t<U>, value_type>) void set(U &&value) const`
- `ecs::resource_ref::try_edit` — `[[nodiscard]] value_type *try_edit() const requires(!std::is_const_v<T>)`
- `ecs::resource_ref::try_get` — `[[nodiscard]] const value_type *try_get() const`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_QUERY_RESOURCE_CAPACITY` — `ECS_QUERY_RESOURCE_CAPACITY 16`
- `ECS_RESOURCE` — `ECS_RESOURCE(rname,...)`
- `ECS_RESOURCE_DECLARE` — `ECS_RESOURCE_DECLARE(rname,...)`
- `ECS_RESOURCE_DECLARE_CPP` — `ECS_RESOURCE_DECLARE_CPP(rname,field_block,method_block)`
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
