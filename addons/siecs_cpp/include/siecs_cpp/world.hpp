#pragma once
#include "component.hpp"
#include "entity.hpp"
#include "module.hpp"
#include "observer.hpp"
#include "query.hpp"
#include "resource.hpp"
#include "siecs.h"
#include "system.hpp"
#include "type.hpp"
#include <cstdint>
#include <string>
#include <utility>

namespace ecs {

enum class world_ownership : uint8_t {
    owned,
    borrowed,
};

class world {
    ecs_world_t *_world = nullptr;
    world_ownership _ownership = world_ownership::owned;

    template <typename T> static void import_module_callback(ecs_world_t *raw, const void *ptr) {
        ecs::world world = ecs::world::borrow(raw);
        T &module = *static_cast<T *>(const_cast<void *>(ptr));
        module.import(world);
    }

    template <typename T> [[nodiscard]] module_ref<T> import_module(T &module) const {
        static const std::string name = std::string(type_name<T>());

        ecs_module_desc_t desc = {
            .name = name.c_str(),
            .id = &detail::module_type<T>::id,
            .import = import_module_callback<T>,
            .desc = &module,
            .desc_size = sizeof(T),
            .disabled = false,
        };

        return module_ref<T>(_world, ecs_module_init(_world, &desc));
    }

  public:
    world() noexcept : _world(ecs_init()), _ownership(world_ownership::owned) {
        detail::ecs_cpp_set_component_id<Disabled>(ecs_id(Disabled));
        detail::ecs_cpp_set_component_id<ChildOf>(ecs_id(ChildOf));
        detail::ecs_cpp_set_event_id<OnAdd>(EcsOnAdd);
        detail::ecs_cpp_set_event_id<OnAdd>(EcsOnSet);
        detail::ecs_cpp_set_event_id<OnAdd>(EcsOnRemove);
    }
    explicit world(ecs_world_t *world) noexcept
        : _world(world), _ownership(world_ownership::owned) {}
    world(ecs_world_t *world, world_ownership ownership) noexcept
        : _world(world), _ownership(ownership) {}

    [[nodiscard]] static world borrow(ecs_world_t *world) noexcept {
        return ecs::world(world, world_ownership::borrowed);
    }

    world(const world &) = delete;
    world &operator=(const world &) = delete;

    world(world &&other) noexcept
        : _world(std::exchange(other._world, nullptr)),
          _ownership(std::exchange(other._ownership, world_ownership::owned)) {}

    world &operator=(world &&other) noexcept {
        if (this != &other) {
            reset();
            _world = std::exchange(other._world, nullptr);
            _ownership = std::exchange(other._ownership, world_ownership::owned);
        }

        return *this;
    }

    ~world() noexcept { reset(); }

    [[nodiscard]] ecs_world_t *c_ptr() const noexcept { return _world; }

    void reset(
        ecs_world_t *world = nullptr,
        world_ownership ownership = world_ownership::owned
    ) noexcept {
        if (_world != nullptr && _ownership == world_ownership::owned) {
            ecs_fini(_world);
        }

        _world = world;
        _ownership = ownership;
    }

    template <typename T> ecs_component_t component() const {
        return detail::ecs_cpp_component_id<T>(_world);
    }

    template <typename T> void set_resource(T &&value) const {
        using type = std::remove_cvref_t<T>;
        ecs_set_resource_rid(_world, ecs_cpp_resource_id<type>(_world), &value);
    }

    template <typename T> [[nodiscard]] T &resource() const {
        using type = std::remove_cv_t<T>;
        return *static_cast<T *>(ecs_resource_rid(_world, ecs_cpp_resource_id<type>(_world)));
    }

    template <typename T> [[nodiscard]] T *try_resource() const {
        using type = std::remove_cv_t<T>;
        ecs_resource_t id = ecs_cpp_try_resource_id<type>(_world);
        return id ? static_cast<T *>(ecs_try_resource_rid(_world, id)) : nullptr;
    }

    template <typename T> [[nodiscard]] bool has_resource() const {
        using type = std::remove_cv_t<T>;
        ecs_resource_t id = ecs_cpp_try_resource_id<type>(_world);
        return id && ecs_has_resource_rid(_world, id);
    }

    template <typename T> void remove_resource() const {
        using type = std::remove_cv_t<T>;
        ecs_resource_t id = ecs_cpp_try_resource_id<type>(_world);
        if (id) {
            ecs_remove_resource_rid(_world, id);
        }
    }

    template <typename T>
        requires detail::module_importable<T>
    module_ref<T> import(T module) const {
        return import_module<T>(module);
    }

    template <typename T, typename... Args>
        requires detail::module_importable<T> && detail::module_list_initializable<T, Args...>
    module_ref<T> import(Args &&...args) const {
        T module{ std::forward<Args>(args)... };
        return import_module<T>(module);
    }

    template <typename T> [[nodiscard]] module_ref<T> module() const noexcept {
        return module_ref<T>(_world, ecs_module_find(_world, &detail::module_type<T>::id));
    }

    template <typename T> observer<T> observe() const { return observer<T>(_world); }

    template <typename T> [[nodiscard]] ecs_event_t event() const {
        return detail::ecs_cpp_event_id<T>(_world);
    }

    template <typename T> void trigger(ecs::entity entity, const void *data = nullptr) const {
        ecs_observer_trigger(_world, entity.id(), event<T>(), data);
    }

    [[nodiscard]] ecs::entity entity() const { return ecs::entity(_world, ecs_new(_world)); }
    [[nodiscard]] ecs::entity instantiate(ecs::entity e) { return this->entity().is_a(e); }
    [[nodiscard]] ecs::query query() const { return ecs::query(_world); }
    [[nodiscard]] ecs::system system(const char *name) const { return ecs::system(_world, name); }

    void progress() { ecs_progress(_world); }
};

} // namespace ecs
