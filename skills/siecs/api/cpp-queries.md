# C++20 API: queries

[Usage and contracts](../references/cpp/queries.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C queries reference](c-queries.md). Public base members also apply to derived builders.

## Types and members

- `ecs::optional` — `template <typename T> class optional`
- `ecs::optional::field` — `void field(uint16_t index) noexcept`
- `ecs::optional::get` — `[[nodiscard]] T *get() const noexcept`
- `ecs::optional::init` — `static optional init() noexcept`
- `ecs::optional::operator bool` — `[[nodiscard]] explicit operator bool() const noexcept`
- `ecs::optional::operator*` — `[[nodiscard]] T &operator*() const noexcept`
- `ecs::optional::operator->` — `[[nodiscard]] T *operator->() const noexcept`
- `ecs::optional::optional<T>` — `explicit optional(T *ptr) noexcept`
- `ecs::optional::optional<T>` — `optional() noexcept = default`
- `ecs::optional::query` — `static void query(system_param_query &query)`
- `ecs::optional::row` — `void row(ecs_iter_t *, uint32_t row) noexcept`
- `ecs::optional::table` — `void table(ecs_iter_t *it) noexcept`
- `ecs::query` — `template <typename... Signature> class query : public query_filter<query<Signature...>>`
- `ecs::query::build` — `query_handle<Signature...> build()`
- `ecs::query::build_raw` — `ecs_query_id_t build_raw()`
- `ecs::query::each` — `template <typename F> void each(F &&func)`
- `ecs::query::first` — `entity first()`
- `ecs::query_filter` — `template <typename Derived> class query_filter`
- `ecs::query_filter::depth` — `template <typename Relation> Derived &depth(uint32_t value)`
- `ecs::query_filter::is_a` — `Derived &is_a(ecs_entity_t target)`
- `ecs::query_filter::is_a` — `Derived &is_a(entity target)`
- `ecs::query_filter::optional` — `template <typename... T> Derived &optional()`
- `ecs::query_filter::optional_relation` — `template <typename Relation> Derived &optional_relation()`
- `ecs::query_filter::order_by` — `Derived &order_by(ecs_query_order_t value)`
- `ecs::query_filter::order_by_depth` — `template <typename Relation> Derived &order_by_depth()`
- `ecs::query_filter::order_by_target` — `template <typename Relation> Derived &order_by_target()`
- `ecs::query_filter::up` — `template <typename Component, typename Relation> Derived &up()`
- `ecs::query_filter::with` — `template <typename Relation> Derived &with(ecs_entity_t target)`
- `ecs::query_filter::with` — `template <typename Relation> Derived &with(entity target)`
- `ecs::query_filter::with` — `template <typename... T> Derived &with()`
- `ecs::query_filter::with_relation` — `template <typename Relation> Derived &with_relation()`
- `ecs::query_filter::without` — `template <typename... T> Derived &without()`
- `ecs::query_filter::without_relation` — `template <typename Relation> Derived &without_relation()`
- `ecs::query_handle` — `template <typename... Signature> class query_handle`
- `ecs::query_handle::each` — `template <typename F> void each(F &&func)`
- `ecs::query_handle::id` — `[[nodiscard]] ecs_query_id_t id() const noexcept`
- `ecs::query_handle::operator=` — `query_handle &operator=(const query_handle &) = delete`
- `ecs::query_handle::operator=` — `query_handle &operator=(query_handle &&other) noexcept`
- `ecs::query_handle::query_handle<Signature...>` — `explicit query_handle(ecs_query_id_t id) noexcept`
- `ecs::query_handle::query_handle<Signature...>` — `query_handle( const ecs_query_desc_t &desc, uint16_t component_index, uint16_t resource_index, uint16_t relation_index )`
- `ecs::query_handle::query_handle<Signature...>` — `query_handle(const query_handle &) = delete`
- `ecs::query_handle::query_handle<Signature...>` — `query_handle(query_handle &&other) noexcept`
- `ecs::query_handle::release` — `[[nodiscard]] ecs_query_id_t release() noexcept`
- `ecs::query_handle::~query_handle<Signature...>` — `~query_handle()`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_QUERY_RELATION_CAPACITY` — `ECS_QUERY_RELATION_CAPACITY 8`
- `ECS_QUERY_TERM_CAPACITY` — `ECS_QUERY_TERM_CAPACITY 16`
- `ECS_QUERY_UP_ACCESS` — `ECS_QUERY_UP_ACCESS(access,relation)`
- `ecs_depth` — `ecs_depth(name,value)`
- `ecs_filter` — `ecs_filter(cname)`
- `ecs_in` — `ecs_in(cname)`
- `ecs_in_optional` — `ecs_in_optional(cname)`
- `ecs_inout` — `ecs_inout(cname)`
- `ecs_inout_optional` — `ecs_inout_optional(cname)`
- `ecs_not` — `ecs_not(cname)`
- `ecs_not_rel` — `ecs_not_rel(name)`
- `ecs_order_by_depth` — `ecs_order_by_depth(name)`
- `ecs_order_by_target` — `ecs_order_by_target(name)`
- `ecs_out` — `ecs_out(cname)`
- `ecs_query` — `ecs_query(...)`
- `ecs_query_each` — `ecs_query_each(it,i,...)`
- `ecs_query_entities` — `ecs_query_entities(entity,...)`
- `ecs_rel` — `ecs_rel(name)`
- `ecs_rel_opt` — `ecs_rel_opt(name)`
- `ecs_table_target` — `ecs_table_target(table,relation)`
- `ecs_to` — `ecs_to(name,entity)`
- `ecs_up` — `ecs_up(cname,relation)`
- `ecs_up_optional` — `ecs_up_optional(cname,relation)`
