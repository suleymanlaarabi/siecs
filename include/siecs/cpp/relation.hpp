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
    const ecs_relation_target_t *targets_ = nullptr;
    ecs_entity_t shared_target_ = 0;

  public:
    using relation_type = Relation;
    target() noexcept = default;
    explicit target(ecs_entity_t value) noexcept : entity(value) {}
    explicit target(entity value) noexcept : entity(value.id()) {}
    static target init() noexcept { return {}; }
    static void query(system_param_query &query) { query.require_relation<Relation>(); }
    void table(ecs_iter_t *it) noexcept {
        if (!it->cache) {
            targets_ = nullptr;
            shared_target_ = ecs_target_id(it->entities[0], detail::ecs_cpp_relation_id<Relation>());
            return;
        }
        const ecs_relation_batch_t batch =
            ecs_relation_batch_id(it, detail::ecs_cpp_relation_id<Relation>());
        targets_ = batch.shared ? nullptr : batch.targets;
        shared_target_ = batch.shared_target;
    }
    void row(ecs_iter_t *, uint32_t row) noexcept {
        static_cast<entity &>(*this) = entity::from(targets_ ? targets_[row].entity : shared_target_);
    }
};

inline entity entity::is_a(entity value) { return relate<ecs::IsA>(value); }
inline entity entity::child_of(entity value) { return relate<ecs::ChildOf>(value); }

} // namespace ecs
