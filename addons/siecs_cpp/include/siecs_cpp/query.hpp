#pragma once

#include "siecs.h"
#include "siecs_cpp/component.hpp"
#include "siecs_cpp/type.hpp"
#include <cassert>
#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ecs {

namespace detail {

template <typename F, typename Tuple, std::size_t... Is>
inline void call_fields_impl(F &&func, Tuple &fields, uint32_t count, std::index_sequence<Is...>) {
    for (uint32_t i = 0; i < count; i++) {
        std::invoke(std::forward<F>(func), (std::get<Is>(fields)[i])...);
    }
}

template <typename F, typename Tuple>
inline void call_fields(F &&func, Tuple &fields, uint32_t count) {
    constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;

    call_fields_impl(std::forward<F>(func), fields, count, std::make_index_sequence<N>{});
}

template <typename Args, std::size_t... Is>
inline auto make_fields(ecs_iter_t *it, std::index_sequence<Is...>) {
    /*
     * ecs_field indexes only field terms (EcsIn/EcsOut/EcsInOut), not every
     * query term. require/exclude append EcsFilter/EcsNot terms, and callback
     * arguments append all field terms, so callback argument N maps to field N.
     */
    return std::tuple{ static_cast<std::remove_reference_t<std::tuple_element_t<Is, Args>> *>(
        ecs_field(it, static_cast<uint16_t>(Is))
    )... };
}

template <typename T> consteval ecs_term_access_t term_access() {
    static_assert(
        std::is_lvalue_reference_v<T>,
        "query callback arguments must be lvalue references"
    );

    if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
        return EcsIn;
    } else {
        return EcsInOut;
    }
}

inline void append_term(
    ecs_query_desc_t &desc,
    uint16_t &term_index,
    ecs_component_t id,
    ecs_term_access_t access
) {
    constexpr uint16_t query_term_capacity =
        static_cast<uint16_t>(sizeof(desc.terms) / sizeof(desc.terms[0]));

    assert(term_index + 1 < query_term_capacity && "too many query terms");

    desc.terms[term_index] = {
        .id = id,
        .access = access,
    };
    term_index += 1;
}

template <typename... T>
inline void append_terms(
    ecs_world_t *world,
    ecs_query_desc_t &desc,
    uint16_t &term_index,
    ecs_term_access_t access
) {
    (append_term(desc, term_index, ecs::ecs_cpp_component_id<T>(world), access), ...);
}

template <typename Args>
inline void
append_callback_terms(ecs_world_t *world, ecs_query_desc_t &desc, uint16_t &term_index) {
    for_each_type<Args>([&]<typename T>() {
        append_term(
            desc,
            term_index,
            ecs::ecs_cpp_component_id<std::remove_cvref_t<T>>(world),
            term_access<T>()
        );
    });
}

template <typename F, typename Args>
inline void run_query(F &func, ecs_world_t *world, ecs_query_id_t qid) {
    constexpr std::size_t arg_count = std::tuple_size_v<Args>;
    static_assert(arg_count > 0, "query callbacks must read at least one component");

    ecs_iter_t it = ecs_query_iter(world, qid);

    while (ecs_iter_next(&it)) {
        auto fields = make_fields<Args>(&it, std::make_index_sequence<arg_count>{});
        call_fields(func, fields, it.count);
    }
}

} // namespace detail

class world;

class query {
  protected:
    ecs_query_desc_t desc{};
    uint16_t term_index = 0;
    ecs_world_t *_world = nullptr;

  public:
    explicit query(ecs_world_t *world) noexcept : _world(world) {}

    template <typename... T> query &&require() {
        detail::append_terms<T...>(_world, desc, term_index, EcsFilter);
        return std::move(*this);
    }

    template <typename... T> query &&exclude() {
        detail::append_terms<T...>(_world, desc, term_index, EcsNot);
        return std::move(*this);
    }

    template <typename F> void each(F &&func) {
        using traits = function_traits<std::remove_reference_t<F>>;
        using args = typename traits::args_tuple;
        constexpr std::size_t arg_count = std::tuple_size_v<args>;
        static_assert(arg_count > 0, "query callbacks must read at least one component");

        detail::append_callback_terms<args>(_world, desc, term_index);

        ecs_query_id_t qid = ecs_query_init(_world, &desc);

        detail::run_query<F, args>(func, _world, qid);

        ecs_query_fini(_world, qid);
    }

    void first() { return; }
};

} // namespace ecs
