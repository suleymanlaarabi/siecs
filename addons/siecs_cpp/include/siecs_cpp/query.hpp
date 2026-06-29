#pragma once
#include "component.hpp"
#include "function_traits.hpp"
#include "resource.hpp"
#include "siecs.h"
#include "siecs_cpp/entity.hpp"
#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

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

template <std::uint8_t SharedMask, std::size_t I, typename Args, typename Tuple>
inline decltype(auto) row_arg(Tuple &fields, uint32_t row) {
    auto &field = std::get<I>(fields);

    using arg = std::tuple_element_t<I, Args>;
    if constexpr (is_res_v<arg>) {
        (void)row;
        return field;
    } else {
        constexpr std::size_t field_index = field_index_before<Args, I>();
        if constexpr ((SharedMask & (1U << field_index)) != 0) {
            (void)row;
            return field[0];
        } else {
            return field[row];
        }
    }
}

template <std::size_t I, typename Args, typename Tuple>
inline decltype(auto) row_arg_stride(Tuple &fields, uint32_t row, const uint32_t *strides) {
    auto &field = std::get<I>(fields);

    using arg = std::tuple_element_t<I, Args>;
    if constexpr (is_res_v<arg>) {
        (void)row;
        (void)strides;
        return field;
    } else {
        constexpr std::size_t field_index = field_index_before<Args, I>();
        return field[row * strides[field_index]];
    }
}

template <std::uint8_t SharedMask, typename F, typename Args, typename Tuple, std::size_t... Is>
inline void call_fields_impl(F &func, Tuple &fields, uint32_t count, std::index_sequence<Is...>) {
    for (uint32_t i = 0; i < count; i++) {
        std::invoke(func, row_arg<SharedMask, Is, Args>(fields, i)...);
    }
}

template <typename F, typename Args, typename Tuple, std::size_t... Is>
inline void call_fields_stride_impl(
    F &func,
    Tuple &fields,
    uint32_t count,
    const uint32_t *strides,
    std::index_sequence<Is...>
) {
    (void)strides;
    for (uint32_t i = 0; i < count; i++) {
        std::invoke(func, row_arg_stride<Is, Args>(fields, i, strides)...);
    }
}

template <std::uint8_t SharedMask, typename F, typename Args, typename Tuple>
inline void call_fields_mask(F &func, Tuple &fields, uint32_t count) {
    constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;

    call_fields_impl<SharedMask, F, Args>(func, fields, count, std::make_index_sequence<N>{});
}

template <typename F, typename Args, typename Tuple>
inline void call_fields_stride(F &func, Tuple &fields, uint32_t count, std::uint8_t shared_mask) {
    constexpr std::size_t component_count = component_arg_count<Args>();
    constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;

    uint32_t strides[component_count == 0 ? 1 : component_count]{};
    for (std::size_t i = 0; i < component_count; i++) {
        strides[i] = (shared_mask & (1U << i)) ? 0U : 1U;
    }

    call_fields_stride_impl<F, Args>(func, fields, count, strides, std::make_index_sequence<N>{});
}

template <typename F, typename Args, typename Tuple>
inline void call_fields(F &func, Tuple &fields, uint32_t count, std::uint8_t shared_mask) {
    switch (shared_mask) {
    case 0:
        call_fields_mask<0, F, Args, Tuple>(func, fields, count);
        break;
    case 1U << 0:
        call_fields_mask<1U << 0, F, Args, Tuple>(func, fields, count);
        break;
    case 1U << 1:
        call_fields_mask<1U << 1, F, Args, Tuple>(func, fields, count);
        break;
    case 1U << 2:
        call_fields_mask<1U << 2, F, Args, Tuple>(func, fields, count);
        break;
    case 1U << 3:
        call_fields_mask<1U << 3, F, Args, Tuple>(func, fields, count);
        break;
    case 1U << 4:
        call_fields_mask<1U << 4, F, Args, Tuple>(func, fields, count);
        break;
    case 1U << 5:
        call_fields_mask<1U << 5, F, Args, Tuple>(func, fields, count);
        break;
    case 1U << 6:
        call_fields_mask<1U << 6, F, Args, Tuple>(func, fields, count);
        break;
    case 1U << 7:
        call_fields_mask<1U << 7, F, Args, Tuple>(func, fields, count);
        break;
    default:
        call_fields_stride<F, Args>(func, fields, count, shared_mask);
        break;
    }
}

template <typename Args> inline std::uint8_t make_shared_mask(const ecs_iter_t *it) {
    constexpr std::size_t component_count = component_arg_count<Args>();
    static_assert(component_count <= 8, "query callbacks can read at most 8 components");

    std::uint8_t mask = 0;
    for (std::size_t i = 0; i < component_count; i++) {
        if (it->field_kinds[i] == EcsFieldShared) {
            mask |= static_cast<std::uint8_t>(1U << i);
        }
    }
    return mask;
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
    // assert(term_index + 1 < query_term_capacity && "too many query terms");

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
        call_fields<F, Args>(func, fields, it.count, make_shared_mask<Args>(&it));
    }
}

template <typename F, typename Args> inline void run_batch(F &func, ecs_iter_t *it) {
    auto resources = make_resources<Args>(it->world);
    auto fields =
        make_fields<Args>(it, resources, std::make_index_sequence<std::tuple_size_v<Args>>{});
    call_fields<F, Args>(func, fields, it->count, make_shared_mask<Args>(it));
}

template <typename F, typename Args> inline void run_once(F &func, ecs_world_t *world) {
    static_assert(component_arg_count<Args>() == 0);

    auto resources = make_resources<Args>(world);
    auto fields =
        make_fields<Args>(nullptr, resources, std::make_index_sequence<std::tuple_size_v<Args>>{});
    call_fields<F, Args>(func, fields, 1, 0);
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
