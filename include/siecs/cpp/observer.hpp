#pragma once
#include "siecs/cpp/component.hpp"
#include "siecs/cpp/function_traits.hpp"
#include "siecs/cpp/query.hpp"
#include <array>
#include <concepts>
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
    ecs_observer_event_t *_event = nullptr;

  public:
    observer_event() noexcept = default;
    /** Wrap a non-null C event payload; the wrapper does not own it. */
    explicit observer_event(ecs_observer_event_t *event) noexcept : _event(event) {}
    static observer_event init() noexcept { return {}; }
    void event(ecs_observer_event_t *value) noexcept { _event = value; }

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

template <typename Callback> struct observer_state {
    Callback callback;
    uintptr_t user_data;
    std::array<ecs_component_term_t, ECS_QUERY_TERM_CAPACITY> fields{};
    uint16_t field_count = 0;
};
template <typename Callback> void destroy_observer_state(uintptr_t data) {
    delete reinterpret_cast<observer_state<Callback> *>(data);
}

template <typename T> inline constexpr ecs_event_t builtin_event = UINT16_MAX;
template <> inline constexpr ecs_event_t builtin_event<OnAdd> = EcsOnAdd;
template <> inline constexpr ecs_event_t builtin_event<OnSet> = EcsOnSet;
template <> inline constexpr ecs_event_t builtin_event<OnRemove> = EcsOnRemove;
template <> inline constexpr ecs_event_t builtin_event<OnRelationSet> = EcsOnRelationSet;
template <> inline constexpr ecs_event_t builtin_event<OnRelationRemove> = EcsOnRelationRemove;

/**
 * External event tags can provide their runtime event id with a static
 * event_id() member. This lets addons expose typed C++ tags for events that
 * they register themselves.
 */
template <typename T>
concept event_id_provider = requires {
    { T::event_id() } -> std::convertible_to<ecs_event_t>;
};

template <typename T> static ecs_event_t ecs_cpp_event_id() {
    if constexpr (builtin_event<T> != UINT16_MAX)
        return builtin_event<T>;
    if constexpr (event_id_provider<T>)
        return T::event_id();
    static ecs_event_t &id = shared_id<T>(id_kind::event, UINT16_MAX);
    if (id == UINT16_MAX)
        id = ecs_event();
    return id;
}

template <typename T>
concept system_param_observer =
    SystemParam<T> &&
    requires(system_param_t<T> &value, ecs_observer_event_t *event) { value.event(event); };

template <typename Func, typename Args>
void ecs_cpp_observer_callback(ecs_observer_event_t *event) {
    auto &state = *reinterpret_cast<observer_state<Func> *>(event->user_data);
    Func &func = state.callback;
    ecs_observer_event_t context = *event;
    context.user_data = state.user_data;
    event = &context;
    constexpr auto indices = std::make_index_sequence<std::tuple_size_v<Args>>{};
    auto params = init_callback_params<Args>();
    ecs_entity_t entity_id = event->entity;
    ecs_iter_t it{};
    it.count = 1;
    it.entities = &entity_id;
    std::array<void *, ECS_QUERY_TERM_CAPACITY> fields{};
    it.ptrs = fields.data();
    for (uint16_t i = 0; i < state.field_count; ++i) {
        const auto term = state.fields[i];
        const auto relation = static_cast<ecs_relation_id_t>(term.access >> 8);
        ecs_entity_t source = relation ? ecs_target_id(entity_id, relation) : entity_id;
        while (source) {
            fields[i] = ecs_try_get_cid(source, term.id);
            if (fields[i] || !relation)
                break;
            source = ecs_target_id(source, relation);
        }
        const auto access = static_cast<ecs_access_t>(term.access & 0xff);
        if (!fields[i] && access != EcsInOptional && access != EcsInOutOptional &&
            access != EcsInUpOptional)
            return;
    }
    std::apply([&](auto &...param) { (prepare_table(param, &it), ...); }, params);
    std::apply([&](auto &...param) { (prepare_row(param, &it, 0), ...); }, params);
    std::apply(
        [&](auto &...param) {
            (([&] {
                 if constexpr (system_param_observer<decltype(param)>)
                     param.event(event);
             }()),
             ...);
        },
        params
    );
    bool has_shared = false;
    auto cursors = make_cursors<Args>(&it, has_shared, indices);
    invoke_row<Args>(func, params, cursors, 0, indices);
}

} // namespace detail

/** Non-owning observer registration, controlled explicitly or by its owner/world. */
class observer_ref {
    ecs_observer_id_t _id;

  public:
    explicit observer_ref(ecs_observer_id_t id) noexcept : _id(id) {}
    void enable() const { ecs_observer_enable(_id); }
    void disable() const { ecs_observer_disable(_id); }
    void remove() const { ecs_observer_fini(_id); }
    [[nodiscard]] ecs_observer_id_t id() const noexcept { return _id; }
};

/** Read-only, synchronous view of a custom event payload and emitting entity. */
template <typename T> class event_view : public observer_event {
    const T *_payload = nullptr;

  public:
    using event_type = T;
    static event_view init() noexcept { return {}; }
    void event(ecs_observer_event_t *value) noexcept {
        assert(value->event == detail::ecs_cpp_event_id<T>() && value->trigger_data);
        observer_event::event(value);
        _payload = static_cast<const T *>(value->trigger_data);
    }
    [[nodiscard]] const T *operator->() const noexcept { return _payload; }
    [[nodiscard]] const T &operator*() const noexcept { return *_payload; }
};
template <typename T> using event = event_view<T>;

/** Typed observer builder; the world owns the callback including its captures. */
template <typename T> class observer : public query_filter<observer<T>> {
    using base = query_filter<observer<T>>;
    using base::component_index;
    using base::desc;
    using base::relation_index;
    using base::resource_index;
    uintptr_t _user_data = 0;
    ecs_entity_t _target = 0;

  public:
    /** Borrow optional user data; captures are preferable for native C++ callbacks. */
    template <typename U> observer &user_data(U *value) {
        _user_data = reinterpret_cast<uintptr_t>(value);
        return *this;
    }
    observer &target(ecs::entity value) noexcept {
        _target = value.id();
        return *this;
    }

    /** Register the callback and return its observer id. */
    template <typename F> observer_ref each(F &&func) {
        using callback = std::remove_cvref_t<F>;
        using traits = function_traits<callback>;
        using args = typename traits::args_tuple;
        for_each_type<args>([]<typename Arg>() {
            if constexpr (requires { typename system_param_t<Arg>::event_type; }) {
                static_assert(
                    std::is_same_v<T, typename system_param_t<Arg>::event_type>,
                    "Observer event payload must match its event type"
                );
                static_assert(
                    detail::builtin_event<T> == UINT16_MAX,
                    "Use observer_event for builtin lifecycle context"
                );
            }
        });
        static_assert(
            detail::field_index_before<args, std::tuple_size_v<args>>() > 0 ||
                detail::builtin_event<T> == UINT16_MAX || std::is_same_v<T, OnRelationSet> ||
                std::is_same_v<T, OnRelationRemove>,
            "lifecycle observers must read at least one component"
        );

        ecs::detail::append_callback_terms<args>(
            this->desc,
            component_index,
            resource_index,
            relation_index
        );

        auto state = std::make_unique<detail::observer_state<callback>>(
            detail::observer_state<callback>{ std::forward<F>(func), _user_data }
        );
        for (uint16_t i = 0; i < component_index; ++i) {
            auto term = desc.components[i];
            auto access = static_cast<ecs_access_t>(term.access & 0xff);
            if (access != EcsFilter && access != EcsNot)
                state->fields[state->field_count++] = term;
        }
        ecs_observer_desc_t observer_desc = {
            .on = detail::ecs_cpp_event_id<T>(),
            .entity = _target,
            .query = this->desc,
            .callback = detail::ecs_cpp_observer_callback<callback, args>,
            .user_data = reinterpret_cast<uintptr_t>(state.get()),
            .user_data_dtor = detail::destroy_observer_state<callback>,
        };

        auto id = ecs_observer_init(&observer_desc);
        state.release();
        return observer_ref(id);
    }
};

template <typename Event, typename F> observer_ref entity::observe(F &&callback) {
    ecs::observer<Event> value;
    value.target(*this);
    return value.each(std::forward<F>(callback));
}

} // namespace ecs
