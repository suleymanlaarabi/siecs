#pragma once

#include "siecs/cpp/component.hpp"
#include "siecs/cpp/type.hpp"
#include <cassert>
#include <string>
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

template <typename T> struct resource_type {
    static inline ecs_resource_t id;
};

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
using resource_value_t = std::remove_cv_t<res_value_t<T>>;

struct no_resource {};

} // namespace detail

template <typename T> static ecs_resource_t ecs_cpp_resource_id() {
    using type = std::remove_cv_t<T>;
    ecs_resource_t &rid = detail::resource_type<type>::id;

    if (rid != 0) return rid;

    static const std::string name = std::string(type_name<type>());

    ecs_resource_desc_t desc = {
        .name = name.c_str(),
        .size = sizeof(type),
        .ops = detail::value_ops<type>(),
        .on_set = nullptr,
        .on_remove = nullptr,
    };

    rid = ecs_resource_init(&desc);
    return rid;
}

template <typename T> static ecs_resource_t ecs_cpp_try_resource_id() {
    using type = std::remove_cv_t<T>;
    return detail::resource_type<type>::id;
}

namespace detail {

template <typename Arg> inline auto make_resource_arg() {
    if constexpr (is_res_v<Arg>) {
        using value_type = res_value_t<Arg>;
        using resource_type = std::remove_cv_t<value_type>;

        ecs_resource_t id = ecs_cpp_resource_id<resource_type>();
        return ecs::res<value_type>(static_cast<value_type *>(ecs_resource_rid(id)));
    } else {
        return no_resource{};
    }
}

template <typename Args, std::size_t... Is>
inline auto make_resources(std::index_sequence<Is...>) {
    return std::tuple{ make_resource_arg<std::tuple_element_t<Is, Args>>()... };
}

template <typename Args> inline auto make_resources() {
    constexpr std::size_t N = std::tuple_size_v<Args>;
    return make_resources<Args>(std::make_index_sequence<N>{});
}

} // namespace detail

} // namespace ecs
