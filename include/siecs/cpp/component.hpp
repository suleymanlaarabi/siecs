#pragma once
#include "siecs/cpp/c_api.hpp"
#include "siecs/cpp/type.hpp"
#if SIECS_HAS_META && !defined(SIREFLECT_H)
#include "sireflect.h"
#endif
#include <cstddef>
#include <cstdio>
#include <memory>
#include <new>
#include <string.h>
#include <string>
#include <type_traits>
#include <utility>

namespace ecs {

/** Lifecycle callbacks for a typed component; callbacks do not own references. */
template <typename T> struct component_hooks {
    using on_set_t = void (*)(ecs_entity_t, const T &, T &);
    using on_remove_t = void (*)(ecs_entity_t, T &);
    using on_add_t = void (*)(ecs_entity_t, T &);

    on_set_t on_set = nullptr;
    on_remove_t on_remove = nullptr;
    on_add_t on_add = nullptr;
};

namespace detail {

template <typename T> struct component_type {
    static inline ecs_component_t id = 0;
};

template <typename T> struct component_hook_state {
    static inline component_hooks<T> hooks{};
};

template <typename T>
static void component_on_set(
    ecs_entity_t entity,
    ecs_component_t,
    const void *new_value,
    void *current_value
) {
    auto callback = component_hook_state<T>::hooks.on_set;
    if (callback != nullptr) {
        callback(entity, *static_cast<const T *>(new_value), *static_cast<T *>(current_value));
    }
}

template <typename T> static void component_on_remove(ecs_entity_t entity, ecs_component_t, void *value) {
    auto callback = component_hook_state<T>::hooks.on_remove;
    if (callback != nullptr) callback(entity, *static_cast<T *>(value));
}

template <typename T> static void component_on_add(ecs_entity_t entity, ecs_component_t, void *value) {
    auto callback = component_hook_state<T>::hooks.on_add;
    if (callback != nullptr) callback(entity, *static_cast<T *>(value));
}

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

template <typename T>
static ecs_component_t ecs_cpp_component_id(
    const component_hooks<T> *hooks = nullptr
) {
    ecs_component_t &cid = detail::component_type<T>::id;

    if (cid != 0) return cid;

#if SIECS_HAS_META
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
#endif

#if SIECS_HAS_NAMES
#if SIECS_HAS_META
    const char *component_name = reflection.name;
#else
    static const std::string name = std::string(type_name<T>());
    const char *component_name = name.c_str();
#endif
#endif

    if (hooks != nullptr) component_hook_state<T>::hooks = *hooks;

    ecs_component_desc_t desc = {
        SIECS_NAME_INIT(component_name)
        .size = sisizeof<T>(),
        .ops = value_ops<T>(),
        .on_set = hooks && hooks->on_set ? component_on_set<T> : nullptr,
        .on_remove = hooks && hooks->on_remove ? component_on_remove<T> : nullptr,
        .on_add = hooks && hooks->on_add ? component_on_add<T> : nullptr,
#if SIECS_HAS_META
        .struct_desc = &reflection,
#endif
    };

    cid = ecs_component_init(&desc);

    return cid;
}

template <typename T> struct relation_type {
    static inline ecs_relation_id_t id = 0;
};

template <typename T>
static ecs_relation_id_t ecs_cpp_relation_id(
    const ecs_relation_desc_t *desc = nullptr
) {
    ecs_relation_id_t &id = relation_type<T>::id;
    if (id) return id;
    static const ecs_relation_desc_t dense = {
        .storage = EcsRelationDense,
        .on_delete_target = EcsRemoveRelation,
        .acyclic = false,
    };
    static const std::string name = std::string(type_name<T>());
    id = ecs_relation_register(&id, name.c_str(), desc ? desc : &dense);
    return id;
}

#define fields_str(...) #__VA_ARGS__

#define reflected(...)                                                                             \
    static constexpr const char *fields = fields_str({ __VA_ARGS__ });                             \
    __VA_ARGS__

} // namespace detail

} // namespace ecs
