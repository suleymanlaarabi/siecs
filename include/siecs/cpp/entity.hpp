#pragma once
#include "siecs/cpp/component.hpp"
#include "siecs/cpp/system_param.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace ecs {

struct IsA;
struct ChildOf;
class entity;
class observer_ref;
entity spawn() noexcept;
entity spawn(entity parent);
entity ensure(const char *path);
entity ensure(entity parent, const char *path);
template <typename T> entity singleton();

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
    template <typename T> friend entity singleton();

    template <typename> static ecs_entity_t &by_type() {
        static ecs_entity_t id = 0;
        return id;
    }

    template <bool Add, typename... T> entity mutate() {
        if constexpr (sizeof...(T) > 1) {
            detail::defer_scope scope;
            ((Add ? ecs_add_cid(_entity, detail::ecs_cpp_component_id<T>())
                  : ecs_remove_cid(_entity, detail::ecs_cpp_component_id<T>())),
             ...);
        } else {
            ((Add ? ecs_add_cid(_entity, detail::ecs_cpp_component_id<T>())
                  : ecs_remove_cid(_entity, detail::ecs_cpp_component_id<T>())),
             ...);
        }
        return *this;
    }

  public:
    /** Construct the null entity handle. */
    entity() noexcept = default;
    static entity init() noexcept { return {}; }
    void row(ecs_iter_t *it, uint32_t row) noexcept { _entity = it->entities[row]; }

    /** Wrap a raw id without validating it; use `is_alive()` to validate. */
    entity(ecs_entity_t entity) noexcept : _entity(entity) {}

    /** Look up a :: path from the root; returns null when absent. */
    static entity lookup(const char *path) { return entity::from(ecs_lookup(path)); }

    /** Look up an existing string without making a temporary string copy. */
    static inline entity lookup(const std::string &name) { return lookup(name.c_str()); }

    /** Resolve a path relative to parent, without searching its ancestors. */
    static entity lookup(entity parent, const char *path) {
        return from(ecs_lookup_from(parent.id(), path));
    }

    /** Resolve an existing string relative to parent without copying it. */
    static entity lookup(entity parent, const std::string &path) {
        return lookup(parent, path.c_str());
    }

    /** Rename locally; a collision deletes the previous entity and its children. */
    bool set_name(const char *name) { return ecs_set_name(_entity, name); }

    /** Return the local name, or the generated diagnostic name when anonymous. */
    [[nodiscard]] const char *name() const { return ecs_entity_name(_entity); }

    /** Compute the absolute path; no implicit path cache is maintained. */
    [[nodiscard]] std::string path() const {
        std::unique_ptr<char, decltype(&std::free)> value(ecs_entity_path(_entity), &std::free);
        return value ? std::string(value.get()) : std::string();
    }

    /** Create an IsA instance and recursively snapshot the prefab's owned ChildOf subtree. */
    static entity instantiate(entity prefab) { return spawn().is_a(prefab); }

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
        return mutate<true, T...>();
    }

    /** Mark this entity abstract; application mutation of abstract bases is restricted. */
    entity abstract() {
        ecs_abstract(_entity);
        return *this;
    }

    /** Remove one or more components; missing components are ignored. */
    template <typename... T>
        requires(sizeof...(T) > 0)
    entity remove() {
        return mutate<false, T...>();
    }

    /** Test that all requested components are present on this entity. */
    template <typename... T>
        requires(sizeof...(T) > 0)
    [[nodiscard]] bool has() const {
        return (ecs_has_cid(_entity, detail::ecs_cpp_component_id<T>()) && ...);
    }

    /** Copy an lvalue or move an rvalue into this entity, adding it if absent. */
    template <typename T> entity set(T &&value) {
        using type = std::remove_cvref_t<T>;
        if constexpr (std::is_lvalue_reference_v<T>)
            ecs_set_cid(_entity, detail::ecs_cpp_component_id<type>(), &value);
        else
            ecs_move_cid(_entity, detail::ecs_cpp_component_id<type>(), &value);
        return *this;
    }

    /** Set multiple component values inside one deferred mutation scope. */
    template <typename First, typename Second, typename... Rest>
    entity set(First &&first, Second &&second, Rest &&...rest) {
        /* Plain values reach their final table in a single move when nothing
         * observes the change; otherwise the defer scope batches them. */
        if constexpr (std::is_trivially_copyable_v<std::remove_cvref_t<First>> &&
                      std::is_trivially_copyable_v<std::remove_cvref_t<Second>> &&
                      (std::is_trivially_copyable_v<std::remove_cvref_t<Rest>> && ...)) {
            const ecs_component_t ids[] = {
                detail::ecs_cpp_component_id<std::remove_cvref_t<First>>(),
                detail::ecs_cpp_component_id<std::remove_cvref_t<Second>>(),
                detail::ecs_cpp_component_id<std::remove_cvref_t<Rest>>()...,
            };
            const void *const values[] = { &first, &second, &rest... };
            if (ecs_set_plain_many(_entity, 2 + sizeof...(Rest), ids, values))
                return *this;
        }
        detail::defer_scope scope;
        set(std::forward<First>(first));
        set(std::forward<Second>(second));
        (set(std::forward<Rest>(rest)), ...);
        return *this;
    }

    /** Return mutable component storage, or null when absent. */
    template <typename T> [[nodiscard]] T *try_edit() {
        ecs_component_t id = detail::ecs_cpp_component_id<T>();
#ifndef NDEBUG
        assert(
            ecs_component_info(id)->mutation != EcsSetOnly &&
            "SetOnly component can only be modified with set()"
        );
#endif
        return static_cast<T *>(ecs_try_get_cid(_entity, id));
    }

    /** Return mutable component storage; the component must be present. */
    template <typename T> [[nodiscard]] T &edit() {
        ecs_component_t id = detail::ecs_cpp_component_id<T>();
#ifndef NDEBUG
        assert(
            ecs_component_info(id)->mutation != EcsSetOnly &&
            "SetOnly component can only be modified with set()"
        );
#endif
        return *static_cast<T *>(ecs_get_cid(_entity, id));
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

    /** Observe events of type `Event` using the provided callback. */
    template <typename Event, typename F> observer_ref observe(F &&callback);

    /** Add child entities to this entity. */
    template <typename... Entities> entity children(Entities... entities) {
        (entities.child_of(*this), ...);
        return *this;
    }

    /** Add or retarget IsA; a new target recursively snapshots its owned ChildOf subtree. */
    entity is_a(entity target);

    /** Add IsA to the singleton for `T` with the same ChildOf snapshot semantics. */
    template <typename T> entity is_a() { return is_a(ecs::singleton<T>()); }

    /** C-compatible IsA overload with the same ChildOf snapshot semantics. */
    entity is_a(ecs_entity_t target) { return is_a(entity::from(target)); }

    /** Test whether this entity is or inherits from the singleton `T`. */
    template <typename T> bool is() { return ecs_is(_entity, by_type<T>()); }

    /** Test whether this entity is or inherits from `target`. */
    [[nodiscard]] bool is(entity target) const { return ecs_is(_entity, target._entity); }

    /** Set the builtin `ChildOf` relation to `parent`. */
    entity child_of(entity parent);

    template <typename Relation> entity relate(entity target) {
        ecs_relate_id(_entity, detail::ecs_cpp_relation_id<Relation>(), target.id());
        return *this;
    }

    template <typename Relation, typename Target> entity relate() {
        relate<Relation>(ecs::singleton<Target>());
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
};

/** Create a fresh anonymous entity; use ensure() for named paths. */
inline entity spawn() noexcept { return entity::from(ecs_new()); }
inline entity spawn(entity parent) { return spawn().child_of(parent); }
/** Create a fresh entity without reusing a freed index. */
inline entity spawn_no_reuse() noexcept { return entity::from(ecs_new_no_reuse()); }
/** Find or create a path, including missing parents. */
inline entity ensure(const char *path) { return entity::from(ecs_new_named(0, path)); }
inline entity ensure(entity parent, const char *path) {
    return entity::from(ecs_new_named(parent.id(), path));
}
/** Return/create the singleton for T, resetting its cache at world shutdown. */
template <typename T> entity singleton() {
    auto &id = entity::by_type<T>();
    if (id == 0 || !ecs_is_alive(id)) {
        const std::string name(type_name<T>());
        id = ensure(name.c_str()).id();
        ecs_fini_desc_t reset{
            .callback = [](void *value) { *static_cast<ecs_entity_t *>(value) = 0; },
            .data = &id,
        };
        ecs_at_fini_init(&reset);
    }
    return entity::from(id);
}

} // namespace ecs

#include "siecs/cpp/relation.hpp"
