#pragma once
#include "component.hpp"
#include "siecs.h"

namespace ecs {

class entity {
    ecs_entity_t _entity;
    ecs_world_t *_world;

  public:
    entity(ecs_world_t *world, ecs_entity_t entity) : _entity(entity), _world(world) {}

    template <typename T> entity add() {
        ecs_add_cid(_world, _entity, ecs_cpp_component_id<T>(_world));
        return *this;
    }

    template <typename T> entity set(const T &value) {
        ecs_set_cid(_world, _entity, ecs_cpp_component_id<T>(_world), &value);
        return *this;
    }
};

} // namespace ecs
