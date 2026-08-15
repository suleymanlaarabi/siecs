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

/** Event tag for component additions. */
struct OnAdd {};
/** Event tag for component updates. */
struct OnSet {};
/** Event tag for component removals. */
struct OnRemove {};
/** Event tag for relation additions and retargeting. */
struct OnRelationSet {};
/** Event tag for relation removals. */
struct OnRelationRemove {};

/** Typed, callback-lifetime view of an observer event. */
class observer_event {
    ecs_observer_event_t *_event;

  public:
    /** Wrap a non-null C event payload; the wrapper does not own it. */
    explicit observer_event(ecs_observer_event_t *event) noexcept : _event(event) {}

    /** Return the entity that emitted the event. */
    [[nodiscard]] entity target() const noexcept { return entity::from(_event->entity); }
    /** Return the event id. */
    [[nodiscard]] ecs_event_t id() const noexcept { return _event->event; }
    /** Return opaque user data supplied when the observer was created. */
    [[nodiscard]] uintptr_t user_data() const noexcept { return _event->user_data; }

    /** Interpret user data as a borrowed pointer of type `T`. */
    template <typename T> [[nodiscard]] T *user_data() const noexcept {
        return reinterpret_cast<T *>(_event->user_data);
    }

    /** Interpret trigger payload as a borrowed const pointer of type `T`. */
    template <typename T> [[nodiscard]] const T *trigger_data() const noexcept {
        return static_cast<const T *>(_event->trigger_data);
    }
};

namespace detail {

template <> struct is_observer_event<ecs::observer_event> : std::true_type {};

template <typename T> inline constexpr ecs_event_t builtin_event = UINT16_MAX;
template <> inline constexpr ecs_event_t builtin_event<OnAdd> = EcsOnAdd;
template <> inline constexpr ecs_event_t builtin_event<OnSet> = EcsOnSet;
template <> inline constexpr ecs_event_t builtin_event<OnRemove> = EcsOnRemove;
template <> inline constexpr ecs_event_t builtin_event<OnRelationSet> = EcsOnRelationSet;
template <> inline constexpr ecs_event_t builtin_event<OnRelationRemove> = EcsOnRelationRemove;
template <typename T> static inline ecs_event_t custom_event = UINT16_MAX;

template <typename T> static ecs_event_t ecs_cpp_event_id() {
    if constexpr (builtin_event<T> != UINT16_MAX) return builtin_event<T>;
    if (custom_event<T> == UINT16_MAX) custom_event<T> = ecs_event();
    return custom_event<T>;
}

template <typename Args, std::size_t I, typename Resources>
decltype(auto) ecs_cpp_observer_arg(ecs_observer_event_t *event, Resources &resources) {
    using arg = std::tuple_element_t<I, Args>;
    if constexpr (is_res_v<arg>) {
        (void)event;
        return std::get<I>(resources);
    } else if constexpr (is_observer_event_v<arg>) {
        return observer_event(event);
    } else if constexpr (is_entity_v<arg>) {
        return entity::from(event->entity);
    } else {
        using raw = std::remove_cvref_t<arg>;
        void *ptr = ecs_get_cid(event->entity, ecs_cpp_component_id<raw>());
        if constexpr (std::is_const_v<std::remove_reference_t<arg>>)
            return *static_cast<const raw *>(ptr);
        else return *static_cast<raw *>(ptr);
    }
}

template <typename Func, typename Args>
void ecs_cpp_observer_callback(ecs_observer_event_t *event) {
    Func func{};
    auto resources = make_resources<Args>();
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        std::invoke(func, ecs_cpp_observer_arg<Args, Is>(event, resources)...);
    }(std::make_index_sequence<std::tuple_size_v<Args>>{});
}

} // namespace detail

/** Typed observer builder; callbacks must be stateless and default constructible. */
template <typename T> class observer : public query {
    uintptr_t _user_data = 0;

  public:
    /** Start an empty observer query for event tag `T`. */
    observer() = default;

    /** Set opaque callback user data; the pointer is borrowed, not deleted. */
    observer &user_data(uintptr_t value) {
        _user_data = value;
        return *this;
    }

    /** Store a typed user-data pointer for callback lifetime. */
    template <typename U> observer &user_data(U *value) {
        _user_data = reinterpret_cast<uintptr_t>(value);
        return *this;
    }

    /** Register the callback and return its observer id. */
    template <typename F> ecs_observer_id_t each(F &&) {
        using callback = std::remove_cvref_t<F>;
        static_assert(
            std::is_empty_v<callback> && std::is_default_constructible_v<callback>,
            "observer callbacks must be stateless with the current C API"
        );

        using traits = function_traits<callback>;
        using args = typename traits::args_tuple;
        static_assert(
            detail::component_arg_count<args>() > 0 || std::is_same_v<T, OnRelationSet> ||
                std::is_same_v<T, OnRelationRemove>,
            "observer callbacks must read at least one component or observe relation data"
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
