#pragma once

#include "siecs.h"
#include "type.hpp"
#include <new>
#include <string>
#include <type_traits>

namespace ecs {

template <typename T, typename = void> struct is_complete : std::false_type {};

template <typename T> struct is_complete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};

template <typename T> constexpr size_t sisizeof() {
    if constexpr (is_complete<T>::value) {
        return sizeof(T);
    } else {
        return 0;
    }
}

template <typename T> static ecs_component_t ecs_cpp_component_id(ecs_world_t *world) {
    static ecs_component_t cid = 0;

    if (cid != 0) {
        return cid;
    }

    std::string name = std::string(type_name<T>());

    ecs_component_desc_t desc = {
        .name = name.c_str(),
        .size = sisizeof<T>(),
        .on_remove =
            [](ecs_world_t *world, ecs_entity_t, ecs_component_t, const void *ptr) {
                static_cast<const T *>(ptr)->~T();
            },
        .on_add =
            [](ecs_world_t *world, ecs_entity_t, ecs_component_t, const void *ptr) {
                new (const_cast<void *>(ptr)) T();
            },
        .struct_desc = NULL,
    };

    cid = ecs_component_init(world, &desc);

    return cid;
}

} // namespace ecs
