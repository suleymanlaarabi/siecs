#pragma once
#include "siecs.h"
#include "siecs/cpp/c_api.hpp"
#include "siecs/cpp/component.hpp"
#include "siecs/cpp/entity.hpp"
#include "siecs/cpp/function_traits.hpp"
#include "siecs/cpp/relation.hpp"
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
        _column = static_cast<T *>(ecs_field(it, _field));
        _step = std::is_const_v<T> && ecs_field_is_shared(it, _field) ? 0 : 1;
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

/**
 * Required query field with a change filter: tables where `T` did not change
 * since the query last ran are skipped. `const T` reads, `T` reads and writes.
 */
template <typename T> class changed {
    T *_ptr = nullptr;
    T *_column = nullptr;
    std::ptrdiff_t _step = 1;
    uint16_t _field = 0;
    bool _changed = false;

  public:
    static changed init() noexcept { return {}; }
    static void query(system_param_query &query) { query.changed_component<T>(); }
    void field(uint16_t index) noexcept { _field = index; }
    void table(ecs_iter_t *it) noexcept {
        _column = static_cast<T *>(ecs_field(it, _field));
        _step = std::is_const_v<T> && ecs_field_is_shared(it, _field) ? 0 : 1;
        _changed = ecs_iter_changed(it, _field);
    }
    void row(ecs_iter_t *, uint32_t row) noexcept { _ptr = _column + row * _step; }

    /**
     * Return true when this field's column was written, or rows of the table
     * moved, since the query last ran. Another filtered field may be the one
     * that let this table through.
     */
    [[nodiscard]] bool has_changed() const noexcept { return _changed; }
    /** Return the field value of the current row. */
    [[nodiscard]] T *get() const noexcept { return _ptr; }
    [[nodiscard]] T *operator->() const noexcept { return _ptr; }
    [[nodiscard]] T &operator*() const noexcept { return *_ptr; }
};

/**
 * Table of the current batch. The id is the same in every query and stays
 * valid for the world's lifetime; rows are positions in that table, also inside
 * the portions of a parallel system.
 */
class table_info {
    uint16_t _id = 0;
    uint32_t _row_offset = 0;
    uint32_t _row = 0;
    bool _structure_changed = false;

  public:
    static table_info init() noexcept { return {}; }
    void table(ecs_iter_t *it) noexcept {
        _id = ecs_iter_table_id(it);
        _row_offset = it->row_offset;
        _structure_changed = ecs_iter_structure_changed(it);
    }
    void row(ecs_iter_t *, uint32_t row) noexcept { _row = _row_offset + row; }

    [[nodiscard]] uint16_t id() const noexcept { return _id; }
    /** Table row of the first entity of this batch. */
    [[nodiscard]] uint32_t row_offset() const noexcept { return _row_offset; }
    /** Table row of the current entity. */
    [[nodiscard]] uint32_t row() const noexcept { return _row; }
    /** True when rows were added, removed or moved since the query last ran. */
    [[nodiscard]] bool structure_changed() const noexcept { return _structure_changed; }
};

namespace detail {

struct no_param {};
struct no_cursor {};

template <typename T>
concept system_param_field =
    SystemParam<T> && requires(system_param_t<T> &value, uint16_t index) { value.field(index); };

template <typename T>
inline constexpr bool callback_arg_has_field = !SystemParam<T> || system_param_field<T>;

template <typename Args, std::size_t I> consteval std::size_t field_index_before() {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (
            static_cast<std::size_t>(callback_arg_has_field<std::tuple_element_t<Is, Args>>) + ... +
            0U
        );
    }(std::make_index_sequence<I>{});
}

template <typename T>
inline constexpr bool callback_arg_requires_row = !SystemParam<T> || IsSystemParamRow<T>;
template <typename T>
inline constexpr bool callback_arg_requires_table =
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

template <typename T> inline void prepare_world(T &value, ecs_world_t *world) noexcept {
    if constexpr (IsSystemParamWorld<T>)
        value.world(world);
}
template <typename T> inline void prepare_table(T &value, ecs_iter_t *it) noexcept {
    if constexpr (IsSystemParamTable<T>)
        value.table(it);
}
template <typename T> inline void prepare_row(T &value, ecs_iter_t *it, uint32_t row) noexcept {
    if constexpr (IsSystemParamRow<T>)
        value.row(it, row);
}

template <typename Args, std::size_t I, typename T> inline void prepare_field(T &value) noexcept {
    if constexpr (system_param_field<T>)
        value.field(static_cast<uint16_t>(field_index_before<Args, I>()));
}

template <typename Args> auto init_callback_params() {
    constexpr auto indices = std::make_index_sequence<std::tuple_size_v<Args>>{};
    auto params = []<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::tuple{ init_system_param<std::tuple_element_t<Is, Args>>()... };
    }(indices);
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (prepare_field<Args, Is>(std::get<Is>(params)), ...);
    }(indices);
    std::apply(
        [&](auto &...param) {
            if constexpr ((IsSystemParamWorld<std::remove_cvref_t<decltype(param)>> || ... ||
                           false)) {
                ecs_world_t *world = ecs_world_current();
                (prepare_world(param, world), ...);
            }
        },
        params
    );
    return params;
}

template <typename T> struct field_cursor {
    T *value;
    std::ptrdiff_t step;
};

template <typename Args, std::size_t I> inline auto make_cursor(ecs_iter_t *it, bool &has_shared) {
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
        return field_cursor<value_type>{ value, static_cast<std::ptrdiff_t>(shared ? 0 : 1) };
    }
}

template <typename Args, std::size_t... Is>
inline auto make_cursors(ecs_iter_t *it, bool &has_shared, std::index_sequence<Is...>) {
    return std::tuple{ make_cursor<Args, Is>(it, has_shared)... };
}

template <typename Arg, typename Param, typename Cursor>
inline decltype(auto) callback_arg(Param &param, Cursor &cursor, uint32_t row) noexcept {
    if constexpr (SystemParam<Arg>)
        return system_param_t<Arg>(param);
    else
        return *(cursor.value + row * cursor.step);
}

/* Same as callback_arg for tables without shared fields: every field is a
 * dense column, so the row indexes it directly with a compile-time unit step. */
template <typename Arg, typename Param, typename Cursor>
inline decltype(auto) callback_arg_dense(Param &param, Cursor &cursor, uint32_t row) noexcept {
    if constexpr (SystemParam<Arg>)
        return system_param_t<Arg>(param);
    else
        return cursor.value[row];
}

template <typename Args, typename F, typename Params, typename Cursors, std::size_t... Is>
inline void
invoke_row(F &func, Params &params, Cursors &cursors, uint32_t row, std::index_sequence<Is...>) {
    std::invoke(
        func,
        callback_arg<std::tuple_element_t<Is, Args>>(
            std::get<Is>(params),
            std::get<Is>(cursors),
            row
        )...
    );
}

template <typename Args, typename F, typename Params, typename Cursors, std::size_t... Is>
inline void invoke_row_dense(
    F &func,
    Params &params,
    Cursors &cursors,
    uint32_t row,
    std::index_sequence<Is...>
) {
    std::invoke(
        func,
        callback_arg_dense<std::tuple_element_t<Is, Args>>(
            std::get<Is>(params),
            std::get<Is>(cursors),
            row
        )...
    );
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
        const uint32_t count = it->count;
        /* Inherited (shared) values only come with IsA bases; every other table
         * gets the loop the compiler sees as plain arrays and can vectorize. */
        if (!has_shared) [[likely]] {
            for (uint32_t row = 0; row < count; ++row) {
                std::apply([&](auto &...param) { (prepare_row(param, it, row), ...); }, params);
                invoke_row_dense<Args>(func, params, cursors, row, indices);
            }
            return;
        }
        for (uint32_t row = 0; row < count; ++row) {
            std::apply([&](auto &...param) { (prepare_row(param, it, row), ...); }, params);
            invoke_row<Args>(func, params, cursors, row, indices);
        }
    } else {
        invoke_once<Args>(func, params, indices);
    }
}

template <typename Args, typename F> inline void each_query(ecs_query_id_t qid, F &&func) {
    using callback = std::remove_cvref_t<F>;
    using args = Args;
    defer_scope defer;
    callback state(std::forward<F>(func));
    auto params = init_callback_params<args>();
    ecs_iter_t it = ecs_query_iter(qid);
    while (ecs_iter_next(&it))
        run_batch<args>(state, &it, params);
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
    bool changed = false;
    uint32_t encoded_access = static_cast<uint32_t>(access);
    for (uint16_t i = 0; i < component_index; i++) {
        ecs_access_t existing_access = static_cast<ecs_access_t>(desc.components[i].access & 0xffu);
        if (desc.components[i].id != id ||
            (existing_access != EcsFilter && existing_access != EcsInUp &&
             existing_access != EcsInUpOptional)) {
            continue;
        }
        up_relation = static_cast<ecs_relation_id_t>((desc.components[i].access >> 8) & 0xffffu);
        changed = desc.components[i].access & ECS_TERM_CHANGED;
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
    if (changed)
        encoded_access |= ECS_TERM_CHANGED;
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

inline ecs_query_relation_term_t *
find_relation_term(ecs_query_desc_t &desc, uint16_t relation_count, ecs_relation_id_t id) {
    for (uint16_t i = 0; i < relation_count; ++i)
        if (desc.relations[i].id == id)
            return &desc.relations[i];
    return nullptr;
}

template <typename R>
inline void ensure_required_relation(ecs_query_desc_t &desc, uint16_t &relation_index) {
    const ecs_relation_id_t id = ecs_cpp_relation_id<R>();
    if (auto *existing = find_relation_term(desc, relation_index, id)) {
        assert(
            existing->kind != EcsRelationExcluded &&
            "ecs::target<R> requires relation R, but the query explicitly excludes it"
        );
        assert(
            existing->kind != EcsRelationOptional &&
            "ecs::target<R> requires relation R; use an optional target for optional relations"
        );
        return;
    }
    assert(relation_index < ECS_QUERY_RELATION_CAPACITY && "too many query relation terms");
    desc.relations[relation_index++] = { .target = 0, .id = id, .kind = EcsRelationRequired };
}

struct system_param_query_access {
    static system_param_query make(
        ecs_query_desc_t &desc,
        uint16_t &component_index,
        uint16_t &resource_index,
        uint16_t &relation_index
    ) {
        return { desc, component_index, resource_index, relation_index };
    }
};

template <typename Args, std::size_t I>
inline void append_callback_term(
    ecs_query_desc_t &desc,
    uint16_t &component_index,
    uint16_t &resource_index,
    uint16_t &relation_index
) {
    using T = std::tuple_element_t<I, Args>;
    if constexpr (SystemParam<T>) {
        static_assert(!std::is_reference_v<T>, "SystemParam arguments must be passed by value");
        if constexpr (HasSystemParamQuery<T>) {
            auto query = system_param_query_access::make(
                desc,
                component_index,
                resource_index,
                relation_index
            );
            system_param_t<T>::query(query);
        }
    } else {
        append_callback_component_term(
            desc,
            component_index,
            ecs_cpp_component_id<std::remove_cvref_t<T>>(),
            term_access<T>()
        );
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
    ecs_query_desc_t &desc,
    uint16_t &component_index,
    uint16_t &resource_index,
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
        desc_,
        component_index_,
        detail::ecs_cpp_component_id<std::remove_cv_t<T>>(),
        std::is_const_v<T> ? EcsInOptional : EcsInOutOptional
    );
}
template <typename T> inline void system_param_query::changed_component() {
    detail::append_callback_component_term(
        desc_,
        component_index_,
        detail::ecs_cpp_component_id<std::remove_cv_t<T>>(),
        std::is_const_v<T> ? EcsIn : EcsInOut
    );
    desc_.components[component_index_ - 1].access |= ECS_TERM_CHANGED;
}

namespace detail {
/* One distinct address per callback argument list: comparing it tells whether
 * the terms appended for a callback are the ones the query already has. */
template <typename Args> inline constexpr char callback_args_tag = 0;
} // namespace detail

/** Move-only RAII owner of a persistent query id. */
template <typename... Signature> class query_handle {
    ecs_query_id_t _id = 0;
    ecs_query_desc_t _base_desc{};
    uint16_t _base_component_index = UINT16_MAX;
    uint16_t _base_resource_index = UINT16_MAX;
    uint16_t _base_relation_index = UINT16_MAX;
    uint64_t _signature = 0;
    const void *_callback_args = nullptr;

    static uint64_t signature(
        const ecs_query_desc_t &desc,
        uint16_t component_count,
        uint16_t resource_count,
        uint16_t relation_count
    ) {
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
    /** Build and own a query from its descriptor and term counts. */
    query_handle(
        const ecs_query_desc_t &desc,
        uint16_t component_index,
        uint16_t resource_index,
        uint16_t relation_index
    )
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
            if (_id != 0)
                ecs_query_fini(_id);
            _id = std::exchange(other._id, 0);
            _base_desc = other._base_desc;
            _base_component_index = other._base_component_index;
            _base_resource_index = other._base_resource_index;
            _base_relation_index = other._base_relation_index;
            _signature = other._signature;
            _callback_args = other._callback_args;
        }
        return *this;
    }

    /** Return the owned query id, or zero for an empty/moved-from handle. */
    [[nodiscard]] ecs_query_id_t id() const noexcept { return _id; }
    /** Transfer ownership to a C caller, which must call ecs_query_fini. */
    [[nodiscard]] ecs_query_id_t release() noexcept { return std::exchange(_id, 0); }

    /** Iterate matching entities; callback arguments must be valid references. */
    template <typename F> void each(F &&func) {
        using callback = std::remove_cvref_t<F>;
        using args = detail::callback_args_t<callback, Signature...>;

        if constexpr (sizeof...(Signature) == 0) {
            if (_base_component_index != UINT16_MAX &&
                _callback_args != &detail::callback_args_tag<args>) {
                _callback_args = &detail::callback_args_tag<args>;
                ecs_query_desc_t desc = _base_desc;
                uint16_t component_index = _base_component_index;
                uint16_t resource_index = _base_resource_index;
                uint16_t relation_index = _base_relation_index;
                detail::append_callback_terms<args>(
                    desc,
                    component_index,
                    resource_index,
                    relation_index
                );
                uint64_t next_signature =
                    signature(desc, component_index, resource_index, relation_index);
                if (next_signature != _signature) {
                    if (_id != 0)
                        ecs_query_fini(_id);
                    _id = ecs_query_init(&desc);
                    _signature = next_signature;
                }
            }
        }
        detail::each_query<args>(_id, std::forward<F>(func));
    }
};

/** Shared query filters; every operation preserves the concrete builder type. */
template <typename Derived> class query_filter {
  protected:
    ecs_query_desc_t desc{};
    uint16_t component_index = 0;
    uint16_t resource_index = 0;
    uint16_t relation_index = 0;

    template <ecs_access_t Access, typename... T> Derived &components() {
        detail::append_terms<T...>(desc, component_index, Access);
        return static_cast<Derived &>(*this);
    }

    template <typename Relation>
    Derived &relation(ecs_entity_t target, ecs_query_relation_kind_t kind) {
        assert(relation_index < ECS_QUERY_RELATION_CAPACITY && "too many query relation terms");
        desc.relations[relation_index++] = {
            .target = target,
            .id = detail::ecs_cpp_relation_id<Relation>(),
            .kind = kind,
        };
        return static_cast<Derived &>(*this);
    }

  public:
    /** Add required, non-returned filter terms. */
    template <typename... T> Derived &with() { return components<EcsFilter, T...>(); }

    /** Add optional read/write component terms. */
    template <typename... T> Derived &optional() { return components<EcsInOutOptional, T...>(); }

    /** Add terms that must be absent from matching tables. */
    template <typename... T> Derived &without() { return components<EcsNot, T...>(); }

    /**
     * Skip tables where none of these components changed since the query last
     * ran. Each component must also be a callback argument.
     */
    template <typename... T> Derived &changed() {
        (detail::append_component_term(
             desc,
             component_index,
             detail::ecs_cpp_component_id<T>(),
             EcsFilter | ECS_TERM_CHANGED
         ),
         ...);
        return static_cast<Derived &>(*this);
    }

    /** Restrict matches to entities inheriting from `target`. */
    Derived &is_a(ecs_entity_t target) {
        desc.is_a = target;
        return static_cast<Derived &>(*this);
    }

    Derived &is_a(entity target) { return is_a(target.id()); }

    template <typename Relation> Derived &with_relation() {
        return relation<Relation>(0, EcsRelationRequired);
    }

    template <typename Relation> Derived &optional_relation() {
        return relation<Relation>(0, EcsRelationOptional);
    }
    template <typename Relation> Derived &without_relation() {
        return relation<Relation>(0, EcsRelationExcluded);
    }

    template <typename Relation> Derived &with(ecs_entity_t target) {
        return relation<Relation>(target, EcsRelationTarget);
    }

    template <typename Relation> Derived &with(entity target) {
        return with<Relation>(target.id());
    }

    template <typename Relation> Derived &depth(uint32_t value) {
        return relation<Relation>(value, EcsRelationDepth);
    }

    Derived &order_by(ecs_query_order_t value) {
        desc.order_by = value;
        return static_cast<Derived &>(*this);
    }

    template <typename Relation> Derived &order_by_target() {
        return order_by(ecs_order_by_target_id(detail::ecs_cpp_relation_id<Relation>()));
    }

    template <typename Relation> Derived &order_by_depth() {
        return order_by(ecs_order_by_depth_id(detail::ecs_cpp_relation_id<Relation>()));
    }

    template <typename Component, typename Relation> Derived &up() {
        detail::append_component_term(
            desc,
            component_index,
            detail::ecs_cpp_component_id<Component>(),
            ECS_QUERY_UP_ACCESS(EcsInUp, detail::ecs_cpp_relation_id<Relation>())
        );
        return static_cast<Derived &>(*this);
    }
};

/** Typed query builder; build() returns a move-only persistent query. */
template <typename... Signature> class query : public query_filter<query<Signature...>> {
    using base = query_filter<query<Signature...>>;
    using base::component_index;
    using base::desc;
    using base::relation_index;
    using base::resource_index;

    template <typename... Terms> query_handle<Terms...> build_signature() {
        ecs_query_desc_t typed = desc;
        auto components = component_index;
        auto resources = resource_index;
        auto relations = relation_index;
        detail::append_callback_terms<std::tuple<Terms...>>(
            typed,
            components,
            resources,
            relations
        );
        return query_handle<Terms...>(typed, components, resources, relations);
    }

  public:
    /** Build a raw query id; caller must eventually call `ecs_query_fini`. */
    ecs_query_id_t build_raw() {
        auto typed = build();
        return typed.release();
    }

    /** Build a move-only RAII query handle. */
    query_handle<Signature...> build() { return build_signature<Signature...>(); }

    /** Iterate a temporary query, deducing access unless a signature was declared. */
    template <typename F> void each(F &&func) {
        using args = detail::callback_args_t<std::remove_cvref_t<F>, Signature...>;
        [&]<typename... Terms>(std::tuple<Terms...> *) {
            auto handle = build_signature<Terms...>();
            detail::each_query<args>(handle.id(), std::forward<F>(func));
        }(static_cast<args *>(nullptr));
    }

    /** Return the first match, or `entity::null()` when no table matches. */
    entity first() {
        auto handle = build();
        ecs_iter_t it = ecs_query_iter(handle.id());
        return ecs_iter_next(&it) && it.count ? entity::from(it.entities[0]) : entity::null();
    }
};

} // namespace ecs
