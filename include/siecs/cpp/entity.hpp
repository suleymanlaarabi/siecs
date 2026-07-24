#pragma once
#include "siecs/cpp/component.hpp"

namespace ecs {

class entity {
    ecs_entity_t _entity;
    ecs_world_t *_world;

  public:
    static ecs::entity null() { return entity(nullptr, 0); }

    entity(ecs_world_t *world, ecs_entity_t entity) : _entity(entity), _world(world) {}

    [[nodiscard]] ecs_entity_t id() const noexcept { return _entity; }

    template <typename T> entity add() {
        ecs_add_cid(_world, _entity, detail::ecs_cpp_component_id<T>(_world));
        return *this;
    }

    operator ecs_entity_t() const noexcept { return _entity; }

    entity abstract() {
        ecs_add(_world, _entity, Abstract);
        return *this;
    }

    template <typename T> entity remove() {
        ecs_remove_cid(_world, _entity, detail::ecs_cpp_component_id<T>(_world));
        return *this;
    }

    template <typename T> [[nodiscard]] bool has() const {
        return ecs_has_cid(_world, _entity, detail::ecs_cpp_component_id<T>(_world));
    }

    template <typename T> entity set(const T &value) {
        ecs_set_cid(_world, _entity, detail::ecs_cpp_component_id<T>(_world), &value);
        return *this;
    }

    template <typename T>
    entity set(T &&value)
        requires(!std::is_lvalue_reference_v<T>)
    {
        using type = std::remove_cvref_t<T>;
        ecs_move_cid(_world, _entity, detail::ecs_cpp_component_id<type>(_world), &value);
        return *this;
    }

    bool is_alive() { return ecs_is_alive(_world, _entity); }

    void kill() { ecs_kill(_world, _entity); }

    entity is_a(entity target) {
        ecs_is_a(_world, _entity, target._entity);
        return *this;
    }

    bool is(entity target) { return ecs_is(_world, _entity, target._entity); }

    entity child_of(entity parent) {
        const ChildOf desc = { .target = parent._entity };
        ecs_set_cid(_world, _entity, ecs_id(ChildOf), &desc);
        return *this;
    }

    entity enable() {
        ecs_remove_cid(_world, _entity, ecs_id(Disabled));
        return *this;
    }

    entity disable() {
        ecs_add_cid(_world, _entity, ecs_id(Disabled));
        return *this;
    }

    [[nodiscard]] bool is_enabled() const {
        return !ecs_has_cid(_world, _entity, ecs_id(Disabled));
    }

    [[nodiscard]] bool is_disabled() const {
        return ecs_has_cid(_world, _entity, ecs_id(Disabled));
    }
};

} // namespace ecs
