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

namespace detail {

template <typename T> struct is_entity : std::false_type {};
template <> struct is_entity<ecs::entity> : std::true_type {};

template <typename T> inline constexpr bool is_entity_v = is_entity<std::remove_cvref_t<T>>::value;

template <typename Args, std::size_t I> consteval std::size_t field_index_before() {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (
            (!is_res_v<std::tuple_element_t<Is, Args>> &&
                     !is_entity_v<std::tuple_element_t<Is, Args>>
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

inline entity cursor_get(entity_cursor &cursor) { return entity::from(*cursor.value); }

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

template <typename Args, std::size_t I, typename Resources>
inline auto make_cursor(ecs_iter_t *it, Resources &resources, bool &has_shared) {
    using arg = std::tuple_element_t<I, Args>;
    if constexpr (is_entity_v<arg>) {
        return entity_cursor{ it->entities };
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

inline void append_term(
    ecs_query_desc_t &desc,
    uint16_t &term_index,
    ecs_component_t id,
    ecs_term_access_t access
) {
    assert(term_index < ECS_QUERY_TERM_CAPACITY);
    desc.terms[term_index++] = {
        .id = id,
        .access = access,
    };
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
    } else if constexpr (!is_res_v<T>) {
        append_term(
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

class query {
  protected:
    ecs_query_desc_t desc{};
    uint16_t term_index = 0;

  public:
    query() = default;

    template <typename... T> query &require() {
        detail::append_terms<T...>(desc, term_index, EcsFilter);
        return *this;
    }

    template <typename... T> query &optional() {
        detail::append_terms<T...>(desc, term_index, EcsInOutOptional);
        return *this;
    }

    template <typename... T> query &exclude() {
        detail::append_terms<T...>(desc, term_index, EcsNot);
        return *this;
    }

    query &is_a(ecs_entity_t target) {
        desc.is_a = target;
        return *this;
    }

    ecs_query_id_t build() { return ecs_query_init(&desc); }

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
