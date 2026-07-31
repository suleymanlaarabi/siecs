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
#if SIECS_HAS_NAMES
    const char *name;
#endif
    ecs_phase_t _phase = EcsOnUpdate;
    ecs_system_id_t _after[ECS_SYSTEM_AFTER_CAPACITY]{};
    bool _disabled = false;

  public:
#if SIECS_HAS_NAMES
    /** Construct a system descriptor with an optional diagnostic name. */
    explicit system(const char *name = "unnamed") : name(name) {}
#else
    explicit system(const char *name = nullptr) { (void)name; }
#endif

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
        this->_phase = _phase;
        return *this;
    }

    /** Add a same-phase dependency; capacity is `ECS_SYSTEM_AFTER_CAPACITY`. */
    system &after(ecs_system_id_t dependency) {
        for (uint16_t i = 0; i < ECS_SYSTEM_AFTER_CAPACITY; i++) {
            if (_after[i] == 0) {
                _after[i] = dependency;
                return *this;
            }
        }
        assert(false && "too many system dependencies");
        return *this;
    }

    /** Set whether the system starts disabled when registered. */
    system &disabled(bool value = true) {
        _disabled = value;
        return *this;
    }

    /** Register the callback and return the owned system id. */
    template <typename F> ecs_system_id_t each(F &&func) {
        using callback = std::remove_cvref_t<F>;
        using args = typename function_traits<callback>::args_tuple;
        detail::append_callback_terms<args>(desc, term_index);
        callback *state = new callback(std::forward<F>(func));

        ecs_system_desc_t system_desc = {
            SIECS_NAME_INIT(name).query = this->desc,
            .callback = detail::system_callback<callback, args>,
            .user_data = reinterpret_cast<uintptr_t>(state),
            .user_data_dtor = detail::system_callback_dtor<callback>,
            .phase = _phase,
            .disabled = _disabled,
        };

        for (uint16_t i = 0; i < ECS_SYSTEM_AFTER_CAPACITY; i++) {
            system_desc.after[i] = _after[i];
        }

        return ecs_system_init(&system_desc);
    }
};

/** Run one enabled system immediately. */
inline void run_system(ecs_system_id_t id) { ecs_run_system(id); }
/** Enable a registered system. */
inline void enable_system(ecs_system_id_t id) { ecs_system_enable(id); }
/** Disable a registered system. */
inline void disable_system(ecs_system_id_t id) { ecs_system_disable(id); }

} // namespace ecs
