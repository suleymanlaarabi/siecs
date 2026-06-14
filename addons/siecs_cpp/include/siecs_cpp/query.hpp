#pragma once

#include "siecs.h"
#include "siecs_cpp/component.hpp"
#include "siecs_cpp/type.hpp"
#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ecs {

template <typename F, typename Tuple, std::size_t... Is>
inline void call_fields_impl(F &&func, Tuple &fields, uint32_t count, std::index_sequence<Is...>) {
    for (uint32_t i = 0; i < count; ++i) {
        std::invoke(std::forward<F>(func), (std::get<Is>(fields)[i])...);
    }
}

template <typename F, typename Tuple>
inline void call_fields(F &&func, Tuple &fields, uint32_t count) {
    constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;

    call_fields_impl(std::forward<F>(func), fields, count, std::make_index_sequence<N>{});
}

template <typename Args, std::size_t... Is>
auto make_fields(ecs_iter_t *it, std::index_sequence<Is...>) {
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

class world;

class query {
    ecs_query_desc_t desc{};
    uint16_t term_index = 0;
    ecs_world_t *_world = nullptr;

  public:
    explicit query(ecs_world_t *world) noexcept : _world(world) {}

    template <typename... T> query &&require() {

        (
            [&] {
                desc.terms[term_index].id = ecs::ecs_cpp_component_id<T>(_world);
                desc.terms[term_index].access = EcsFilter;
                term_index += 1;
            }(),
            ...);

        return std::move(*this);
    }

    template <typename... T> query &&exclude() {

        (
            [&] {
                desc.terms[term_index].id = ecs::ecs_cpp_component_id<T>(_world);
                desc.terms[term_index].access = EcsNot;
                term_index += 1;
            }(),
            ...);

        return std::move(*this);
    }

    template <typename F> void each(F &&func) {
        using traits = function_traits<std::remove_reference_t<F>>;
        using args = typename traits::args_tuple;
        constexpr std::size_t arg_count = std::tuple_size_v<args>;
        static_assert(arg_count > 0, "query callbacks must read at least one component");

        for_each_type<args>([&]<typename T>() {
            desc.terms[term_index].id = ecs::ecs_cpp_component_id<std::remove_cvref_t<T>>(_world);
            desc.terms[term_index].access = term_access<T>();
            term_index += 1;
        });

        ecs_query_id_t qid = ecs_query_init(_world, &desc);
        ecs_iter_t it = ecs_query_iter(_world, qid);

        while (ecs_iter_next(&it)) {
            auto fields = make_fields<args>(&it, std::make_index_sequence<arg_count>{});

            call_fields(std::forward<F>(func), fields, it.count);
        }

        ecs_query_fini(_world, qid);
    }

    void first() { return; }
};

} // namespace ecs
