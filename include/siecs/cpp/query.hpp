#pragma once
#include "siecs.h"
#include "siecs/cpp/c_api.hpp"
#include "siecs/cpp/component.hpp"
#include "siecs/cpp/entity.hpp"
#include "siecs/cpp/relation.hpp"
#include "siecs/cpp/function_traits.hpp"
#include "siecs/cpp/resource.hpp"
#include "siecs/cpp/system_param.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ecs {

/** Nullable view for an optional query field; it never owns `T`. */
template <typename T> class optional {
    T *_ptr = nullptr;
    T *_column = nullptr;
    std::ptrdiff_t _step = 1;
    uint16_t _field = 0;

  public:
    optional() noexcept = default;
    /** Construct from the current iterator field pointer, possibly null. */
    explicit optional(T *ptr) noexcept : _ptr(ptr) {}
    static optional init() noexcept { return {}; }
    static void query(system_param_query &query) { query.optional_component<T>(); }
    void field(uint16_t index) noexcept { _field = index; }
    void table(ecs_iter_t *it) noexcept {
        if (it->cache) {
            _column = static_cast<T *>(ecs_field(it, _field));
            _step = std::is_const_v<T> && ecs_field_is_shared(it, _field) ? 0 : 1;
        } else {
            _column = static_cast<T *>(ecs_try_get_cid(
                it->entities[0], detail::ecs_cpp_component_id<std::remove_cv_t<T>>()));
            _step = 0;
        }
    }
    void row(ecs_iter_t *, uint32_t row) noexcept {
        _ptr = _column ? _column + row * _step : nullptr;
    }

    /** Return true when the optional field exists in the current table. */
    [[nodiscard]] explicit operator bool() const noexcept { return _ptr != nullptr; }
    /** Return the field pointer, or null when absent. */
    [[nodiscard]] T *get() const noexcept { return _ptr; }
    /** Dereference the present field; caller must test the optional first. */
    [[nodiscard]] T *operator->() const noexcept { return _ptr; }
    /** Dereference the present field; caller must test the optional first. */
    [[nodiscard]] T &operator*() const noexcept { return *_ptr; }
};

namespace detail {

struct no_param {};
struct no_cursor {};

template <typename T>
concept system_param_field = SystemParam<T> &&
    requires(system_param_t<T> &value, uint16_t index) { value.field(index); };

template <typename T>
inline constexpr bool callback_arg_has_field = !SystemParam<T> || system_param_field<T>;

template <typename Args, std::size_t I> consteval std::size_t field_index_before() {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (static_cast<std::size_t>(callback_arg_has_field<std::tuple_element_t<Is, Args>>) + ... + 0U);
    }(std::make_index_sequence<I>{});
}

template <typename T> inline constexpr bool callback_arg_requires_row =
    !SystemParam<T> || IsSystemParamRow<T>;
template <typename T> inline constexpr bool callback_arg_requires_table =
    !SystemParam<T> || IsSystemParamTable<T> || IsSystemParamRow<T>;

template <typename Args, std::size_t... Is>
consteval bool callback_requires_row(std::index_sequence<Is...>) {
    return (callback_arg_requires_row<std::tuple_element_t<Is, Args>> || ...);
}

template <typename Args, std::size_t... Is>
consteval bool callback_requires_table(std::index_sequence<Is...>) {
    return (callback_arg_requires_table<std::tuple_element_t<Is, Args>> || ...);
}

template <typename T> auto init_system_param() {
    if constexpr (SystemParam<T>)
        return system_param_t<T>::init();
    else
        return no_param{};
}

template <typename Args, std::size_t... Is>
auto init_system_params(std::index_sequence<Is...>) {
    return std::tuple{init_system_param<std::tuple_element_t<Is, Args>>()...};
}

template <typename T> inline void prepare_world(T &value, ecs_world_t *world) noexcept {
    if constexpr (IsSystemParamWorld<T>) value.world(world);
}
template <typename T> inline void prepare_table(T &value, ecs_iter_t *it) noexcept {
    if constexpr (IsSystemParamTable<T>) value.table(it);
}
template <typename T> inline void prepare_row(T &value, ecs_iter_t *it, uint32_t row) noexcept {
    if constexpr (IsSystemParamRow<T>) value.row(it, row);
}

template <typename Args, std::size_t I, typename T>
inline void prepare_field(T &value) noexcept {
    if constexpr (system_param_field<T>)
        value.field(static_cast<uint16_t>(field_index_before<Args, I>()));
}

template <typename T> struct field_cursor {
    T *value;
    std::ptrdiff_t step;
};

template <typename Args, std::size_t I>
inline auto make_cursor(ecs_iter_t *it, bool &has_shared) {
    using arg = std::tuple_element_t<I, Args>;
    if constexpr (SystemParam<arg>) {
        return no_cursor{};
    } else {
        constexpr uint16_t field = static_cast<uint16_t>(field_index_before<Args, I>());
        using value_type = std::remove_reference_t<arg>;
        auto *value = static_cast<value_type *>(ecs_field(it, field));
        bool shared = false;
        if constexpr (std::is_const_v<value_type>) {
            shared = ecs_field_is_shared(it, field);
            has_shared |= shared;
        }
        return field_cursor<value_type>{value, static_cast<std::ptrdiff_t>(shared ? 0 : 1)};
    }
}

template <typename Args, std::size_t... Is>
inline auto make_cursors(ecs_iter_t *it, bool &has_shared, std::index_sequence<Is...>) {
    return std::tuple{make_cursor<Args, Is>(it, has_shared)...};
}

template <typename Arg, typename Param, typename Cursor>
inline decltype(auto) callback_arg(Param &param, Cursor &cursor, uint32_t row) noexcept {
    if constexpr (SystemParam<Arg>)
        return system_param_t<Arg>(param);
    else
        return *(cursor.value + row * cursor.step);
}

template <typename Args, typename F, typename Params, typename Cursors, std::size_t... Is>
inline void invoke_row(F &func, Params &params, Cursors &cursors, uint32_t row,
                       std::index_sequence<Is...>) {
    std::invoke(func, callback_arg<std::tuple_element_t<Is, Args>>(
                          std::get<Is>(params), std::get<Is>(cursors), row)...);
}

template <typename Args, typename F, typename Params, std::size_t... Is>
inline void invoke_once(F &func, Params &params, std::index_sequence<Is...>) {
    std::invoke(func, system_param_t<std::tuple_element_t<Is, Args>>(std::get<Is>(params))...);
}

template <typename Args, typename F, typename Params>
inline void run_batch(F &func, ecs_iter_t *it, Params &params) {
    constexpr auto indices = std::make_index_sequence<std::tuple_size_v<Args>>{};
    std::apply([&](auto &...param) { (prepare_table(param, it), ...); }, params);
    if constexpr (callback_requires_row<Args>(indices)) {
        bool has_shared = false;
        auto cursors = make_cursors<Args>(it, has_shared, indices);
        (void)has_shared;
        for (uint32_t row = 0; row < it->count; ++row) {
            std::apply([&](auto &...param) { (prepare_row(param, it, row), ...); }, params);
            invoke_row<Args>(func, params, cursors, row, indices);
        }
    } else {
        invoke_once<Args>(func, params, indices);
    }
}

template <typename F> inline void each_query(ecs_query_id_t qid, F &&func) {
    using callback = std::remove_cvref_t<F>;
    using args = typename function_traits<callback>::args_tuple;
    constexpr auto indices = std::make_index_sequence<std::tuple_size_v<args>>{};
    defer_scope defer;
    callback state(std::forward<F>(func));
    auto params = init_system_params<args>(indices);
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (prepare_field<args, Is>(std::get<Is>(params)), ...);
    }(indices);
    std::apply([&](auto &...param) { (prepare_world(param, ecs_world_current()), ...); }, params);
    ecs_iter_t it = ecs_query_iter(qid);
    while (ecs_iter_next(&it)) run_batch<args>(state, &it, params);
}

template <typename T> consteval ecs_access_t term_access() {
    static_assert(
        std::is_lvalue_reference_v<T>,
        "query callback arguments must be lvalue references"
    );

    return std::is_const_v<std::remove_reference_t<T>> ? EcsIn : EcsInOut;
}

inline void append_component_term(
    ecs_query_desc_t &desc,
    uint16_t &component_index,
    ecs_component_t id,
    uint32_t access
) {
    assert(component_index < ECS_QUERY_TERM_CAPACITY);
    desc.components[component_index++] = {
        .id = id,
        .access = access,
    };
}

inline void append_callback_component_term(
    ecs_query_desc_t &desc,
    uint16_t &component_index,
    ecs_component_t id,
    ecs_access_t access
) {
    ecs_relation_id_t up_relation = 0;
    uint32_t encoded_access = static_cast<uint32_t>(access);
    for (uint16_t i = 0; i < component_index; i++) {
        ecs_access_t existing_access = static_cast<ecs_access_t>(desc.components[i].access & 0xffu);
        if (desc.components[i].id != id ||
            (existing_access != EcsFilter && existing_access != EcsInUp &&
             existing_access != EcsInUpOptional)) {
            continue;
        }
        up_relation = static_cast<ecs_relation_id_t>(desc.components[i].access >> 8);
        std::memmove(
            desc.components + i,
            desc.components + i + 1,
            (--component_index - i) * sizeof(ecs_component_term_t)
        );
        break;
    }
    if (up_relation) {
        assert(access == EcsIn || access == EcsInOptional);
        encoded_access =
            ECS_QUERY_UP_ACCESS(access == EcsInOptional ? EcsInUpOptional : EcsInUp, up_relation);
    }
    append_component_term(desc, component_index, id, encoded_access);
}

template <typename... T>
inline void append_terms(ecs_query_desc_t &desc, uint16_t &component_index, ecs_access_t access) {
    (append_component_term(desc, component_index, ecs::detail::ecs_cpp_component_id<T>(), access),
     ...);
}

inline void append_resource_term(
    ecs_query_desc_t &desc,
    uint16_t &resource_index,
    ecs_resource_t id,
    ecs_access_t access
) {
    assert(resource_index < ECS_QUERY_RESOURCE_CAPACITY);
    desc.resources[resource_index++] = { .id = id, .access = static_cast<uint32_t>(access) };
}

inline ecs_query_relation_term_t *find_relation_term(
    ecs_query_desc_t &desc, uint16_t relation_count, ecs_relation_id_t id
) {
    for (uint16_t i = 0; i < relation_count; ++i)
        if (desc.relations[i].id == id)
            return &desc.relations[i];
    return nullptr;
}

template <typename R>
inline void ensure_required_relation(ecs_query_desc_t &desc, uint16_t &relation_index) {
    const ecs_relation_id_t id = ecs_cpp_relation_id<R>();
    if (auto *existing = find_relation_term(desc, relation_index, id)) {
        assert(existing->kind != EcsRelationExcluded &&
               "ecs::target<R> requires relation R, but the query explicitly excludes it");
        assert(existing->kind != EcsRelationOptional &&
               "ecs::target<R> requires relation R; use an optional target for optional relations");
        return;
    }
    assert(relation_index < ECS_QUERY_RELATION_CAPACITY && "too many query relation terms");
    desc.relations[relation_index++] = { .target = 0, .id = id, .kind = EcsRelationRequired };
}

struct system_param_query_access {
    static system_param_query make(ecs_query_desc_t &desc, uint16_t &component_index,
                                   uint16_t &resource_index, uint16_t &relation_index) {
        return {desc, component_index, resource_index, relation_index};
    }
};

template <typename Args, std::size_t I>
inline void append_callback_term(ecs_query_desc_t &desc, uint16_t &component_index,
                                 uint16_t &resource_index, uint16_t &relation_index) {
    using T = std::tuple_element_t<I, Args>;
    if constexpr (SystemParam<T>) {
        static_assert(!std::is_reference_v<T>, "SystemParam arguments must be passed by value");
        if constexpr (HasSystemParamQuery<T>) {
            auto query = system_param_query_access::make(desc, component_index,
                                                          resource_index, relation_index);
            system_param_t<T>::query(query);
        }
    } else {
        append_callback_component_term(
            desc, component_index, ecs_cpp_component_id<std::remove_cvref_t<T>>(), term_access<T>());
    }
}

template <typename Args, std::size_t... Is>
inline void append_callback_terms_impl(
    ecs_query_desc_t &desc,
    uint16_t &component_index,
    uint16_t &resource_index,
    uint16_t &relation_index,
    std::index_sequence<Is...>
) {
    (append_callback_term<Args, Is>(desc, component_index, resource_index, relation_index), ...);
}

template <typename Args>
inline void append_callback_terms(
    ecs_query_desc_t &desc, uint16_t &component_index, uint16_t &resource_index,
    uint16_t &relation_index
) {
    append_callback_terms_impl<Args>(
        desc,
        component_index,
        resource_index,
        relation_index,
        std::make_index_sequence<std::tuple_size_v<Args>>{}
    );
}

inline uint16_t query_resource_count(const ecs_query_desc_t &desc) {
    uint16_t count = 0;
    while (count < ECS_QUERY_RESOURCE_CAPACITY && desc.resources[count].id)
        count++;
    return count;
}

inline uint16_t query_relation_count(const ecs_query_desc_t &desc) {
    uint16_t count = 0;
    while (count < ECS_QUERY_RELATION_CAPACITY && desc.relations[count].id)
        ++count;
    return count;
}

} // namespace detail

template <typename T> inline void system_param_query::read_resource() {
    detail::append_resource_term(desc_, resource_index_, ecs_cpp_resource_id<T>(), EcsIn);
}
template <typename T> inline void system_param_query::write_resource() {
    detail::append_resource_term(desc_, resource_index_, ecs_cpp_resource_id<T>(), EcsInOut);
}
template <typename Relation> inline void system_param_query::require_relation() {
    detail::ensure_required_relation<Relation>(desc_, relation_index_);
}
template <typename T> inline void system_param_query::optional_component() {
    detail::append_callback_component_term(
        desc_, component_index_, detail::ecs_cpp_component_id<std::remove_cv_t<T>>(),
        std::is_const_v<T> ? EcsInOptional : EcsInOutOptional);
}

/** Move-only RAII owner of a persistent query id. */
class query_handle {
    ecs_query_id_t _id = 0;
    ecs_query_desc_t _base_desc{};
    uint16_t _base_component_index = UINT16_MAX;
    uint16_t _base_resource_index = UINT16_MAX;
    uint16_t _base_relation_index = UINT16_MAX;
    uint64_t _signature = 0;

    static uint64_t
    signature(const ecs_query_desc_t &desc, uint16_t component_count, uint16_t resource_count,
              uint16_t relation_count) {
        uint64_t value = component_count;
        for (uint16_t i = 0; i < component_count; i++)
            value = (value * 1099511628211ULL) ^ desc.components[i].id ^
                    ((uint64_t)desc.components[i].access << 16);
        value = (value * 1099511628211ULL) ^ (0x9e3779b97f4a7c15ULL + resource_count);
        for (uint16_t i = 0; i < resource_count; i++)
            value = (value * 1099511628211ULL) ^ desc.resources[i].id ^
                    ((uint64_t)desc.resources[i].access << 16);
        value = (value * 1099511628211ULL) ^ relation_count;
        for (uint16_t i = 0; i < relation_count; ++i) {
            const auto &r = desc.relations[i];
            value = (value * 1099511628211ULL) ^ r.id;
            value = (value * 1099511628211ULL) ^ r.kind;
            value = (value * 1099511628211ULL) ^ r.target;
        }
        return value;
    }

  public:
    /** Adopt an existing query id; the handle destroys it on scope exit. */
    explicit query_handle(ecs_query_id_t id) noexcept : _id(id) {}
    /** Build and own a query from its descriptor and term count. */
    query_handle(const ecs_query_desc_t &desc, uint16_t component_index)
        : query_handle(desc, component_index, detail::query_resource_count(desc),
                       detail::query_relation_count(desc)) {}
    query_handle(const ecs_query_desc_t &desc, uint16_t component_index, uint16_t resource_index)
        : query_handle(desc, component_index, resource_index, detail::query_relation_count(desc)) {}
    query_handle(const ecs_query_desc_t &desc, uint16_t component_index, uint16_t resource_index,
                 uint16_t relation_index)
        : _id(ecs_query_init(&desc)), _base_desc(desc), _base_component_index(component_index),
          _base_resource_index(resource_index), _base_relation_index(relation_index),
          _signature(signature(desc, component_index, resource_index, relation_index)) {}
    /** Destroy the owned query, if any. */
    ~query_handle() {
        if (_id != 0)
            ecs_query_fini(_id);
    }

    /** Query handles cannot be copied because they own a C query id. */
    query_handle(const query_handle &) = delete;
    query_handle &operator=(const query_handle &) = delete;

    /** Transfer query ownership from `other`; `other` becomes empty. */
    query_handle(query_handle &&other) noexcept { *this = std::move(other); }

    /** Replace this query by moving ownership from `other`. */
    query_handle &operator=(query_handle &&other) noexcept {
        if (this != &other) {
            std::swap(_id, other._id);
            std::swap(_base_desc, other._base_desc);
            std::swap(_base_component_index, other._base_component_index);
            std::swap(_base_resource_index, other._base_resource_index);
            std::swap(_base_relation_index, other._base_relation_index);
            std::swap(_signature, other._signature);
        }
        return *this;
    }

    /** Return the owned query id, or zero for an empty/moved-from handle. */
    [[nodiscard]] ecs_query_id_t id() const noexcept { return _id; }

    /** Iterate matching entities; callback arguments must be valid references. */
    template <typename F> void each(F &&func) {
        using callback = std::remove_cvref_t<F>;
        using args = typename function_traits<callback>::args_tuple;

        if (_base_component_index != UINT16_MAX) {
            ecs_query_desc_t desc = _base_desc;
            uint16_t component_index = _base_component_index;
            uint16_t resource_index = _base_resource_index;
            uint16_t relation_index = _base_relation_index;
            detail::append_callback_terms<args>(desc, component_index, resource_index, relation_index);
            uint64_t next_signature = signature(desc, component_index, resource_index, relation_index);
            if (next_signature != _signature) {
                if (_id != 0)
                    ecs_query_fini(_id);
                _id = ecs_query_init(&desc);
                _signature = next_signature;
            }
        }
        detail::each_query(_id, std::forward<F>(func));
    }
};

/** Fluent typed query builder; `build_handle` owns the resulting query. */
class query {
  protected:
    ecs_query_desc_t desc{};
    uint16_t component_index = 0;
    uint16_t resource_index = 0;
    uint16_t relation_index = 0;

    template <ecs_access_t Access, typename... T> query &components() {
        detail::append_terms<T...>(desc, component_index, Access);
        return *this;
    }

    template <typename Relation>
    query &relation(ecs_entity_t target, ecs_query_relation_kind_t kind) {
        assert(relation_index < ECS_QUERY_RELATION_CAPACITY && "too many query relation terms");
        desc.relations[relation_index++] = {
            .target = target,
            .id = detail::ecs_cpp_relation_id<Relation>(),
            .kind = kind,
        };
        return *this;
    }

  public:
    /** Construct an empty query descriptor. */
    query() = default;

    /** Add required, non-returned filter terms. */
    template <typename... T> query &require() { return components<EcsFilter, T...>(); }

    /** Add optional read/write component terms. */
    template <typename... T> query &optional() { return components<EcsInOutOptional, T...>(); }

    /** Add terms that must be absent from matching tables. */
    template <typename... T> query &exclude() { return components<EcsNot, T...>(); }

    /** Restrict matches to entities inheriting from `target`. */
    query &is_a(ecs_entity_t target) {
        desc.is_a = target;
        return *this;
    }

    query &is_a(entity target) { return is_a(target.id()); }

    template <typename Relation> query &with_relation() {
        return relation<Relation>(0, EcsRelationRequired);
    }

    template <typename Relation> query &where() { return with_relation<Relation>(); }
    template <typename Relation> query &where(ecs_entity_t target) { return to<Relation>(target); }
    template <typename Relation> query &where(entity target) { return to<Relation>(target); }
    template <typename Relation> query &at_depth(uint32_t value) { return depth<Relation>(value); }
    template <typename Relation> query &optional_relation() {
        return relation<Relation>(0, EcsRelationOptional);
    }
    template <typename Relation> query &exclude_relation() {
        return relation<Relation>(0, EcsRelationExcluded);
    }

    template <typename Relation> query &to(ecs_entity_t target) {
        return relation<Relation>(target, EcsRelationTarget);
    }

    template <typename Relation> query &to(entity target) { return to<Relation>(target.id()); }

    template <typename Relation> query &depth(uint32_t value) {
        return relation<Relation>(value, EcsRelationDepth);
    }

    query &order_by(ecs_query_order_t value) {
        desc.order_by = value;
        return *this;
    }

    template <typename Relation> query &order_by_target() {
        return order_by(ecs_order_by_target_id(detail::ecs_cpp_relation_id<Relation>()));
    }

    template <typename Relation> query &order_by_depth() {
        return order_by(ecs_order_by_depth_id(detail::ecs_cpp_relation_id<Relation>()));
    }

    template <typename Component, typename Relation> query &up() {
        desc.components[component_index++] = {
            .id = detail::ecs_cpp_component_id<Component>(),
            .access = ECS_QUERY_UP_ACCESS(EcsInUp, detail::ecs_cpp_relation_id<Relation>()),
        };
        return *this;
    }

    /** Build a raw query id; caller must eventually call `ecs_query_fini`. */
    ecs_query_id_t build() { return ecs_query_init(&desc); }

    /** Build a move-only RAII query handle. */
    query_handle build_handle() { return query_handle(desc, component_index, resource_index, relation_index); }

    /** Build, iterate, and destroy a temporary query around `func`. */
    template <typename F> void each(F &&func) {
        using args = typename function_traits<std::remove_cvref_t<F>>::args_tuple;
        ecs_query_desc_t typed = desc;
        uint16_t typed_component_index = component_index;
        uint16_t typed_resource_index = resource_index;
        uint16_t typed_relation_index = relation_index;
        detail::append_callback_terms<args>(typed, typed_component_index, typed_resource_index,
                                            typed_relation_index);
        ecs_query_id_t qid = ecs_query_init(&typed);
        detail::each_query(qid, std::forward<F>(func));
        ecs_query_fini(qid);
    }

    /** Return the first match, or `entity::null()` when no table matches. */
    entity first() {
        ecs_query_id_t qid = this->build();
        ecs_iter_t it = ecs_query_iter(qid);

        if (ecs_iter_next(&it)) {
            ecs_query_fini(qid);
            return it.entities[0];
        }
        ecs_query_fini(qid);
        return entity::null();
    }
};

} // namespace ecs
