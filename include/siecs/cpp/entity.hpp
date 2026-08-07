#pragma once
#include "siecs/cpp/component.hpp"
#include <cstring>
#include <string>

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

/** Small non-owning handle to an entity in the active world. */
class entity {
    ecs_entity_t _entity = 0;

    template <typename> static ecs_entity_t &by_type() {
        static ecs_entity_t id = 0;
        return id;
    }

  public:
    /** Construct the null entity handle. */
    entity() noexcept = default;

    /** Wrap a raw id without validating it; use `is_alive()` to validate. */
    entity(ecs_entity_t entity) noexcept : _entity(entity) {}

    /** Look up a named live entity; returns null when names are disabled/missing. */
    static inline entity lookup(const std::string &name) {
        return entity::from(ecs_lookup(name.c_str()));
    }

    /** Create a new live entity in the active world. */
    static entity create() noexcept { return entity(ecs_new()); }

    /** Create a new live entity without reusing a freed entity index. */
    static entity create_no_reuse() noexcept { return entity(ecs_new_no_reuse()); }

    /** Look up or create a named entity; name storage is copied by the world. */
    static entity create(const char *name) {
        entity value = lookup(name);

        if (value) {
            return value;
        }

        value = create();

        if (name != nullptr) {
            char *copy = strdup(name);
            value.set<Name>({ .value = copy });
        }
        return value;
    }

    /** Wrap an id without changing world state. */
    static entity from(ecs_entity_t id) noexcept { return entity(id); }
    /** Return the null entity handle. */
    static entity null() noexcept { return from(static_cast<ecs_entity_t>(0)); }
    /** Return the raw entity id. */
    [[nodiscard]] ecs_entity_t id() const noexcept { return _entity; }
    /** Explicit conversion used when calling low-level C APIs. */
    operator ecs_entity_t() const noexcept { return _entity; }

    /** Test whether this handle is non-null (not whether it is alive). */
    operator bool() { return _entity != 0; }

    /** Add one or more registered components; returns this handle for chaining. */
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

    /** Mark this entity abstract; application mutation of abstract bases is restricted. */
    entity abstract() {
        ecs_add(_entity, Abstract);
        return *this;
    }

    /** Remove one or more components; missing components are ignored. */
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

    /** Test that all requested components are present on this entity. */
    template <typename... T>
        requires(sizeof...(T) > 0)
    [[nodiscard]] bool has() const {
        return (ecs_has_cid(_entity, detail::ecs_cpp_component_id<T>()) && ...);
    }

    /** Copy a component value into this entity, adding the component if absent. */
    template <typename T> entity set(const T &value) {
        ecs_set_cid(_entity, detail::ecs_cpp_component_id<T>(), &value);
        return *this;
    }

    /** Move a component value into this entity, consuming the source value. */
    template <typename T> entity set(T &&value) {
        using type = std::remove_cvref_t<T>;
        ecs_move_cid(_entity, detail::ecs_cpp_component_id<type>(), &value);
        return *this;
    }

    /** Set multiple component values inside one deferred mutation scope. */
    template <typename First, typename Second, typename... Rest>
    entity set(First &&first, Second &&second, Rest &&...rest) {
        detail::defer_scope scope;
        set(std::forward<First>(first));
        set(std::forward<Second>(second));
        (set(std::forward<Rest>(rest)), ...);
        return *this;
    }

    /** Return mutable component storage, or null when absent. */
    template <typename T> [[nodiscard]] T *try_get() {
        return static_cast<T *>(ecs_try_get_cid(_entity, detail::ecs_cpp_component_id<T>()));
    }

    /** Return mutable component storage; the component must be present. */
    template <typename T> [[nodiscard]] T &get() {
        return *static_cast<T *>(ecs_get_cid(_entity, detail::ecs_cpp_component_id<T>()));
    }

    /** Return const component storage, or null when absent. */
    template <typename T> [[nodiscard]] const T *try_get() const {
        return static_cast<const T *>(ecs_try_get_cid(_entity, detail::ecs_cpp_component_id<T>()));
    }

    /** Return const component storage; the component must be present. */
    template <typename T> [[nodiscard]] const T &get() const {
        return *static_cast<const T *>(ecs_get_cid(_entity, detail::ecs_cpp_component_id<T>()));
    }

    /** Return whether the id is live in the active world. */
    [[nodiscard]] bool is_alive() const { return _entity != 0 && ecs_is_alive(_entity); }
    /** Kill this entity; subsequent component access is invalid. */
    void kill() { ecs_kill(_entity); }

    /** Add an inheritance link to `target`. */
    entity is_a(entity target) {
        ecs_is_a(_entity, target.id());
        return *this;
    }

    /** Add an inheritance link to the singleton entity for `T`. */
    template <typename T> entity is_a() {
        ecs_is_a(_entity, ecs::entity::create<T>());
        return *this;
    }

    /** C-compatible overload of `is_a`; target must be a live entity. */
    entity is_a(ecs_entity_t target) {
        ecs_is_a(_entity, target);
        return *this;
    }

    /** Test whether this entity is or inherits from the singleton `T`. */
    template <typename T> bool is() { return ecs_is(_entity, ecs::entity::typed<T>()); }

    /** Test whether this entity is or inherits from `target`. */
    [[nodiscard]] bool is(entity target) const { return ecs_is(_entity, target._entity); }

    /** Set the builtin `ChildOf` relation to `parent`. */
    entity child_of(entity parent) {
        ecs_relate_id(_entity, ecs_rid(ChildOf), parent.id());
        return *this;
    }

    template <typename Relation> entity relate(entity target) {
        ecs_relate_id(_entity, detail::ecs_cpp_relation_id<Relation>(), target.id());
        return *this;
    }

    template <typename Relation> entity unrelate() {
        ecs_unrelate_id(_entity, detail::ecs_cpp_relation_id<Relation>());
        return *this;
    }

    template <typename Relation> [[nodiscard]] bool has_relation() const {
        return ecs_has_relation_id(_entity, detail::ecs_cpp_relation_id<Relation>());
    }

    template <typename Relation> [[nodiscard]] entity target() const {
        return entity(ecs_target_id(_entity, detail::ecs_cpp_relation_id<Relation>()));
    }

    /** Remove `Disabled`, allowing the entity in default queries. */
    entity enable() {
        ecs_remove(_entity, Disabled);
        return *this;
    }

    /** Add `Disabled`, excluding the entity from default queries. */
    entity disable() {
        ecs_add(_entity, Disabled);
        return *this;
    }

    /** Return whether the entity is not marked `Disabled`. */
    [[nodiscard]] bool is_enabled() const { return !has<Disabled>(); }
    /** Return whether the entity is marked `Disabled`. */
    [[nodiscard]] bool is_disabled() const { return has<Disabled>(); }

    /** Return/create the singleton entity associated with type `T`. */
    template <typename T> static entity create(const char *name = nullptr) {
        ecs_entity_t &id = by_type<T>();
        if (id == 0 || !ecs_is_alive(id)) {
            const std::string generated = std::string(type_name<T>());
            id = create(name ? name : generated.c_str()).id();
        }
        return from(id);
    }

    /** Return the currently cached singleton entity for `T`, or null. */
    template <typename T> static entity typed() { return from(by_type<T>()); }
};

} // namespace ecs
