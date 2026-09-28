#pragma once
#include "siecs/cpp/entity.hpp"

namespace ecs {

struct IsA {};
struct ChildOf {};

namespace detail {
template <> struct c_relation_traits<ecs::IsA> {
    static constexpr bool value = true;
    static constexpr const char *relation_name() noexcept { return "IsA"; }
    static ecs_relation_id_t *id_storage() noexcept { return &::ecs_rid(IsA); }
    static ecs_relation_desc_t *desc_storage() noexcept { return &::ecs_rid(IsA_desc); }
};
template <> struct c_relation_traits<ecs::ChildOf> {
    static constexpr bool value = true;
    static constexpr const char *relation_name() noexcept { return "ChildOf"; }
    static ecs_relation_id_t *id_storage() noexcept { return &::ecs_rid(ChildOf); }
    static ecs_relation_desc_t *desc_storage() noexcept { return &::ecs_rid(ChildOf_desc); }
};
} // namespace detail

template <typename Relation> class target : public entity {
  public:
    using relation_type = Relation;
    target() noexcept = default;
    explicit target(ecs_entity_t value) noexcept : entity(value) {}
    explicit target(entity value) noexcept : entity(value.id()) {}
};

inline entity entity::is_a(entity value) { return relate<ecs::IsA>(value); }
inline entity entity::child_of(entity value) { return relate<ecs::ChildOf>(value); }

} // namespace ecs
