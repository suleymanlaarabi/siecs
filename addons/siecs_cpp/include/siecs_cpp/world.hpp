#pragma once
#include "component.hpp"
#include "entity.hpp"
#include "siecs.h"

namespace ecs {

class world {
    ecs_world_t *_world;

  public:
    world() : _world(ecs_init()) {}

    template <typename T> ecs_component_t component() {
        return ecs_cpp_component_id<T>(_world);
    }

    ecs::entity entity() { return ecs::entity(_world, ecs_new(_world)); }
};

} // namespace ecs
