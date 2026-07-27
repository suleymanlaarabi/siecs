#pragma once
#include "siecs/cpp/c_api.hpp"
#include "siecs/cpp/type.hpp"
#include "sireflect.h"
#include <cstddef>
#include <cstdio>
#include <memory>
#include <new>
#include <string.h>
#include <string>
#include <type_traits>
#include <utility>

namespace ecs {

namespace detail {

template <typename T> struct component_type {
    static inline ecs_component_t id = 0;
};

template <typename T, typename = void> struct is_complete : std::false_type {};

template <typename T> struct is_complete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};

template <typename T> consteval size_t sisizeof() {
    if constexpr (is_complete<T>::value) {
        return sizeof(T);
    } else {
        return 0;
    }
}

template <typename T> static void value_ctor(void *ptr, uint32_t count) {
    T *values = static_cast<T *>(ptr);
    for (uint32_t i = 0; i < count; i++) {
        std::construct_at(&values[i]);
    }
}

template <typename T> static void value_dtor(void *ptr, uint32_t count) {
    T *values = static_cast<T *>(ptr);
    for (uint32_t i = 0; i < count; i++) {
        std::destroy_at(&values[i]);
    }
}

template <typename T> static void value_copy_ctor(void *dst, const void *src, uint32_t count) {
    T *out = static_cast<T *>(dst);
    const T *in = static_cast<const T *>(src);
    for (uint32_t i = 0; i < count; i++) {
        std::construct_at(&out[i], in[i]);
    }
}

template <typename T> static void value_copy(void *dst, const void *src, uint32_t count) {
    T *out = static_cast<T *>(dst);
    const T *in = static_cast<const T *>(src);
    for (uint32_t i = 0; i < count; i++) {
        if constexpr (std::is_copy_assignable_v<T>) {
            out[i] = in[i];
        } else {
            std::destroy_at(&out[i]);
            std::construct_at(&out[i], in[i]);
        }
    }
}

template <typename T> static void value_move_ctor(void *dst, void *src, uint32_t count) {
    T *out = static_cast<T *>(dst);
    T *in = static_cast<T *>(src);
    for (uint32_t i = 0; i < count; i++) {
        std::construct_at(&out[i], std::move(in[i]));
        std::destroy_at(&in[i]);
    }
}

template <typename T> static void value_move(void *dst, void *src, uint32_t count) {
    T *out = static_cast<T *>(dst);
    T *in = static_cast<T *>(src);
    for (uint32_t i = 0; i < count; i++) {
        if constexpr (std::is_move_assignable_v<T>) {
            out[i] = std::move(in[i]);
        } else {
            std::destroy_at(&out[i]);
            std::construct_at(&out[i], std::move(in[i]));
        }
        std::destroy_at(&in[i]);
    }
}

template <typename T> consteval ecs_type_ops_t value_ops() {
    if constexpr (!is_complete<T>::value) {
        return {};
    } else if constexpr (sizeof(T) == 0 || std::is_trivially_copyable_v<T>) {
        return {};
    } else {
        return {
            .ctor = std::is_default_constructible_v<T> ? value_ctor<T> : nullptr,
            .dtor = std::is_destructible_v<T> ? value_dtor<T> : nullptr,
            .copy_ctor = std::is_copy_constructible_v<T> ? value_copy_ctor<T> : nullptr,
            .copy = std::is_copy_constructible_v<T> ? value_copy<T> : nullptr,
            .move_ctor = std::is_move_constructible_v<T> ? value_move_ctor<T> : nullptr,
            .move = std::is_move_constructible_v<T> ? value_move<T> : nullptr,
        };
    }
}

template <typename T> static ecs_component_t ecs_cpp_component_id() {
    ecs_component_t &cid = detail::component_type<T>::id;

    if (cid != 0) return cid;

    static sireflect_struct_desc_t reflection = {
        .name = strdup(std::string(type_name<T>()).c_str()),
        .fields = "{}",
        .size = 0,
        .align = 1,
    };

    if constexpr (requires {
                      { T::fields } -> std::convertible_to<const char *>;
                  }) {
        reflection.fields = T::fields;
        reflection.size = sisizeof<T>();
        reflection.align = _Alignof(T);
    }

    ecs_component_desc_t desc = {
        .name = reflection.name,
        .size = sisizeof<T>(),
        .ops = value_ops<T>(),
        .on_set = nullptr,
        .on_remove = nullptr,
        .on_add = nullptr,
        .relation_flags = 0,
        .struct_desc = &reflection,
    };

    cid = ecs_component_init(&desc);

    return cid;
}

#define fields_str(...) #__VA_ARGS__

#define reflected(...)                                                                             \
    static constexpr const char *fields = fields_str({ __VA_ARGS__ });                             \
    __VA_ARGS__

} // namespace detail

} // namespace ecs
