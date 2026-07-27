#pragma once

#include "siecs/cpp/component.hpp"

namespace ecs {

namespace detail {

class defer_scope {
  public:
    defer_scope() { ecs_defer_begin(); }
    ~defer_scope() { ecs_defer_end(); }

    defer_scope(const defer_scope &) = delete;
    defer_scope &operator=(const defer_scope &) = delete;
};

} // namespace detail

class entity {
    ecs_entity_t _entity = 0;

  public:
    entity() noexcept = default;

    explicit entity(ecs_entity_t entity) noexcept : _entity(entity) {}

    static entity create() noexcept { return entity(ecs_new()); }
    static entity create(const char *name) {
        entity value = create();
        if (name != nullptr) {
            value.set<Name>({ .value = strdup(name) });
        }
        return value;
    }

    static entity from(ecs_entity_t id) noexcept { return entity(id); }
    static entity null() noexcept { return from(static_cast<ecs_entity_t>(0)); }
    [[nodiscard]] ecs_entity_t id() const noexcept { return _entity; }
    operator ecs_entity_t() const noexcept { return _entity; }

    template <typename... T>
        requires(sizeof...(T) > 0)
    entity add() {
        if constexpr (sizeof...(T) == 1) {
            (ecs_add_cid(_entity, detail::ecs_cpp_component_id<T>()), ...);
        } else {
            detail::defer_scope scope;
            (ecs_add_cid(_entity, detail::ecs_cpp_component_id<T>()), ...);
        }
        return *this;
    }

    entity abstract() {
        ecs_add(_entity, Abstract);
        return *this;
    }

    template <typename... T>
        requires(sizeof...(T) > 0)
    entity remove() {
        if constexpr (sizeof...(T) == 1) {
            (ecs_remove_cid(_entity, detail::ecs_cpp_component_id<T>()), ...);
        } else {
            detail::defer_scope scope;
            (ecs_remove_cid(_entity, detail::ecs_cpp_component_id<T>()), ...);
        }
        return *this;
    }

    template <typename... T>
        requires(sizeof...(T) > 0)
    [[nodiscard]] bool has() const {
        return (ecs_has_cid(_entity, detail::ecs_cpp_component_id<T>()) && ...);
    }

    template <typename T> entity set(const T &value) {
        ecs_set_cid(_entity, detail::ecs_cpp_component_id<T>(), &value);
        return *this;
    }

    template <typename T> entity set(T &&value) {
        using type = std::remove_cvref_t<T>;
        ecs_move_cid(_entity, detail::ecs_cpp_component_id<type>(), &value);
        return *this;
    }

    template <typename First, typename Second, typename... Rest>
    entity set(First &&first, Second &&second, Rest &&...rest) {
        detail::defer_scope scope;
        set(std::forward<First>(first));
        set(std::forward<Second>(second));
        (set(std::forward<Rest>(rest)), ...);
        return *this;
    }

    template <typename T> [[nodiscard]] T *try_get() {
        return static_cast<T *>(ecs_try_get_cid(_entity, detail::ecs_cpp_component_id<T>()));
    }

    template <typename T> [[nodiscard]] T &get() {
        return *static_cast<T *>(ecs_get_cid(_entity, detail::ecs_cpp_component_id<T>()));
    }

    template <typename T> [[nodiscard]] const T *try_get() const {
        return static_cast<const T *>(ecs_try_get_cid(_entity, detail::ecs_cpp_component_id<T>()));
    }

    template <typename T> [[nodiscard]] const T &get() const {
        return *static_cast<const T *>(ecs_get_cid(_entity, detail::ecs_cpp_component_id<T>()));
    }

    [[nodiscard]] bool is_alive() const { return _entity != 0 && ecs_is_alive(_entity); }
    void kill() { ecs_kill(_entity); }

    entity is_a(entity target) {
        ecs_is_a(_entity, target.id());
        return *this;
    }

    template <typename T> entity is_a() {
        ecs_is_a(_entity, ecs::entity::create<T>());
        return *this;
    }

    entity is_a(ecs_entity_t target) {
        ecs_is_a(_entity, target);
        return *this;
    }

    [[nodiscard]] bool is(entity target) const { return ecs_is(_entity, target._entity); }

    entity child_of(entity parent) {
        ChildOf relation{ parent.id() };
        ecs_set_cid(_entity, ecs_id(ChildOf), &relation);
        return *this;
    }

    entity enable() {
        ecs_remove(_entity, Disabled);
        return *this;
    }

    entity disable() {
        ecs_add(_entity, Disabled);
        return *this;
    }

    [[nodiscard]] bool is_enabled() const { return !has<Disabled>(); }
    [[nodiscard]] bool is_disabled() const { return has<Disabled>(); }

    template <typename T> static entity create(const char *name = nullptr) {
        static ecs_entity_t id = 0;
        if (id == 0 || !ecs_is_alive(id)) {
            const std::string generated = std::string(type_name<T>());
            id = create(name ? name : generated.c_str()).id();
        }
        return from(id);
    }
};

} // namespace ecs
