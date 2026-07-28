#pragma once
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

inline void init_cpp_state() {
    detail::component_type<Disabled>::id = ecs_id(Disabled);
    detail::component_type<Name>::id = ecs_id(Name);
    detail::component_type<ChildOf>::id = ecs_id(ChildOf);
    detail::component_type<Abstract>::id = ecs_id(Abstract);
}
inline void init() {
    ecs_init();
    init_cpp_state();
}
inline void init(const ecs_world_feat_desc_t &features) {
    ecs_init_w_features(&features);
    init_cpp_state();
}
inline void fini() { ecs_fini(); }
inline void quit() { ecs_quit(); }
inline bool progress() { return ecs_progress(); }
inline void run() { ecs_run(); }
inline void run_phase(ecs_phase_t phase) { ecs_run_phase(phase); }

template <typename T> inline ecs_component_t component() {
    return detail::ecs_cpp_component_id<T>();
}

template <typename T> inline ecs_component_t component(const component_hooks<T> &hooks) {
    return detail::ecs_cpp_component_id<T>(0, &hooks);
}

template <typename Component, typename Required> inline void component_requires() {
    ecs_with(component<Component>(), component<Required>());
}

template <typename T> inline ecs_component_t relation(ecs_relation_flags_t flags = {}) {
    return detail::ecs_cpp_component_id<T>(flags);
}

template <typename T> inline ecs_component_t relation_source() {
    return relation<T>() + 1;
}

template <typename T> inline void set_resource(T &&value) {
    using type = std::remove_cvref_t<T>;
    if constexpr (std::is_lvalue_reference_v<T>) {
        ecs_set_resource_rid(ecs_cpp_resource_id<type>(), &value);
    } else {
        ecs_move_resource_rid(ecs_cpp_resource_id<type>(), &value);
    }
}

template <typename T> [[nodiscard]] inline T &resource() {
    using type = std::remove_cv_t<T>;
    return *static_cast<T *>(ecs_resource_rid(ecs_cpp_resource_id<type>()));
}

template <typename T> [[nodiscard]] inline ecs_resource_t resource_id() {
    return ecs_cpp_resource_id<std::remove_cv_t<T>>();
}

template <typename T> [[nodiscard]] inline T *try_resource() {
    using type = std::remove_cv_t<T>;
    ecs_resource_t id = ecs_cpp_try_resource_id<type>();
    return id ? static_cast<T *>(ecs_try_resource_rid(id)) : nullptr;
}

template <typename T> [[nodiscard]] inline bool has_resource() {
    using type = std::remove_cv_t<T>;
    ecs_resource_t id = ecs_cpp_try_resource_id<type>();
    return id && ecs_has_resource_rid(id);
}

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

template <typename T> [[nodiscard]] module_ref<T> import(T module) {
    if (detail::module_type<T>::id != 0) {
        return module_ref<T>(detail::module_type<T>::id);
    }
    static const std::string name = std::string(type_name<T>());
    ecs_module_desc_t desc = {
        .name = name.c_str(),
        .id = &detail::module_type<T>::id,
        .import = import_module_callback<T>,
        .desc = &module,
        .desc_size = sizeof(T),
        .disabled = false,
    };
    return module_ref<T>(ecs_module_init(&desc));
}

template <typename T, typename... Args>
    requires detail::module_importable<T> && detail::module_list_initializable<T, Args...>
[[nodiscard]] module_ref<T> import(Args &&...args) {
    return import(T{ std::forward<Args>(args)... });
}

template <typename T> [[nodiscard]] module_ref<T> module() noexcept {
    return module_ref<T>(detail::module_type<T>::id);
}

template <typename T> [[nodiscard]] observer<T> observe() { return observer<T>(); }

template <typename T> [[nodiscard]] ecs_event_t event() { return detail::ecs_cpp_event_id<T>(); }

template <typename T> inline void trigger(entity value, const void *data = nullptr) {
    ecs_observer_trigger(value.id(), event<T>(), data);
}

inline entity instantiate(entity base) { return entity::create().is_a(base); }

} // namespace ecs
