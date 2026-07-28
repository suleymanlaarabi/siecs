#pragma once
#include "siecs/cpp/component.hpp"
#include "siecs/cpp/function_traits.hpp"
#include "siecs/cpp/query.hpp"
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

class observer_event {
    ecs_observer_event_t *_event;

  public:
    explicit observer_event(ecs_observer_event_t *event) noexcept : _event(event) {}

    [[nodiscard]] entity target() const noexcept { return entity::from(_event->entity); }
    [[nodiscard]] ecs_event_t id() const noexcept { return _event->event; }
    [[nodiscard]] uintptr_t user_data() const noexcept { return _event->user_data; }

    template <typename T> [[nodiscard]] T *user_data() const noexcept {
        return reinterpret_cast<T *>(_event->user_data);
    }

    template <typename T> [[nodiscard]] const T *trigger_data() const noexcept {
        return static_cast<const T *>(_event->trigger_data);
    }
};

namespace detail {

template <> struct is_observer_event<ecs::observer_event> : std::true_type {};

template <typename T> struct event_type {
    static inline ecs_event_t id = UINT16_MAX;
};

template <typename T> static ecs_event_t ecs_cpp_event_id() {
    ecs_event_t &eid = detail::event_type<T>::id;

    if constexpr (std::is_same_v<T, OnAdd>) {
        eid = EcsOnAdd;
    } else if constexpr (std::is_same_v<T, OnSet>) {
        eid = EcsOnSet;
    } else if constexpr (std::is_same_v<T, OnRemove>) {
        eid = EcsOnRemove;
    } else {
        if (eid == UINT16_MAX) {
            eid = ecs_event();
        }
    }

    return eid;
}

template <typename T> decltype(auto) ecs_cpp_observer_arg(ecs_observer_event_t *event) {
    using raw = std::remove_cvref_t<T>;
    if constexpr (is_observer_event_v<T>) {
        return observer_event(event);
    } else if constexpr (is_entity_v<T>) {
        return entity::from(event->entity);
    } else {
        void *ptr = ecs_get_cid(event->entity, ecs_cpp_component_id<raw>());

        if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
            return *static_cast<const raw *>(ptr);
        } else {
            return *static_cast<raw *>(ptr);
        }
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
    auto resources = make_resources<Args>();
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
    uintptr_t _user_data = 0;

  public:
    observer() = default;

    observer &user_data(uintptr_t value) {
        _user_data = value;
        return *this;
    }

    template <typename U> observer &user_data(U *value) {
        _user_data = reinterpret_cast<uintptr_t>(value);
        return *this;
    }

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

        ecs::detail::append_callback_terms<args>(this->desc, term_index);

        ecs_observer_desc_t observer_desc = {
            .on = detail::ecs_cpp_event_id<T>(),
            .query = this->desc,
            .callback = detail::ecs_cpp_observer_callback<callback, args>,
            .user_data = _user_data,
        };

        return ecs_observer_init(&observer_desc);
    }
};

} // namespace ecs
