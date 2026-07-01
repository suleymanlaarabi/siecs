#pragma once
#include <string_view>

namespace ecs {

template <class T> consteval std::string_view type_name() {
    constexpr std::string_view func = __PRETTY_FUNCTION__;
    constexpr std::string_view key = "T = ";

    constexpr auto start = func.find(key) + key.size();
    constexpr auto end_semi = func.find(';', start);
    constexpr auto end_bracket = func.find(']', start);

    constexpr auto end = end_semi == std::string_view::npos ? end_bracket : end_semi;

    return func.substr(start, end - start);
}

} // namespace ecs
