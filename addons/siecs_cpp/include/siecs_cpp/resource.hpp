#pragma once

#include "siecs.h"
#include "siecs_cpp/component.hpp"
#include <cassert>
#include <tuple>
#include <type_traits>

namespace ecs {

template <typename T> class res {
    T *_ptr = nullptr;

  public:
    explicit res(T *ptr) noexcept : _ptr(ptr) { assert(ptr != nullptr); }

    [[nodiscard]] T *operator->() const noexcept { return _ptr; }
    [[nodiscard]] T &operator*() const noexcept { return *_ptr; }
    [[nodiscard]] T *get() const noexcept { return _ptr; }
};

namespace detail {

template <typename T> struct is_res : std::false_type {};
template <typename T> struct is_res<ecs::res<T>> : std::true_type {};

template <typename T> inline constexpr bool is_res_v = is_res<std::remove_cvref_t<T>>::value;

template <typename T> struct res_value;
template <typename T> struct res_value<ecs::res<T>> {
    using type = T;
};

template <typename T>
using res_value_t = typename res_value<std::remove_cvref_t<T>>::type;

template <typename T>
using resource_component_t = std::remove_cv_t<res_value_t<T>>;

struct no_resource {};

template <typename Arg> inline auto make_resource_arg(ecs_world_t *world) {
    if constexpr (is_res_v<Arg>) {
        using value_type = res_value_t<Arg>;
        using component_type = std::remove_cv_t<value_type>;

        ecs_component_t id = ecs::ecs_cpp_component_id<component_type>(world);
        return ecs::res<value_type>(static_cast<value_type *>(ecs_resource_cid(world, id)));
    } else {
        return no_resource{};
    }
}

template <typename Args, std::size_t... Is>
inline auto make_resources(ecs_world_t *world, std::index_sequence<Is...>) {
    return std::tuple{ make_resource_arg<std::tuple_element_t<Is, Args>>(world)... };
}

template <typename Args> inline auto make_resources(ecs_world_t *world) {
    constexpr std::size_t N = std::tuple_size_v<Args>;
    return make_resources<Args>(world, std::make_index_sequence<N>{});
}

} // namespace detail

} // namespace ecs
