#pragma once
#include "component.hpp"
#include "function_traits.hpp"
#include "resource.hpp"
#include "siecs.h"
#include "siecs_cpp/entity.hpp"
#include <functional>

namespace ecs {

namespace detail {

template <typename T> consteval ecs_term_access_t term_access();

template <typename Args, std::size_t I, std::size_t... Is>
consteval std::size_t field_index_before_impl(std::index_sequence<Is...>) {
    return ((Is < I && !is_res_v<std::tuple_element_t<Is, Args>> ? 1U : 0U) + ... + 0U);
}

template <typename Args, std::size_t I> consteval std::size_t field_index_before() {
    return field_index_before_impl<Args, I>(std::make_index_sequence<std::tuple_size_v<Args>>{});
}

template <typename Args, std::size_t... Is>
consteval std::size_t component_arg_count_impl(std::index_sequence<Is...>) {
    return ((!is_res_v<std::tuple_element_t<Is, Args>> ? 1U : 0U) + ... + 0U);
}

template <typename Args> consteval std::size_t component_arg_count() {
    return component_arg_count_impl<Args>(std::make_index_sequence<std::tuple_size_v<Args>>{});
}

template <typename Args, std::size_t I, typename Resources>
inline auto make_batch_arg(ecs_iter_t *it, Resources &resources) {
    using arg = std::tuple_element_t<I, Args>;

    if constexpr (is_res_v<arg>) {
        return std::get<I>(resources);
    } else {
        using value_type = std::remove_reference_t<arg>;
        return static_cast<value_type *>(
            ecs_field(it, static_cast<uint16_t>(field_index_before<Args, I>()))
        );
    }
}

template <typename Args, typename Resources, std::size_t... Is>
inline auto make_fields(ecs_iter_t *it, Resources &resources, std::index_sequence<Is...>) {
    (void)it;
    return std::tuple{ make_batch_arg<Args, Is>(it, resources)... };
}

template <std::size_t I, typename Tuple>
inline decltype(auto) row_arg(Tuple &fields, uint32_t row) {
    auto &field = std::get<I>(fields);

    if constexpr (is_res_v<std::remove_reference_t<decltype(field)>>) {
        (void)row;
        return field;
    } else {
        return field[row];
    }
}

template <typename F, typename Tuple, std::size_t... Is>
inline void call_fields_impl(F &func, Tuple &fields, uint32_t count, std::index_sequence<Is...>) {
    for (uint32_t i = 0; i < count; i++) {
        std::invoke(func, row_arg<Is>(fields, i)...);
    }
}

template <typename F, typename Tuple>
inline void call_fields(F &func, Tuple &fields, uint32_t count) {
    constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;

    call_fields_impl(func, fields, count, std::make_index_sequence<N>{});
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
    (append_term(desc, term_index, ecs::detail::ecs_cpp_component_id<T>(world), access), ...);
}

template <typename Args>
inline void
append_callback_terms(ecs_world_t *world, ecs_query_desc_t &desc, uint16_t &term_index) {
    for_each_type<Args>([&]<typename T>() {
        if constexpr (!is_res_v<T>) {
            append_term(
                desc,
                term_index,
                ecs::detail::ecs_cpp_component_id<std::remove_cvref_t<T>>(world),
                term_access<T>()
            );
        }
    });
}

template <typename F, typename Args>
inline void run_query(F &func, ecs_world_t *world, ecs_query_id_t qid) {
    auto resources = make_resources<Args>(world);
    ecs_iter_t it = ecs_query_iter(world, qid);

    while (ecs_iter_next(&it)) {
        auto fields =
            make_fields<Args>(&it, resources, std::make_index_sequence<std::tuple_size_v<Args>>{});
        call_fields(func, fields, it.count);
    }
}

template <typename F, typename Args> inline void run_batch(F &func, ecs_iter_t *it) {
    auto resources = make_resources<Args>(it->world);
    auto fields =
        make_fields<Args>(it, resources, std::make_index_sequence<std::tuple_size_v<Args>>{});
    call_fields(func, fields, it->count);
}

template <typename F, typename Args> inline void run_once(F &func, ecs_world_t *world) {
    static_assert(component_arg_count<Args>() == 0);

    auto resources = make_resources<Args>(world);
    auto fields =
        make_fields<Args>(nullptr, resources, std::make_index_sequence<std::tuple_size_v<Args>>{});
    call_fields(func, fields, 1);
}

template <typename F, typename Args>
inline void run_strict_query(F &func, ecs_world_t *world, ecs_query_id_t qid) {
    constexpr std::size_t component_count = component_arg_count<Args>();
    static_assert(component_count > 0, "query callbacks must read at least one component");

    run_query<F, Args>(func, world, qid);
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

    ecs_query_id_t build() { return ecs_query_init(_world, &desc); }

    template <typename F> void each(F &&func) {
        using traits = function_traits<std::remove_reference_t<F>>;
        using args = typename traits::args_tuple;
        constexpr std::size_t component_count = detail::component_arg_count<args>();
        static_assert(component_count > 0, "query callbacks must read at least one component");

        detail::append_callback_terms<args>(_world, desc, term_index);

        ecs_query_id_t qid = this->build();

        detail::run_strict_query<F, args>(func, _world, qid);

        ecs_query_fini(_world, qid);
    }

    entity first() {
        ecs_query_id_t qid = this->build();
        ecs_iter_t it = ecs_query_iter(_world, qid);
        while (ecs_iter_next(&it)) {
            return entity(_world, it.entities[0]);
        }
        return entity::null();
    }
};

} // namespace ecs
