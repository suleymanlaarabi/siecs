#pragma once

#include "query.hpp"

namespace ecs {

namespace detail {

template <typename Callback, typename Args> static void system_callback(ecs_iter_t *it) {
    constexpr std::size_t arg_count = std::tuple_size_v<Args>;
    auto fields = make_fields<Args>(it, std::make_index_sequence<arg_count>{});

    call_fields(Callback{}, fields, it->count);
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
            std::tuple_size_v<args> > 0,
            "system callbacks must read at least one component"
        );

        detail::append_callback_terms<args>(_world, desc, term_index);

        ecs_system_desc_t system_desc = {
            .name = name,
            .query = this->desc,
            .callback = detail::system_callback<callback, args>,
            .phase = _phase,
        };

        return ecs_system_init(_world, &system_desc);
    }
};

} // namespace ecs
