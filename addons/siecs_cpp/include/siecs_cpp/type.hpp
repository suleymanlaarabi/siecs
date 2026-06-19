#pragma once
#include <string_view>

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

} // namespace ecs
