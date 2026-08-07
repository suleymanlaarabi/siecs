#pragma once
#include "siecs/cpp/c_api.hpp"
#include "siecs/cpp/component.hpp"
#include "siecs/cpp/entity.hpp"
#include "siecs/cpp/function_traits.hpp"
#include "siecs/cpp/resource.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ecs {

/** Nullable view for an optional query field; it never owns `T`. */
template <typename T> class optional {
    T *_ptr = nullptr;

  public:
    /** Construct from the current iterator field pointer, possibly null. */
    explicit optional(T *ptr) noexcept : _ptr(ptr) {}

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

template <typename T> struct is_entity : std::false_type {};
template <> struct is_entity<ecs::entity> : std::true_type {};

template <typename T> struct is_observer_event : std::false_type {};
template <typename T>
inline constexpr bool is_observer_event_v = is_observer_event<std::remove_cvref_t<T>>::value;

template <typename T> struct is_optional : std::false_type {};
template <typename T> struct is_optional<ecs::optional<T>> : std::true_type {};

template <typename T> inline constexpr bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

template <typename T> struct optional_value;
template <typename T> struct optional_value<ecs::optional<T>> {
    using type = T;
};

template <typename T>
using optional_value_t = typename optional_value<std::remove_cvref_t<T>>::type;

template <typename T> inline constexpr bool is_entity_v = is_entity<std::remove_cvref_t<T>>::value;

template <typename Args, std::size_t I> consteval std::size_t field_index_before() {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (
            (!is_res_v<std::tuple_element_t<Is, Args>> &&
                     !is_entity_v<std::tuple_element_t<Is, Args>> &&
                     !is_observer_event_v<std::tuple_element_t<Is, Args>>
                 ? 1U
                 : 0U) +
            ... + 0U
        );
    }(std::make_index_sequence<I>{});
}

template <typename Args> consteval std::size_t component_arg_count() {
    return field_index_before<Args, std::tuple_size_v<Args>>();
}

template <typename T> struct component_cursor {
    T *value;
    std::ptrdiff_t step;
};

struct entity_cursor {
    ecs_entity_t *value;
};

template <typename T> struct optional_cursor {
    T *value;
    std::ptrdiff_t step;
};

inline entity cursor_get(entity_cursor &cursor) { return entity::from(*cursor.value); }

template <typename T> inline optional<T> cursor_get(optional_cursor<T> &cursor) {
    return optional<T>(cursor.value);
}

template <typename T> inline decltype(auto) cursor_get(T &cursor) {
    if constexpr (is_res_v<T>)
        return (cursor);
    else
        return *cursor.value;
}

template <bool OwnedOnly, typename T> inline void cursor_next(T &cursor) noexcept {
    if constexpr (!is_res_v<T>)
        cursor.value += OwnedOnly ? 1 : cursor.step;
}

template <bool OwnedOnly> inline void cursor_next(entity_cursor &cursor) noexcept {
    (void)OwnedOnly;
    cursor.value++;
}

template <bool OwnedOnly, typename T>
inline void cursor_next(optional_cursor<T> &cursor) noexcept {
    if (cursor.value != nullptr) {
        cursor.value += OwnedOnly ? 1 : cursor.step;
    }
}

template <typename Args, std::size_t I, typename Resources>
inline auto make_cursor(ecs_iter_t *it, Resources &resources, bool &has_shared) {
    using arg = std::tuple_element_t<I, Args>;
    if constexpr (is_entity_v<arg>) {
        return entity_cursor{ it->entities };
    } else if constexpr (is_optional_v<arg>) {
        constexpr std::size_t field = field_index_before<Args, I>();
        using value_type = optional_value_t<arg>;
        auto *value = static_cast<value_type *>(ecs_field(it, static_cast<uint16_t>(field)));
        bool shared = false;
        if constexpr (std::is_const_v<value_type>) {
            shared = ecs_field_is_shared(it, static_cast<uint16_t>(field));
            has_shared |= shared;
        }
        return optional_cursor<value_type>{ value, shared ? 0U : 1U };
    } else if constexpr (is_res_v<arg>) {
        return std::get<I>(resources);
    } else {
        constexpr std::size_t field = field_index_before<Args, I>();
        using value_type = std::remove_reference_t<arg>;
        auto *value = static_cast<value_type *>(ecs_field(it, static_cast<uint16_t>(field)));
        bool shared = false;
        if constexpr (std::is_const_v<value_type>) {
            shared = ecs_field_is_shared(it, static_cast<uint16_t>(field));
            has_shared |= shared;
        }
        return component_cursor<value_type>{ value, shared ? 0U : 1U };
    }
}

template <typename Args, typename Resources, std::size_t... Is>
inline auto
make_cursors(ecs_iter_t *it, Resources &resources, bool &has_shared, std::index_sequence<Is...>) {
    return std::tuple{ make_cursor<Args, Is>(it, resources, has_shared)... };
}

template <bool OwnedOnly, typename F, typename Cursors>
inline void run_rows(F &func, Cursors &cursors, uint32_t count) {
    for (uint32_t row = 0; row < count; row++) {
        std::apply(
            [&](auto &...cursor) {
                std::invoke(func, cursor_get(cursor)...);
                (cursor_next<OwnedOnly>(cursor), ...);
            },
            cursors
        );
    }
}

template <typename F, typename Args, typename Resources>
inline void run_batch(F &func, ecs_iter_t *it, Resources &resources) {
    constexpr std::size_t count = std::tuple_size_v<Args>;
    bool has_shared = false;
    auto cursors = make_cursors<Args>(it, resources, has_shared, std::make_index_sequence<count>{});
    if (has_shared) {
        run_rows<false>(func, cursors, it->count);
    } else {
        run_rows<true>(func, cursors, it->count);
    }
}

template <typename T> consteval ecs_term_access_t term_access() {
    static_assert(
        std::is_lvalue_reference_v<T>,
        "query callback arguments must be lvalue references"
    );

    return std::is_const_v<std::remove_reference_t<T>> ? EcsIn : EcsInOut;
}

template <typename T> consteval ecs_term_access_t optional_term_access() {
    using value_type = optional_value_t<T>;
    return std::is_const_v<value_type> ? EcsInOptional : EcsInOutOptional;
}

inline void append_term(
    ecs_query_desc_t &desc,
    uint16_t &term_index,
    ecs_component_t id,
    ecs_term_access_t access
) {
    assert(term_index < ECS_QUERY_TERM_CAPACITY);
    desc.terms[term_index++] = {
        .id = id,
        .access = static_cast<uint32_t>(access),
    };
}

inline void append_callback_component_term(
    ecs_query_desc_t &desc,
    uint16_t &term_index,
    ecs_component_t id,
    ecs_term_access_t access
) {
    ecs_relation_id_t up_relation = 0;
    for (uint16_t i = 0; i < term_index; i++) {
        const ecs_term_access_t existing_access = static_cast<ecs_term_access_t>(
            static_cast<uint32_t>(desc.terms[i].access) & 0xffu
        );
        if (desc.terms[i].id != id ||
            (existing_access != EcsFilter && existing_access != EcsInUp &&
             existing_access != EcsInUpOptional)) {
            continue;
        }

        up_relation = static_cast<ecs_relation_id_t>(
            static_cast<uint32_t>(desc.terms[i].access) >> 8
        );

        for (uint16_t j = i; j + 1 < term_index; j++) {
            desc.terms[j] = desc.terms[j + 1];
        }
        term_index--;
        break;
    }

    if (up_relation) {
        assert(access == EcsIn || access == EcsInOptional);
        desc.terms[term_index++] = {
            .id = id,
            .access = ECS_QUERY_UP_ACCESS(
                access == EcsInOptional ? EcsInUpOptional : EcsInUp,
                up_relation
            ),
        };
    } else {
        append_term(desc, term_index, id, access);
    }
}

template <typename... T>
inline void append_terms(ecs_query_desc_t &desc, uint16_t &term_index, ecs_term_access_t access) {
    (append_term(desc, term_index, ecs::detail::ecs_cpp_component_id<T>(), access), ...);
}

template <typename Args, std::size_t I>
inline void append_callback_term(ecs_query_desc_t &desc, uint16_t &term_index) {
    using T = std::tuple_element_t<I, Args>;
    if constexpr (is_entity_v<T>) {
        static_assert(I == 0, "ecs::entity must be the first callback argument");
        static_assert(!std::is_reference_v<T>, "ecs::entity must be passed by value");
    } else if constexpr (is_observer_event_v<T>) {
        static_assert(I == 0, "ecs::observer_event must be the first callback argument");
        static_assert(!std::is_reference_v<T>, "ecs::observer_event must be passed by value");
    } else if constexpr (is_optional_v<T>) {
        append_callback_component_term(
            desc,
            term_index,
            ecs::detail::ecs_cpp_component_id<std::remove_cv_t<optional_value_t<T>>>(),
            optional_term_access<T>()
        );
    } else if constexpr (!is_res_v<T>) {
        append_callback_component_term(
            desc,
            term_index,
            ecs::detail::ecs_cpp_component_id<std::remove_cvref_t<T>>(),
            term_access<T>()
        );
    }
}

template <typename Args, std::size_t... Is>
inline void append_callback_terms_impl(
    ecs_query_desc_t &desc,
    uint16_t &term_index,
    std::index_sequence<Is...>
) {
    (append_callback_term<Args, Is>(desc, term_index), ...);
}

template <typename Args>
inline void append_callback_terms(ecs_query_desc_t &desc, uint16_t &term_index) {
    append_callback_terms_impl<Args>(
        desc,
        term_index,
        std::make_index_sequence<std::tuple_size_v<Args>>{}
    );
}

} // namespace detail

/** Move-only RAII owner of a persistent query id. */
class query_handle {
    ecs_query_id_t _id = 0;
    ecs_query_desc_t _base_desc{};
    uint16_t _base_term_index = 0;
    ecs_query_desc_t _active_desc{};
    uint16_t _active_term_index = 0;
    bool _has_base_desc = false;
    bool _has_active_desc = false;

    static bool desc_equals(
        const ecs_query_desc_t &left,
        uint16_t left_term_index,
        const ecs_query_desc_t &right,
        uint16_t right_term_index
    ) {
        if (left.is_a != right.is_a || left_term_index != right_term_index) {
            return false;
        }
        for (uint16_t i = 0; i < left_term_index; i++) {
            if (left.terms[i].id != right.terms[i].id ||
                left.terms[i].access != right.terms[i].access) {
                return false;
            }
        }
        return true;
    }

  public:
    /** Adopt an existing query id; the handle destroys it on scope exit. */
    explicit query_handle(ecs_query_id_t id) noexcept : _id(id) {}
    /** Build and own a query from its descriptor and term count. */
    query_handle(const ecs_query_desc_t &desc, uint16_t term_index)
        : _id(ecs_query_init(&desc)),
          _base_desc(desc),
          _base_term_index(term_index),
          _has_base_desc(true) {}
    /** Destroy the owned query, if any. */
    ~query_handle() {
        if (_id != 0) ecs_query_fini(_id);
    }

    /** Query handles cannot be copied because they own a C query id. */
    query_handle(const query_handle &) = delete;
    query_handle &operator=(const query_handle &) = delete;

    /** Transfer query ownership from `other`; `other` becomes empty. */
    query_handle(query_handle &&other) noexcept
        : _id(other._id),
          _base_desc(other._base_desc),
          _base_term_index(other._base_term_index),
          _active_desc(other._active_desc),
          _active_term_index(other._active_term_index),
          _has_base_desc(other._has_base_desc),
          _has_active_desc(other._has_active_desc) {
        other._id = 0;
        other._has_base_desc = false;
        other._has_active_desc = false;
    }

    /** Replace this query by moving ownership from `other`. */
    query_handle &operator=(query_handle &&other) noexcept {
        if (this != &other) {
            if (_id != 0) ecs_query_fini(_id);
            _id = other._id;
            _base_desc = other._base_desc;
            _base_term_index = other._base_term_index;
            _active_desc = other._active_desc;
            _active_term_index = other._active_term_index;
            _has_base_desc = other._has_base_desc;
            _has_active_desc = other._has_active_desc;
            other._id = 0;
            other._has_base_desc = false;
            other._has_active_desc = false;
        }
        return *this;
    }

    /** Return the owned query id, or zero for an empty/moved-from handle. */
    [[nodiscard]] ecs_query_id_t id() const noexcept { return _id; }

    /** Iterate matching entities; callback arguments must be valid references. */
    template <typename F> void each(F &&func) {
        using callback = std::remove_cvref_t<F>;
        using args = typename function_traits<callback>::args_tuple;

        if (_has_base_desc) {
            ecs_query_desc_t desc = _base_desc;
            uint16_t term_index = _base_term_index;
            detail::append_callback_terms<args>(desc, term_index);

            if (!_has_active_desc ||
                !desc_equals(desc, term_index, _active_desc, _active_term_index)) {
                if (_id != 0) ecs_query_fini(_id);
                _id = ecs_query_init(&desc);
                _active_desc = desc;
                _active_term_index = term_index;
                _has_active_desc = true;
            }
        }

        callback state(std::forward<F>(func));
        auto resources = detail::make_resources<args>();
        ecs_iter_t it = ecs_query_iter(_id);
        while (ecs_iter_next(&it)) {
            detail::run_batch<callback, args>(state, &it, resources);
        }
    }
};

/** Fluent typed query builder; `build_handle` owns the resulting query. */
class query {
  protected:
    ecs_query_desc_t desc{};
    uint16_t term_index = 0;
    uint16_t relation_index = 0;

  public:
    /** Construct an empty query descriptor. */
    query() = default;

    /** Add required, non-returned filter terms. */
    template <typename... T> query &require() {
        detail::append_terms<T...>(desc, term_index, EcsFilter);
        return *this;
    }

    /** Add optional read/write component terms. */
    template <typename... T> query &optional() {
        detail::append_terms<T...>(desc, term_index, EcsInOutOptional);
        return *this;
    }

    /** Add terms that must be absent from matching tables. */
    template <typename... T> query &exclude() {
        detail::append_terms<T...>(desc, term_index, EcsNot);
        return *this;
    }

    /** Restrict matches to entities inheriting from `target`. */
    query &is_a(ecs_entity_t target) {
        desc.is_a = target;
        return *this;
    }

    query &is_a(entity target) { return is_a(target.id()); }

    template <typename Relation> query &with_relation() {
        desc.relations[relation_index++] = {
            .target = 0,
            .id = detail::ecs_cpp_relation_id<Relation>(),
            .kind = EcsRelationRequired,
        };
        return *this;
    }

    template <typename Relation> query &to(ecs_entity_t target) {
        desc.relations[relation_index++] = {
            .target = target,
            .id = detail::ecs_cpp_relation_id<Relation>(),
            .kind = EcsRelationTarget,
        };
        return *this;
    }

    template <typename Relation> query &to(entity target) {
        return to<Relation>(target.id());
    }

    template <typename Relation> query &depth(uint32_t value) {
        desc.relations[relation_index++] = {
            .target = value,
            .id = detail::ecs_cpp_relation_id<Relation>(),
            .kind = EcsRelationDepth,
        };
        return *this;
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
        desc.terms[term_index++] = {
            .id = detail::ecs_cpp_component_id<Component>(),
            .access = ECS_QUERY_UP_ACCESS(
                EcsInUp,
                detail::ecs_cpp_relation_id<Relation>()
            ),
        };
        return *this;
    }

    /** Build a raw query id; caller must eventually call `ecs_query_fini`. */
    ecs_query_id_t build() { return ecs_query_init(&desc); }

    /** Build a move-only RAII query handle. */
    query_handle build_handle() { return query_handle(desc, term_index); }

    /** Build, iterate, and destroy a temporary query around `func`. */
    template <typename F> void each(F &&func) {
        using args = typename function_traits<std::remove_reference_t<F>>::args_tuple;

        detail::append_callback_terms<args>(desc, term_index);

        ecs_query_id_t qid = this->build();
        auto resources = detail::make_resources<args>();
        ecs_iter_t it = ecs_query_iter(qid);
        while (ecs_iter_next(&it)) {
            detail::run_batch<F, args>(func, &it, resources);
        }
        ecs_query_fini(qid);
    }

    /** Return the first match, or `entity::null()` when no table matches. */
    entity first() {
        ecs_query_id_t qid = this->build();
        ecs_iter_t it = ecs_query_iter(qid);
        ecs_entity_t result = 0;
        while (ecs_iter_next(&it)) {
            result = it.entities[0];
            break;
        }
        ecs_query_fini(qid);
        return result ? entity(result) : entity::null();
    }
};

} // namespace ecs
