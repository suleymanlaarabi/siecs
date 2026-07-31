#pragma once
#include "siecs/cpp/component.hpp"
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

/** Non-owning, typed handle to one per-world resource value. */
template <typename T> class resource_ref {
    using value_type = std::remove_cv_t<T>;
    ecs_resource_t _id;

  public:
    /** Adopt a registered resource id; the world owns the storage. */
    explicit resource_ref(ecs_resource_t id) noexcept : _id(id) {}

    template <typename U = T>
        requires(!std::is_const_v<U>)
    /** Copy `value` into the resource; the resource must be writable. */
    void set(const value_type &value) const {
        ecs_set_resource_rid(_id, &value);
    }

    template <typename U = T>
        requires(!std::is_const_v<U>)
    /** Move `value` into the resource, consuming its source state. */
    void set(value_type &&value) const {
        ecs_move_resource_rid(_id, &value);
    }

    /** Return storage or null when the resource is absent. */
    [[nodiscard]] T *try_get() const noexcept {
        return static_cast<T *>(ecs_try_resource_rid(_id));
    }

    /** Return storage; calling this when absent is invalid. */
    [[nodiscard]] T &get() const { return *static_cast<T *>(ecs_resource_rid(_id)); }
    /** Return whether the resource currently exists. */
    [[nodiscard]] bool has() const { return ecs_has_resource_rid(_id); }
    /** Remove the resource if present. */
    void remove() const { ecs_remove_resource_rid(_id); }
    /** Return the underlying resource id. */
    [[nodiscard]] ecs_resource_t id() const noexcept { return _id; }
};

/** Borrowed resource argument passed to typed system/observer callbacks. */
template <typename T> class res {
    T *_ptr = nullptr;

  public:
    /** Construct from callback-owned storage; `ptr` must not be null. */
    explicit res(T *ptr) noexcept : _ptr(ptr) { assert(ptr != nullptr); }

    /** Access the borrowed resource member. */
    [[nodiscard]] T *operator->() const noexcept { return _ptr; }
    /** Access the borrowed resource value. */
    [[nodiscard]] T &operator*() const noexcept { return *_ptr; }
    /** Return the borrowed pointer without transferring ownership. */
    [[nodiscard]] T *get() const noexcept { return _ptr; }
};

namespace detail {

template <typename T> struct resource_type {
    static inline ecs_resource_t id;
};

template <typename T> struct is_res : std::false_type {};
template <typename T> struct is_res<ecs::res<T>> : std::true_type {};

template <typename T> inline constexpr bool is_res_v = is_res<std::remove_cvref_t<T>>::value;

template <typename T> struct res_value;
template <typename T> struct res_value<ecs::res<T>> {
    using type = T;
};

template <typename T> using res_value_t = typename res_value<std::remove_cvref_t<T>>::type;

template <typename T> using resource_value_t = std::remove_cv_t<res_value_t<T>>;

struct no_resource {};

template <typename T> struct resource_hook_state {
    static inline resource_hooks<T> hooks{};
};

template <typename T> static void resource_on_set(const void *ptr) {
    auto callback = resource_hook_state<T>::hooks.on_set;
    if (callback != nullptr) callback(*static_cast<const T *>(ptr));
}

template <typename T> static void resource_on_remove(const void *ptr) {
    auto callback = resource_hook_state<T>::hooks.on_remove;
    if (callback != nullptr) callback(*static_cast<const T *>(ptr));
}

} // namespace detail

/** Register `T` and return a typed resource handle, installing hooks once. */
template <typename T>
static ecs_resource_t ecs_cpp_resource_id(const resource_hooks<std::remove_cv_t<T>> *hooks = nullptr) {
    using type = std::remove_cv_t<T>;
    ecs_resource_t &rid = detail::resource_type<type>::id;

    if (rid != 0)
        return rid;

#if SIECS_HAS_NAMES
    static const std::string name = std::string(type_name<type>());
#endif

    if (hooks != nullptr) detail::resource_hook_state<type>::hooks = *hooks;

    ecs_resource_desc_t desc = {
        SIECS_NAME_INIT(name.c_str())
        .size = sizeof(type),
        .ops = detail::value_ops<type>(),
        .on_set = hooks && hooks->on_set ? detail::resource_on_set<type> : nullptr,
        .on_remove = hooks && hooks->on_remove ? detail::resource_on_remove<type> : nullptr,
    };

    rid = ecs_resource_init(&desc);
    return rid;
}

/** Create a typed resource handle and lazily register its descriptor. */
template <typename T> resource_ref<T> resource_handle() {
    return resource_ref<T>(ecs_cpp_resource_id<T>());
}

/** Create a typed resource handle with lifecycle hooks. */
template <typename T> resource_ref<T> resource_handle(const resource_hooks<std::remove_cv_t<T>> &hooks) {
    return resource_ref<T>(ecs_cpp_resource_id<T>(&hooks));
}

template <typename T> static ecs_resource_t ecs_cpp_try_resource_id() {
    using type = std::remove_cv_t<T>;
    return detail::resource_type<type>::id;
}

namespace detail {

template <typename Arg> inline auto make_resource_arg() {
    if constexpr (is_res_v<Arg>) {
        using value_type = res_value_t<Arg>;
        using resource_type = std::remove_cv_t<value_type>;

        ecs_resource_t id = ecs_cpp_resource_id<resource_type>();
        return ecs::res<value_type>(static_cast<value_type *>(ecs_resource_rid(id)));
    } else {
        return no_resource{};
    }
}

template <typename Args, std::size_t... Is> inline auto make_resources(std::index_sequence<Is...>) {
    return std::tuple{ make_resource_arg<std::tuple_element_t<Is, Args>>()... };
}

template <typename Args> inline auto make_resources() {
    constexpr std::size_t N = std::tuple_size_v<Args>;
    return make_resources<Args>(std::make_index_sequence<N>{});
}

} // namespace detail

} // namespace ecs
