#pragma once
#include "siecs.h"
#include "siecs/cpp/query.hpp"

namespace ecs {

namespace detail {

template <typename Callback, typename Args> static void system_callback(ecs_iter_t *it) {
    Callback &callback = *reinterpret_cast<Callback *>(it->user_data);
    auto params = init_callback_params<Args>();
    if (it->cache) {
        while (ecs_iter_next(it))
            run_batch<Args>(callback, it, params);
    } else {
        run_batch<Args>(callback, it, params);
    }
}

template <typename Callback> static void system_callback_dtor(uintptr_t user_data) {
    delete reinterpret_cast<Callback *>(user_data);
}

} // namespace detail

/** Non-owning handle to a system registered in the active world. */
class system_ref {
    ecs_system_id_t _id;

  public:
    explicit system_ref(ecs_system_id_t id) noexcept : _id(id) {}
    void enable() const { ecs_system_enable(_id); }
    void disable() const { ecs_system_disable(_id); }
    void run() const { ecs_run_system(_id); }
    [[nodiscard]] ecs_system_id_t id() const noexcept { return _id; }
};

/** Fluent system builder; the world owns its callback and registration. */
class system : public query_filter<system> {
    ecs_system_desc_t _system{ .phase = EcsOnUpdate };

  public:
    /** Construct a system descriptor with an optional diagnostic name. */
    explicit system(const char *name = "unnamed") { _system.name = name; }

    /** Select the phase in which the system is scheduled. */
    system &phase(ecs_phase_t _phase) {
        _system.phase = _phase;
        return *this;
    }

    /** Set the automatic execution interval in seconds. Zero runs every frame. */
    system &interval(double seconds) {
        _system.interval = seconds;
        return *this;
    }

    /** Opt in to concurrent portions of an unordered entity query.
     * Zero selects 1024 rows per portion. Captures and external state must
     * support concurrent invocation; writable resources are rejected. */
    system &parallel(uint32_t chunk_size = 0) {
        _system.parallel = true;
        _system.chunk_size = chunk_size;
        return *this;
    }

    /** Set whether the system is only run on the main thread. */
    system &main_thread() {
        _system.main_thread_only = true;
        return *this;
    }

    /** Run a resource-only system without an outer deferred mutation scope. */
    system &no_defer() {
        _system.no_defer = true;
        return *this;
    }

    /** Add a same-phase dependency; capacity is `ECS_SYSTEM_AFTER_CAPACITY`. */
    system &after(system_ref dependency) {
        for (uint16_t i = 0; i < ECS_SYSTEM_AFTER_CAPACITY; i++) {
            if (_system.after[i] == 0) {
                _system.after[i] = dependency.id();
                return *this;
            }
        }
        assert(false && "too many system dependencies");
        return *this;
    }

    /** Set whether the system starts disabled when registered. */
    system &disabled(bool value = true) {
        _system.disabled = value;
        return *this;
    }

    /** Register a world-owned callback and return its non-owning handle. */
    template <typename F> system_ref each(F &&func) {
        using callback = std::remove_cvref_t<F>;
        using args = typename function_traits<callback>::args_tuple;
        detail::append_callback_terms<args>(desc, component_index, resource_index, relation_index);
        callback *state = new callback(std::forward<F>(func));

        _system.query = this->desc;
        _system.callback = detail::system_callback<callback, args>;
        _system.callback_iterates_query = true;
        _system.user_data = reinterpret_cast<uintptr_t>(state);
        _system.user_data_dtor = detail::system_callback_dtor<callback>;
        return system_ref(ecs_system_init(&_system));
    }
};

} // namespace ecs
