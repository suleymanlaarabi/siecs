#pragma once
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

class system : protected query {
    const char *name;
    ecs_phase_t _phase = EcsOnUpdate;

  public:
    explicit system(const char *name = "unnamed") : name(name) {}

    template <typename... T> system &require() {
        query::require<T...>();
        return *this;
    }

    template <typename... T> system &exclude() {
        query::exclude<T...>();
        return *this;
    }

    system &phase(ecs_phase_t _phase) {
        this->_phase = _phase;
        return *this;
    }

    template <typename F> ecs_system_id_t each(F &&func) {
        using callback = std::remove_cvref_t<F>;
        using args = typename function_traits<callback>::args_tuple;
        detail::append_callback_terms<args>(desc, term_index);
        callback *state = new callback(std::forward<F>(func));

        ecs_system_desc_t system_desc = {
            .name = name,
            .query = this->desc,
            .callback = detail::system_callback<callback, args>,
            .user_data = reinterpret_cast<uintptr_t>(state),
            .user_data_dtor = detail::system_callback_dtor<callback>,
            .phase = _phase,
        };

        return ecs_system_init(&system_desc);
    }
};

} // namespace ecs
