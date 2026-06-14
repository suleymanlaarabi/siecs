#pragma once
#include "component.hpp"
#include "entity.hpp"
#include "query.hpp"
#include "siecs.h"
#include <utility>

namespace ecs {

class world {
    ecs_world_t *_world = nullptr;

  public:
    world() noexcept : _world(ecs_init()) {}
    explicit world(ecs_world_t *world) noexcept : _world(world) {}

    world(const world &) = delete;
    world &operator=(const world &) = delete;

    world(world &&other) noexcept : _world(std::exchange(other._world, nullptr)) {}

    world &operator=(world &&other) noexcept {
        if (this != &other) {
            reset();
            _world = std::exchange(other._world, nullptr);
        }

        return *this;
    }

    ~world() noexcept { reset(); }

    [[nodiscard]] ecs_world_t *c_ptr() const noexcept { return _world; }

    void reset(ecs_world_t *world = nullptr) noexcept {
        if (_world != nullptr) {
            ecs_fini(_world);
        }

        _world = world;
    }

    template <typename T> [[nodiscard]] ecs_component_t component() const {
        return ecs_cpp_component_id<T>(_world);
    }

    [[nodiscard]] ecs::entity entity() const { return ecs::entity(_world, ecs_new(_world)); }
    [[nodiscard]] ecs::query query() const { return ecs::query(_world); }
};

} // namespace ecs
