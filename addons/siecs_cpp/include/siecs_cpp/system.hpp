#pragma once

#include "query.hpp"

namespace ecs {

namespace detail {

template <typename Callback, typename Args>
static void system_run(ecs_world_t *world, ecs_query_id_t query, void *) {
    Callback callback{};
    run_query<Callback, Args>(callback, world, query);
}

} // namespace detail

class system : public query {
    const char *name;
    ecs_phase_t _phase = EcsOnUpdate;

  public:
    system(ecs_world_t *_world, const char *name) : query(_world), name(name) {}

    template <typename... T> system require() {
        query::require<T...>();
        return *this;
    }

    template <typename... T> system exclude() {
        query::exclude<T...>();
        return *this;
    }

    system phase(ecs_phase_t _phase) {
        this->_phase = _phase;
        return *this;
    }

    template <typename F> ecs_system_id_t each(F &&) {
        using callback = std::remove_cvref_t<F>;
        static_assert(
            std::is_empty_v<callback> && std::is_default_constructible_v<callback>,
            "system callbacks must be stateless with the current C API"
        );

        using traits = function_traits<callback>;
        using args = typename traits::args_tuple;
        static_assert(
            detail::component_arg_count<args>() > 0,
            "system callbacks must read at least one component"
        );

        detail::append_callback_terms<args>(_world, desc, term_index);

        ecs_system_desc_t system_desc = {
            .name = name,
            .query = this->desc,
            .run = detail::system_run<callback, args>,
            .phase = _phase,
        };

        return ecs_system_init(_world, &system_desc);
    }
};

} // namespace ecs
