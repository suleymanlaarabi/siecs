#pragma once

#include "component.hpp"
#include "function_traits.hpp"
#include "query.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ecs {

struct OnAdd {};
struct OnSet {};
struct OnRemove {};

namespace detail {

template <typename T> struct event_type {
    static inline ecs_event_t id = UINT16_MAX;
};

template <typename T> static ecs_event_t ecs_cpp_event_id(ecs_world_t *world) {
    ecs_event_t &eid = detail::event_type<T>::id;

    if (eid != UINT16_MAX) {
        return eid;
    }

    eid = ecs_event(world);

    return eid;
}

template <typename T> static void ecs_cpp_set_event_id(ecs_event_t eid) {
    detail::event_type<T>::id = eid;
}

template <typename T> decltype(auto) ecs_cpp_observer_arg(ecs_observer_event_t *event) {
    using raw = std::remove_cvref_t<T>;
    void *ptr = ecs_get_cid(event->world, event->entity, ecs_cpp_component_id<raw>(event->world));

    if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
        return *static_cast<const raw *>(ptr);
    } else {
        return *static_cast<raw *>(ptr);
    }
}

template <typename Args, std::size_t I, typename Resources>
decltype(auto) ecs_cpp_observer_arg(ecs_observer_event_t *event, Resources &resources) {
    using arg = std::tuple_element_t<I, Args>;

    if constexpr (is_res_v<arg>) {
        (void)event;
        return std::get<I>(resources);
    } else {
        return ecs_cpp_observer_arg<arg>(event);
    }
}

template <typename Func, typename Args, std::size_t... Is>
void ecs_cpp_observer_callback_impl(ecs_observer_event_t *event, std::index_sequence<Is...>) {
    Func func{};
    auto resources = make_resources<Args>(event->world);
    std::invoke(func, ecs_cpp_observer_arg<Args, Is>(event, resources)...);
}

template <typename Func, typename Tuple>
void ecs_cpp_observer_callback(ecs_observer_event_t *event) {
    ecs_cpp_observer_callback_impl<Func, Tuple>(
        event,
        std::make_index_sequence<std::tuple_size_v<Tuple>>{}
    );
}

} // namespace detail

template <typename T> class observer : public query {

  public:
    observer(ecs_world_t *world) : query(world) {};

    template <typename F> ecs_observer_id_t each(F &&) {
        using callback = std::remove_cvref_t<F>;
        static_assert(
            std::is_empty_v<callback> && std::is_default_constructible_v<callback>,
            "observer callbacks must be stateless with the current C API"
        );

        using traits = function_traits<callback>;
        using args = typename traits::args_tuple;
        static_assert(
            detail::component_arg_count<args>() > 0,
            "observer callbacks must read at least one component"
        );

        ecs::detail::append_callback_terms<args>(_world, this->desc, term_index);

        ecs_observer_desc_t observer_desc = {
            .on = detail::ecs_cpp_event_id<T>(_world),
            .query = this->desc,
            .callback = detail::ecs_cpp_observer_callback<callback, args>,
        };

        return ecs_observer_init(_world, &observer_desc);
    }
};

} // namespace ecs
