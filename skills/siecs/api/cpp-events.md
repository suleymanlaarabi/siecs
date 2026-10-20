# C++20 API: events

[Usage and contracts](../references/cpp/events.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C events reference](c-events.md). Public base members also apply to derived builders.

Inherited selection methods: [complete filter table](../references/cpp/filters.md).
Payload, resource and custom context binding: [parameters](../references/cpp/parameters.md).

## Functions

- `ecs::emit` — `template <typename T> inline void emit(entity value, const T &payload)`
- `ecs::event_id` — `template <typename T> [[nodiscard]] ecs_event_t event_id()`
- `ecs::observe` — `template <typename T> [[nodiscard]] observer<T> observe()`

## Types and members

- `ecs::OnAdd` — `struct OnAdd`
- `ecs::OnRelationRemove` — `struct OnRelationRemove`
- `ecs::OnRelationSet` — `struct OnRelationSet`
- `ecs::OnRemove` — `struct OnRemove`
- `ecs::OnSet` — `struct OnSet`
- `ecs::entity::observe` — `template <typename Event, typename F> observer_ref observe(F &&callback)`
- `ecs::event` — `template <typename T> using event = event_view<T>`
- `ecs::event_view` — `template <typename T> class event_view : public observer_event`
- `ecs::event_view::event` — `void event(ecs_observer_event_t *value) noexcept`
- `ecs::event_view::event_type` — `using event_type = T`
- `ecs::event_view::init` — `static event_view init() noexcept`
- `ecs::event_view::operator*` — `[[nodiscard]] const T &operator*() const noexcept`
- `ecs::event_view::operator->` — `[[nodiscard]] const T *operator->() const noexcept`
- `ecs::observer` — `template <typename T> class observer : public query_filter<observer<T>>`
- `ecs::observer::each` — `template <typename F> observer_ref each(F &&func)`
- `ecs::observer::target` — `observer &target(ecs::entity value) noexcept`
- `ecs::observer::user_data` — `template <typename U> observer &user_data(U *value)`
- `ecs::observer_event` — `class observer_event`
- `ecs::observer_event::event` — `void event(ecs_observer_event_t *value) noexcept`
- `ecs::observer_event::id` — `[[nodiscard]] ecs_event_t id() const noexcept`
- `ecs::observer_event::init` — `static observer_event init() noexcept`
- `ecs::observer_event::observer_event` — `explicit observer_event(ecs_observer_event_t *event) noexcept`
- `ecs::observer_event::observer_event` — `observer_event() noexcept = default`
- `ecs::observer_event::target` — `[[nodiscard]] entity target() const noexcept`
- `ecs::observer_event::trigger_data` — `template <typename T> [[nodiscard]] const T *trigger_data() const noexcept`
- `ecs::observer_event::user_data` — `[[nodiscard]] uintptr_t user_data() const noexcept`
- `ecs::observer_event::user_data` — `template <typename T> [[nodiscard]] T *user_data() const noexcept`
- `ecs::observer_ref` — `class observer_ref`
- `ecs::observer_ref::disable` — `void disable() const`
- `ecs::observer_ref::enable` — `void enable() const`
- `ecs::observer_ref::id` — `[[nodiscard]] ecs_observer_id_t id() const noexcept`
- `ecs::observer_ref::observer_ref` — `explicit observer_ref(ecs_observer_id_t id) noexcept`
- `ecs::observer_ref::remove` — `void remove() const`

## Macros

Function-like macros show invocation shape, not their implementation.

- `EcsOnAdd` — `EcsOnAdd 0`
- `EcsOnRelationRemove` — `EcsOnRelationRemove 4`
- `EcsOnRelationSet` — `EcsOnRelationSet 3`
- `EcsOnRemove` — `EcsOnRemove 1`
- `EcsOnSet` — `EcsOnSet 2`
- `ecs_observer` — `ecs_observer(...)`
