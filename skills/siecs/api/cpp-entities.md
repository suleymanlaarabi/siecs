# C++20 API: entities

[Usage and contracts](../references/cpp/entities.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C entities reference](c-entities.md). Public base members also apply to derived builders.

## Functions

- `ecs::ensure` — `entity ensure(const char *path)`
- `ecs::ensure` — `entity ensure(entity parent, const char *path)`
- `ecs::singleton` — `template <typename T> entity singleton()`
- `ecs::spawn` — `entity spawn() noexcept`
- `ecs::spawn` — `entity spawn(entity parent)`
- `ecs::spawn_no_reuse` — `inline entity spawn_no_reuse() noexcept`

## Types and members

- `ecs::entity` — `class entity`
- `ecs::entity::disable` — `entity disable()`
- `ecs::entity::enable` — `entity enable()`
- `ecs::entity::entity` — `entity() noexcept = default`
- `ecs::entity::entity` — `entity(ecs_entity_t entity) noexcept`
- `ecs::entity::from` — `static entity from(ecs_entity_t id) noexcept`
- `ecs::entity::id` — `[[nodiscard]] ecs_entity_t id() const noexcept`
- `ecs::entity::init` — `static entity init() noexcept`
- `ecs::entity::is_alive` — `[[nodiscard]] bool is_alive() const`
- `ecs::entity::is_disabled` — `[[nodiscard]] bool is_disabled() const`
- `ecs::entity::is_enabled` — `[[nodiscard]] bool is_enabled() const`
- `ecs::entity::kill` — `void kill()`
- `ecs::entity::lookup` — `static entity lookup(const char *path)`
- `ecs::entity::lookup` — `static entity lookup(entity parent, const char *path)`
- `ecs::entity::lookup` — `static entity lookup(entity parent, const std::string &path)`
- `ecs::entity::lookup` — `static inline entity lookup(const std::string &name)`
- `ecs::entity::name` — `[[nodiscard]] const char *name() const`
- `ecs::entity::null` — `static entity null() noexcept`
- `ecs::entity::operator bool` — `operator bool()`
- `ecs::entity::operator ecs_entity_t` — `operator ecs_entity_t() const noexcept`
- `ecs::entity::path` — `[[nodiscard]] std::string path() const`
- `ecs::entity::row` — `void row(ecs_iter_t *it, uint32_t row) noexcept`
- `ecs::entity::set_name` — `bool set_name(const char *name)`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ecs_entity_generation` — `ecs_entity_generation(entity)`
- `ecs_entity_id` — `ecs_entity_id(entity)`
