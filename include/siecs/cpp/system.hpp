#pragma once
#include "siecs.h"
#include "siecs/cpp/query.hpp"

namespace ecs {

namespace detail {

template <typename Callback, typename Args> static void system_callback(ecs_iter_t *it) {
    Callback &callback = *reinterpret_cast<Callback *>(it->user_data);
    auto resources = make_resources<Args>();
    if constexpr (component_arg_count<Args>() == 0) {
        std::apply(callback, resources);
    } else {
        run_batch<Callback, Args>(callback, it, resources);
    }
}

template <typename Callback> static void system_callback_dtor(uintptr_t user_data) {
    delete reinterpret_cast<Callback *>(user_data);
}

} // namespace detail

/** Fluent typed system builder; the resulting C system owns its callback. */
class system : protected query {
    ecs_system_desc_t _system{ .phase = EcsOnUpdate };

  public:
    /** Construct a system descriptor with an optional diagnostic name. */
    explicit system(const char *name = "unnamed") { _system.name = name; }

    /** Add required filter terms to the system query. */
    template <typename... T> system &require() {
        query::require<T...>();
        return *this;
    }

    /** Add optional component terms to the system query. */
    template <typename... T> system &optional() {
        query::optional<T...>();
        return *this;
    }

    /** Add exclusion terms to the system query. */
    template <typename... T> system &exclude() {
        query::exclude<T...>();
        return *this;
    }

    /** Select the phase in which the system is scheduled. */
    system &phase(ecs_phase_t _phase) {
        _system.phase = _phase;
        return *this;
    }

    /** Add a same-phase dependency; capacity is `ECS_SYSTEM_AFTER_CAPACITY`. */
    system &after(ecs_system_id_t dependency) {
        for (uint16_t i = 0; i < ECS_SYSTEM_AFTER_CAPACITY; i++) {
            if (_system.after[i] == 0) {
                _system.after[i] = dependency;
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

    /** Register the callback and return the owned system id. */
    template <typename F> ecs_system_id_t each(F &&func) {
        using callback = std::remove_cvref_t<F>;
        using args = typename function_traits<callback>::args_tuple;
        detail::append_callback_terms<args>(desc, term_index);
        callback *state = new callback(std::forward<F>(func));

        _system.query = this->desc;
        _system.callback = detail::system_callback<callback, args>;
        _system.user_data = reinterpret_cast<uintptr_t>(state);
        _system.user_data_dtor = detail::system_callback_dtor<callback>;
        return ecs_system_init(&_system);
    }
};

/** Run one enabled system immediately. */
inline void run_system(ecs_system_id_t id) { ecs_run_system(id); }
/** Enable a registered system. */
inline void enable_system(ecs_system_id_t id) { ecs_system_enable(id); }
/** Disable a registered system. */
inline void disable_system(ecs_system_id_t id) { ecs_system_disable(id); }

} // namespace ecs
