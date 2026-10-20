# C++20 API: relations

[Usage and contracts](../references/cpp/relations.md). Signatures only; consult the guide for behavior.

The C ABI remains available. Operations without a typed facade use the
[C relations reference](c-relations.md). Public base members also apply to derived builders.

## Functions

- `ecs::relation` — `template <typename T> inline ecs_relation_id_t relation( const ecs_relation_desc_t &desc = { .storage = EcsRelationDense, .on_delete_target = EcsRemoveRelation, .acyclic = false, } )`

## Types and members

- `ecs::ChildOf` — `struct ChildOf`
- `ecs::IsA` — `struct IsA`
- `ecs::entity::child_of` — `entity child_of(entity parent)`
- `ecs::entity::children` — `template <typename... Entities> entity children(Entities... entities)`
- `ecs::entity::has_relation` — `template <typename Relation> [[nodiscard]] bool has_relation() const`
- `ecs::entity::relate` — `template <typename Relation, typename Target> entity relate()`
- `ecs::entity::relate` — `template <typename Relation> entity relate(entity target)`
- `ecs::entity::target` — `template <typename Relation> [[nodiscard]] entity target() const`
- `ecs::entity::unrelate` — `template <typename Relation> entity unrelate()`
- `ecs::target` — `template <typename Relation> class target : public entity`
- `ecs::target::init` — `static target init() noexcept`
- `ecs::target::query` — `static void query(system_param_query &query)`
- `ecs::target::relation_type` — `using relation_type = Relation`
- `ecs::target::row` — `void row(ecs_iter_t *, uint32_t row) noexcept`
- `ecs::target::table` — `void table(ecs_iter_t *it) noexcept`
- `ecs::target::target<Relation>` — `explicit target(ecs_entity_t value) noexcept`
- `ecs::target::target<Relation>` — `explicit target(entity value) noexcept`
- `ecs::target::target<Relation>` — `target() noexcept = default`

## Macros

Function-like macros show invocation shape, not their implementation.

- `ECS_RELATION` — `ECS_RELATION(name,...)`
- `ECS_RELATION_DECLARE` — `ECS_RELATION_DECLARE(name)`
- `ECS_RELATION_DEFINE` — `ECS_RELATION_DEFINE(name,...)`
- `ECS_RELATION_REGISTER` — `ECS_RELATION_REGISTER(...)`
- `ECS_RELATION_REGISTER_ONE` — `ECS_RELATION_REGISTER_ONE(cname)`
- `ecs_has_relation` — `ecs_has_relation(entity,relation)`
- `ecs_has_relation_to` — `ecs_has_relation_to(entity,relation,target)`
- `ecs_relate` — `ecs_relate(entity,relation,target)`
- `ecs_relation_batch` — `ecs_relation_batch(it,relation)`
- `ecs_target` — `ecs_target(entity,relation)`
- `ecs_target_shared` — `ecs_target_shared(it,relation)`
- `ecs_targets` — `ecs_targets(it,relation)`
- `ecs_unrelate` — `ecs_unrelate(entity,relation)`
- `ecs_with_relation` — `ecs_with_relation(component,relation,target)`
