#pragma once
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ecs {

template <class T> consteval std::string_view type_name() {
#if defined(__clang__)
    constexpr std::string_view prefix = "std::string_view type_name() [T = ";
    constexpr std::string_view suffix = "]";
    constexpr std::string_view func = __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
    constexpr std::string_view prefix = "consteval std::string_view type_name() [with T = ";
    constexpr std::string_view suffix = "]";
    constexpr std::string_view func = __PRETTY_FUNCTION__;
#else
#error "type_name<T>() only supports GCC and Clang"
#endif

    constexpr auto start = prefix.size();
    constexpr auto end = func.size() - suffix.size();
    return func.substr(start, end - start);
}

template <typename T> struct function_traits;

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
struct function_traits : function_traits<decltype(&std::remove_reference_t<F>::operator())> {};

template <typename Tuple, typename Fn, std::size_t... I>
constexpr void for_each_type_impl(Fn &&fn, std::index_sequence<I...>) {
    (fn.template operator()<std::tuple_element_t<I, Tuple>>(), ...);
}

template <typename Tuple, typename Fn> constexpr void for_each_type(Fn &&fn) {
    constexpr std::size_t N = std::tuple_size_v<Tuple>;
    for_each_type_impl<Tuple>(std::forward<Fn>(fn), std::make_index_sequence<N>{});
}

} // namespace ecs
