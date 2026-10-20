# C++20 API: systems

[Usage and contracts](../references/cpp/systems.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C systems reference](c-systems.md). Public base members also apply to derived builders.

Inherited `with/without`, relation, ancestry, ancestor and ordering methods:
[complete filter table](../references/cpp/filters.md).
Callback forms and custom parameter hooks: [parameters](../references/cpp/parameters.md).

## Types and members

- `ecs::HasSystemParamQuery` — `template <typename T> concept HasSystemParamQuery = SystemParam<T> && requires(system_param_query &query) { system_param_t<T>::query(query); }`
- `ecs::IsSystemParamRow` — `template <typename T> concept IsSystemParamRow = SystemParam<T> && requires(system_param_t<T> &value, ecs_iter_t *it, uint32_t row) { value.row(it, row); }`
- `ecs::IsSystemParamTable` — `template <typename T> concept IsSystemParamTable = SystemParam<T> && requires(system_param_t<T> &value, ecs_iter_t *it) { value.table(it); }`
- `ecs::IsSystemParamWorld` — `template <typename T> concept IsSystemParamWorld = SystemParam<T> && requires(system_param_t<T> &value, ecs_world_t *world) { value.world(world); }`
- `ecs::SystemParam` — `template <typename T> concept SystemParam = requires { { system_param_t<T>::init() } -> std::same_as<system_param_t<T>>; }`
- `ecs::phase` — `class phase`
- `ecs::phase::after` — `phase &after(ecs_phase_t p)`
- `ecs::phase::before` — `phase &before(ecs_phase_t p)`
- `ecs::phase::id` — `ecs_phase_t id() const`
- `ecs::phase::operator ecs_phase_t` — `operator ecs_phase_t() const`
- `ecs::phase::phase` — `explicit phase(const char *name = "unnamed")`
- `ecs::system` — `class system : public query_filter<system>`
- `ecs::system::after` — `system &after(system_ref dependency)`
- `ecs::system::disabled` — `system &disabled(bool value = true)`
- `ecs::system::each` — `template <typename F> system_ref each(F &&func)`
- `ecs::system::interval` — `system &interval(double seconds)`
- `ecs::system::main_thread` — `system &main_thread()`
- `ecs::system::no_defer` — `system &no_defer()`
- `ecs::system::phase` — `system &phase(ecs_phase_t _phase)`
- `ecs::system::system` — `explicit system(const char *name = "unnamed")`
- `ecs::system_param_query` — `class system_param_query`
- `ecs::system_param_query::read_resource` — `template <typename T> void read_resource()`
- `ecs::system_param_query::require_relation` — `template <typename Relation> void require_relation()`
- `ecs::system_param_query::write_resource` — `template <typename T> void write_resource()`
- `ecs::system_param_t` — `template <typename T> using system_param_t = std::remove_cvref_t<T>`
- `ecs::system_ref` — `class system_ref`
- `ecs::system_ref::disable` — `void disable() const`
- `ecs::system_ref::enable` — `void enable() const`
- `ecs::system_ref::id` — `[[nodiscard]] ecs_system_id_t id() const noexcept`
- `ecs::system_ref::run` — `void run() const`
- `ecs::system_ref::system_ref` — `explicit system_ref(ecs_system_id_t id) noexcept`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_PHASE_NONE` — `ECS_PHASE_NONE ((ecs_phase_t) - 1)`
- `ECS_SYSTEM_AFTER_CAPACITY` — `ECS_SYSTEM_AFTER_CAPACITY 16`
- `ecs_phase` — `ecs_phase(...)`
- `ecs_system` — `ecs_system(...)`
