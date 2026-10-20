#pragma once
#include <tuple>
#include <type_traits>
#include <utility>

namespace ecs {

template <typename T, typename = void> struct function_traits {
    static_assert(
        sizeof(T) == 0,
        "Cannot infer callback arguments: use explicitly typed parameters or ecs::query<Args...>() "
        "for a generic lambda"
    );
    using args_tuple = std::tuple<>;
};

// function pointer
template <typename R, typename... Args> struct function_traits<R (*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
};

// function reference
template <typename R, typename... Args> struct function_traits<R (&)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
};

// member function pointer const
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
};

// member function pointer non-const
template <typename C, typename R, typename... Args> struct function_traits<R (C::*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
};

// lambda / functor
template <typename F>
struct function_traits<F, std::void_t<decltype(&F::operator())>>
    : function_traits<decltype(&F::operator())> {};

namespace detail {
template <typename Callback, typename... Signature> struct callback_args {
    static_assert(
        std::is_invocable_v<Callback &, Signature...>,
        "Callback does not accept the declared query signature"
    );
    using type = std::tuple<Signature...>;
};
template <typename Callback> struct callback_args<Callback> {
    using type = typename function_traits<Callback>::args_tuple;
};
template <typename Callback, typename... Signature>
using callback_args_t = typename callback_args<Callback, Signature...>::type;
} // namespace detail

template <typename Tuple, typename Fn, std::size_t... I>
constexpr void for_each_type_impl(Fn &&fn, std::index_sequence<I...>) {
    (fn.template operator()<std::tuple_element_t<I, Tuple>>(), ...);
}

template <typename Tuple, typename Fn> constexpr void for_each_type(Fn &&fn) {
    constexpr std::size_t N = std::tuple_size_v<Tuple>;
    for_each_type_impl<Tuple>(std::forward<Fn>(fn), std::make_index_sequence<N>{});
}

} // namespace ecs
