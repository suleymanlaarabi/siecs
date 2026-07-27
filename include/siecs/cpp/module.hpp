#pragma once

#include "siecs/cpp/c_api.hpp"
#include <cassert>
#include <concepts>

namespace ecs {

template <typename T> class module_ref {
    ecs_module_id_t _id = 0;

  public:
    constexpr module_ref() noexcept = default;
    constexpr explicit module_ref(ecs_module_id_t id) noexcept : _id(id) {}

    [[nodiscard]] constexpr ecs_module_id_t id() const noexcept { return _id; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _id != 0; }

    void enable() const noexcept {
        assert(_id != 0);
        ecs_module_enable(_id);
    }

    void disable() const noexcept {
        assert(_id != 0);
        ecs_module_disable(_id);
    }

    [[nodiscard]] bool is_enabled() const noexcept {
        assert(_id != 0);
        return ecs_module_is_enabled(_id);
    }
};

namespace detail {

template <typename T> struct module_type {
    static inline ecs_module_id_t id;
};

template <typename T>
concept module_importable = requires(T module) {
    { module.import() } -> std::same_as<void>;
};

template <typename T, typename... Args>
concept module_list_initializable =
    requires(Args &&...args) { T{ static_cast<Args &&>(args)... }; };

} // namespace detail

} // namespace ecs
