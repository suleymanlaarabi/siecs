#pragma once
#include "component.hpp"
#include "siecs.h"

namespace ecs {

class entity {
    ecs_entity_t _entity;
    ecs_world_t *_world;

  public:
    static ecs::entity null() { return entity(nullptr, 0); }

    entity(ecs_world_t *world, ecs_entity_t entity) : _entity(entity), _world(world) {}

    [[nodiscard]] ecs_entity_t id() const noexcept { return _entity; }

    template <typename T> entity add() {
        ecs_add_cid(_world, _entity, ecs_cpp_component_id<T>(_world));
        return *this;
    }

    template <typename T> entity remove() {
        ecs_remove_cid(_world, _entity, ecs_cpp_component_id<T>(_world));
        return *this;
    }

    template <typename T> [[nodiscard]] bool has() const {
        return ecs_has_cid(_world, _entity, ecs_cpp_component_id<T>(_world));
    }

    template <typename T> entity set(const T &value) {
        ecs_set_cid(_world, _entity, ecs_cpp_component_id<T>(_world), &value);
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
