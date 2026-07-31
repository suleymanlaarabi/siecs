#pragma once
#include "siecs.h"
#include "siecs/cpp/component.hpp"
#include "siecs/cpp/entity.hpp"
#include "siecs/cpp/module.hpp"
#include "siecs/cpp/observer.hpp"
#include "siecs/cpp/query.hpp"
#include "siecs/cpp/resource.hpp"
#include "siecs/cpp/system.hpp"
#include "siecs/cpp/type.hpp"
#include <cstring>
#include <string>
#include <utility>

namespace ecs {

/** Synchronize builtin component ids after the C world is initialized. */
inline void init_cpp_state() {
    detail::component_type<Disabled>::id = ecs_id(Disabled);
#if SIECS_HAS_NAMES
    detail::component_type<Name>::id = ecs_id(Name);
#endif
    detail::component_type<ChildOf>::id = ecs_id(ChildOf);
    detail::component_type<Abstract>::id = ecs_id(Abstract);
}
/** Initialize the process-wide active ECS world and C++ builtin ids. */
inline void init() {
    ecs_init();
    init_cpp_state();
}
/** Initialize the active world with feature settings; `features` is read now. */
inline void init(const ecs_world_feat_desc_t &features) {
    ecs_init_w_features(&features);
    init_cpp_state();
}
/** Destroy the active world; all entity, query, module and resource handles expire. */
inline void fini() { ecs_fini(); }
/** Request that future progress calls stop. */
inline void quit() { ecs_quit(); }
/** Run one frame; returns false after `quit()` has been requested. */
inline bool progress() { return ecs_progress(); }
/** Run all enabled systems in phase order without frame pacing. */
inline void run() { ecs_run(); }
/** Run enabled systems from the specified phase. */
inline void run_phase(ecs_phase_t phase) { ecs_run_phase(phase); }

/** Register or return the component id associated with `T`. */
template <typename T> inline ecs_component_t component() {
    return detail::ecs_cpp_component_id<T>();
}

/** Register a component and install its lifecycle hooks before first use. */
template <typename T> inline ecs_component_t component(const component_hooks<T> &hooks) {
    return detail::ecs_cpp_component_id<T>(0, &hooks);
}

/** Declare that adding `Component` implicitly adds `Required` first. */
template <typename Component, typename Required> inline void component_requires() {
    ecs_with(component<Component>(), component<Required>());
}

/** Register `T` as a relation with the requested target/source behavior. */
template <typename T> inline ecs_component_t relation(ecs_relation_flags_t flags = {}) {
    return detail::ecs_cpp_component_id<T>(flags);
}

/** Experimental: return reverse/source storage id for relation `T`. */
template <typename T> inline ecs_component_t relation_source() { return relation<T>() + 1; }

/** Copy an lvalue or move an rvalue into the per-world resource of type `T`. */
template <typename T> inline void set_resource(T &&value) {
    using type = std::remove_cvref_t<T>;
    if constexpr (std::is_lvalue_reference_v<T>) {
        ecs_set_resource_rid(ecs_cpp_resource_id<type>(), &value);
    } else {
        ecs_move_resource_rid(ecs_cpp_resource_id<type>(), &value);
    }
}

/** Return a mutable resource reference; a value must have been set first. */
template <typename T> [[nodiscard]] inline T &resource() {
    using type = std::remove_cv_t<T>;
    return *static_cast<T *>(ecs_resource_rid(ecs_cpp_resource_id<type>()));
}

/** Return the registered resource id for `T`, creating registration if needed. */
template <typename T> [[nodiscard]] inline ecs_resource_t resource_id() {
    return ecs_cpp_resource_id<std::remove_cv_t<T>>();
}

/** Return a resource pointer, or null when `T` is not registered/present. */
template <typename T> [[nodiscard]] inline T *try_resource() {
    using type = std::remove_cv_t<T>;
    ecs_resource_t id = ecs_cpp_try_resource_id<type>();
    return id ? static_cast<T *>(ecs_try_resource_rid(id)) : nullptr;
}

/** Test whether a resource of type `T` is present in the active world. */
template <typename T> [[nodiscard]] inline bool has_resource() {
    using type = std::remove_cv_t<T>;
    ecs_resource_t id = ecs_cpp_try_resource_id<type>();
    return id && ecs_has_resource_rid(id);
}

/** Remove resource `T` if present; no-op when it has not been registered. */
template <typename T> inline void remove_resource() {
    using type = std::remove_cv_t<T>;
    ecs_resource_t id = ecs_cpp_try_resource_id<type>();
    if (id) {
        ecs_remove_resource_rid(id);
    }
}

template <typename T> static void import_module_callback(const void *ptr) {
    T &module = *static_cast<T *>(const_cast<void *>(ptr));
    module.import();
}

/** Import a module instance once; subsequent calls return the cached module. */
template <typename T> [[nodiscard]] module_ref<T> import(T module) {
    if (detail::module_type<T>::id != 0) {
        return module_ref<T>(detail::module_type<T>::id);
    }
#if SIECS_HAS_NAMES
    static const std::string name = std::string(type_name<T>());
#endif
    ecs_module_desc_t desc = {
        SIECS_NAME_INIT(name.c_str()).id = &detail::module_type<T>::id,
        .import = import_module_callback<T>,
        .desc = &module,
        .desc_size = sizeof(T),
        .disabled = false,
    };
    return module_ref<T>(ecs_module_init(&desc));
}

/** Construct and import a module from arguments satisfying its import contract. */
template <typename T, typename... Args>
    requires detail::module_importable<T> && detail::module_list_initializable<T, Args...>
[[nodiscard]] module_ref<T> import(Args &&...args) {
    return import(T{ std::forward<Args>(args)... });
}

/** Return the cached module handle for `T`, or an empty handle. */
template <typename T> [[nodiscard]] module_ref<T> module() noexcept {
    return module_ref<T>(detail::module_type<T>::id);
}

/** Build an observer for event tag `T`. */
template <typename T> [[nodiscard]] observer<T> observe() { return observer<T>(); }

/** Return the stable event id associated with event tag `T`. */
template <typename T> [[nodiscard]] ecs_event_t event() { return detail::ecs_cpp_event_id<T>(); }

/** Trigger event `T` for a live entity; callback data is borrowed for the call. */
template <typename T> inline void trigger(entity value, const void *data = nullptr) {
    ecs_observer_trigger(value.id(), event<T>(), data);
}

/** Experimental: create an empty entity with an `IsA` link; this is not a deep clone. */
inline entity instantiate(entity base) { return entity::create().is_a(base); }

} // namespace ecs
