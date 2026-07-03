#pragma once

#include "siecs.h"
#include "sireflect.h"
#include "siecs/cpp/type.hpp"
#include <cstddef>
#include <cstdio>
#include <string.h>
#include <string>

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

template <typename T> static void ecs_cpp_set_component_id(ecs_component_t cid) {
    detail::component_type<T>::id = cid;
}

template <typename T> static ecs_component_t ecs_cpp_component_id(ecs_world_t *world) {
    ecs_component_t &cid = detail::component_type<T>::id;

    if (cid != 0) {
        return cid;
    }

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
        .on_set = nullptr,
        .on_remove = []([[maybe_unused]] ecs_world_t *world,
                        [[maybe_unused]] ecs_entity_t entity,
                        [[maybe_unused]] ecs_component_t component,
                        void *ptr) { static_cast<const T *>(ptr)->~T(); },
        .on_add = []([[maybe_unused]] ecs_world_t *world,
                     [[maybe_unused]] ecs_entity_t entity,
                     [[maybe_unused]] ecs_component_t component,
                     void *ptr) { new (const_cast<void *>(ptr)) T(); },
        .relation_flags = 0,
        .struct_desc = &reflection,
    };

    cid = ecs_component_init(world, &desc);

    return cid;
}

#define fields_str(...) #__VA_ARGS__

#define reflected(...)                                                                             \
    static constexpr const char *fields = fields_str({ __VA_ARGS__ });                             \
    __VA_ARGS__

} // namespace detail

} // namespace ecs
