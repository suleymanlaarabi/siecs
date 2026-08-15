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

/** Lifecycle callbacks for a typed component; callbacks do not own references. */
template <typename T> struct component_hooks {
    using on_set_t = void (*)(ecs_entity_t, const T &, T &);
    using on_remove_t = void (*)(ecs_entity_t, T &);
    using on_add_t = void (*)(ecs_entity_t, T &);

    on_set_t on_set = nullptr;
    on_remove_t on_remove = nullptr;
    on_add_t on_add = nullptr;
};

/** Registration options for a native C++ component. */
template <typename T> struct component_options {
    component_hooks<T> hooks{};
    ecs_component_inheritance_t inheritance = EcsInheritOwned;
};

namespace detail {

enum class id_kind { component, relation, resource };
template <typename T, id_kind Kind> inline uint16_t typed_id = 0;

template <typename T>
concept c_declared_component = c_component_traits<std::remove_cv_t<T>>::value;

template <typename T> struct component_hook_state {
    static inline component_hooks<T> hooks{};
};

template <typename T>
static void
component_on_set(ecs_entity_t entity, ecs_component_t, const void *new_value, void *current_value) {
    auto callback = component_hook_state<T>::hooks.on_set;
    if (callback != nullptr) {
        callback(entity, *static_cast<const T *>(new_value), *static_cast<T *>(current_value));
    }
}

template <typename T, bool Add>
static void component_hook(ecs_entity_t entity, ecs_component_t, void *value) {
    auto callback = Add ? component_hook_state<T>::hooks.on_add
                        : component_hook_state<T>::hooks.on_remove;
    if (callback != nullptr)
        callback(entity, *static_cast<T *>(value));
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

template <typename T, bool Destroy> static void value_lifetime(void *ptr, uint32_t count) {
    T *values = static_cast<T *>(ptr);
    for (uint32_t i = 0; i < count; i++) {
        if constexpr (Destroy) std::destroy_at(&values[i]);
        else std::construct_at(&values[i]);
    }
}

template <typename T, bool Move, bool Construct>
static void value_transfer(
    void *dst,
    std::conditional_t<Move, void *, const void *> src,
    uint32_t count
) {
    T *out = static_cast<T *>(dst);
    using input = std::conditional_t<Move, T, const T>;
    input *in = static_cast<input *>(src);
    for (uint32_t i = 0; i < count; i++) {
        if constexpr (Construct) {
            if constexpr (Move) std::construct_at(&out[i], std::move(in[i]));
            else std::construct_at(&out[i], in[i]);
        } else if constexpr (Move ? std::is_move_assignable_v<T>
                                  : std::is_copy_assignable_v<T>) {
            if constexpr (Move) out[i] = std::move(in[i]);
            else out[i] = in[i];
        } else {
            std::destroy_at(&out[i]);
            if constexpr (Move) std::construct_at(&out[i], std::move(in[i]));
            else std::construct_at(&out[i], in[i]);
        }
        if constexpr (Move) std::destroy_at(&in[i]);
    }
}

template <typename T> consteval ecs_type_ops_t value_ops() {
    if constexpr (!is_complete<T>::value) {
        return {};
    } else if constexpr (sizeof(T) == 0 || std::is_trivially_copyable_v<T>) {
        return {};
    } else {
        return {
            .ctor = std::is_default_constructible_v<T> ? value_lifetime<T, false> : nullptr,
            .dtor = std::is_destructible_v<T> ? value_lifetime<T, true> : nullptr,
            .copy_ctor = std::is_copy_constructible_v<T> ? value_transfer<T, false, true> : nullptr,
            .copy = std::is_copy_constructible_v<T> ? value_transfer<T, false, false> : nullptr,
            .move_ctor = std::is_move_constructible_v<T> ? value_transfer<T, true, true> : nullptr,
            .move = std::is_move_constructible_v<T> ? value_transfer<T, true, false> : nullptr,
        };
    }
}

template <typename T>
static ecs_component_t ecs_cpp_component_id(
    const component_hooks<T> *hooks = nullptr,
    ecs_component_inheritance_t inheritance = EcsInheritOwned
) {
    using type = std::remove_cv_t<T>;
    if constexpr (c_declared_component<type>) {
        (void)hooks;
        return ecs_component_register(
            c_component_traits<type>::id_storage(),
            c_component_traits<type>::desc_storage()
        );
    }

    ecs_component_t &cid = typed_id<T, id_kind::component>;

    if (cid != 0)
        return cid;

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
    if (hooks != nullptr)
        component_hook_state<T>::hooks = *hooks;

    ecs_component_desc_t desc = {
        .name = reflection.name,
        .size = sisizeof<T>(),
        .ops = value_ops<T>(),
        .on_set = hooks && hooks->on_set ? component_on_set<T> : nullptr,
        .on_remove = hooks && hooks->on_remove ? component_hook<T, false> : nullptr,
        .on_add = hooks && hooks->on_add ? component_hook<T, true> : nullptr,
        .struct_desc = &reflection,
        .inheritance = inheritance,
    };

    cid = ecs_component_init(&desc);

    return cid;
}

template <typename T>
static ecs_relation_id_t ecs_cpp_relation_id(const ecs_relation_desc_t *desc = nullptr) {
    using type = std::remove_cv_t<T>;
    if constexpr (c_relation_traits<type>::value) {
        (void)desc;
        return ecs_relation_register(
            c_relation_traits<type>::id_storage(),
            c_relation_traits<type>::relation_name(),
            c_relation_traits<type>::desc_storage()
        );
    }

    ecs_relation_id_t &id = typed_id<T, id_kind::relation>;
    if (id)
        return id;
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
