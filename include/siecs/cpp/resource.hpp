#pragma once
#include "siecs/cpp/component.hpp"
#include "siecs/cpp/system_param.hpp"
#include "siecs/cpp/type.hpp"
#include <cassert>
#include <string>
#include <tuple>
#include <type_traits>

namespace ecs {

/** Optional callbacks invoked around replacement/removal of resource `T`. */
template <typename T> struct resource_hooks {
    using on_set_t = void (*)(const T &);
    using on_remove_t = void (*)(const T &);

    on_set_t on_set = nullptr;
    on_remove_t on_remove = nullptr;
};

/** Borrowed resource argument passed to typed system/observer callbacks. */
template <typename T> class res {
    T *_ptr = nullptr;

  public:
    res() noexcept = default;
    /** Construct from callback-owned storage; `ptr` must not be null. */
    explicit res(T *ptr) noexcept : _ptr(ptr) { assert(ptr != nullptr); }

    static res init() noexcept { return {}; }
    static void query(system_param_query &query) {
        if constexpr (std::is_const_v<T>)
            query.read_resource<std::remove_const_t<T>>();
        else
            query.write_resource<T>();
    }
    void world(ecs_world_t *world) noexcept;

    /** Access the borrowed resource member. */
    [[nodiscard]] T *operator->() const noexcept { return _ptr; }
    /** Access the borrowed resource value. */
    [[nodiscard]] T &operator*() const noexcept { return *_ptr; }
    /** Return the borrowed pointer without transferring ownership. */
    [[nodiscard]] T *get() const noexcept { return _ptr; }
};

namespace detail {

template <typename T>
concept c_declared_resource = c_resource_traits<std::remove_cv_t<T>>::value;

template <typename T> struct resource_hook_state {
    static inline resource_hooks<T> hooks{};
};

template <typename T, bool Set> static void resource_hook(const void *ptr) {
    auto callback =
        Set ? resource_hook_state<T>::hooks.on_set : resource_hook_state<T>::hooks.on_remove;
    if (callback != nullptr)
        callback(*static_cast<const T *>(ptr));
}

} // namespace detail

/** Register `T` and return its typed resource id, installing native hooks once. */
template <typename T>
static ecs_resource_t
ecs_cpp_resource_id(const resource_hooks<std::remove_cv_t<T>> *hooks = nullptr) {
    using type = std::remove_cv_t<T>;
    if constexpr (detail::c_declared_resource<type>) {
        (void)hooks;
        return ecs_resource_register(
            detail::c_resource_traits<type>::id_storage(),
            detail::c_resource_traits<type>::desc_storage()
        );
    }
    ecs_resource_t &rid = detail::typed_id<type, detail::id_kind::resource>();

    if (rid != 0)
        return rid;

    static const std::string name = std::string(type_name<type>());

    if (hooks != nullptr)
        detail::resource_hook_state<type>::hooks = *hooks;

    ecs_resource_desc_t desc = {
        .name = name.c_str(),
        .size = sizeof(type),
        .ops = detail::value_ops<type>(),
        .on_set = hooks && hooks->on_set ? detail::resource_hook<type, true> : nullptr,
        .on_remove = hooks && hooks->on_remove ? detail::resource_hook<type, false> : nullptr,
    };

    rid = ecs_resource_init(&desc);
    return rid;
}

template <typename T> static ecs_resource_t ecs_cpp_try_resource_id() {
    using type = std::remove_cv_t<T>;
    if constexpr (detail::c_declared_resource<type>) {
        return *detail::c_resource_traits<type>::id_storage();
    }
    return detail::typed_id<type, detail::id_kind::resource>();
}

/** Non-owning resource handle; presence checks never register an absent type. */
template <typename T> class resource_ref {
    using value_type = std::remove_cv_t<T>;

  public:
    [[nodiscard]] ecs_resource_t id() const { return ecs_cpp_resource_id<value_type>(); }
    template <typename U>
        requires(!std::is_const_v<T> && std::is_same_v<std::remove_cvref_t<U>, value_type>)
    void set(U &&value) const {
        if constexpr (std::is_lvalue_reference_v<U>)
            ecs_set_resource_rid(id(), &value);
        else
            ecs_move_resource_rid(id(), &value);
    }
    [[nodiscard]] const value_type *try_get() const {
        auto rid = ecs_cpp_try_resource_id<value_type>();
        return rid ? static_cast<const value_type *>(ecs_try_resource_rid(rid)) : nullptr;
    }
    [[nodiscard]] const value_type &get() const {
        return *static_cast<const value_type *>(ecs_resource_rid(id()));
    }
    [[nodiscard]] value_type *try_edit() const
        requires(!std::is_const_v<T>)
    {
        return const_cast<value_type *>(try_get());
    }
    [[nodiscard]] value_type &edit() const
        requires(!std::is_const_v<T>)
    {
        return const_cast<value_type &>(get());
    }
    [[nodiscard]] bool has() const { return try_get() != nullptr; }
    void remove() const
        requires(!std::is_const_v<T>)
    {
        auto rid = ecs_cpp_try_resource_id<value_type>();
        if (rid)
            ecs_remove_resource_rid(rid);
    }
};

/** Access a resource without registering its type until storage or id is needed. */
template <typename T> resource_ref<T> resource() { return {}; }
/** Install resource lifecycle hooks before the first registration. */
template <typename T>
    requires(!detail::c_declared_resource<T>)
resource_ref<T> resource(const resource_hooks<std::remove_cv_t<T>> &hooks) {
    (void)ecs_cpp_resource_id<T>(&hooks);
    return {};
}

template <typename T> void res<T>::world(ecs_world_t *world) noexcept {
    (void)world;
    _ptr = static_cast<T *>(ecs_resource_rid(ecs_cpp_resource_id<std::remove_cv_t<T>>()));
}

} // namespace ecs
