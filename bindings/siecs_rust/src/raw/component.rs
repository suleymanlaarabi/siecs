pub use super::generated::{
    ecs_add_cid, ecs_component_init, ecs_component_register, ecs_get_cid, ecs_has_cid,
    ecs_has_cid_owned, ecs_move_cid, ecs_remove_cid, ecs_set_cid, ecs_try_get_cid, ecs_with,
};

pub type ComponentOnAdd = super::generated::ecs_component_on_add_t;
pub type ComponentOnSet = super::generated::ecs_component_on_set_t;
pub type ComponentOnRemove = super::generated::ecs_component_on_remove_t;
pub type TypeCtor = super::generated::ecs_type_ctor_t;
pub type TypeDtor = super::generated::ecs_type_dtor_t;
pub type TypeCopy = super::generated::ecs_type_copy_t;
pub type TypeMove = super::generated::ecs_type_move_t;
pub type TypeOps = super::generated::ecs_type_ops_t;
pub type ComponentDesc = super::generated::ecs_component_desc_t;

pub const ECS_RELATION_TARGET: u32 = super::generated::ecs_relation_flags_t_EcsRelationTarget;
pub const ECS_RELATION_SOURCE: u32 = super::generated::ecs_relation_flags_t_EcsRelationSource;
pub const ECS_RELATION_CASCADE_DELETE: u32 =
    super::generated::ecs_relation_flags_t_EcsRelationCascadeDelete;
pub const ECS_RELATION_ONE_TO_ONE: u32 = super::generated::ecs_relation_flags_t_EcsRelationOneToOne;
pub const ECS_RELATION_ONE_TO_MANY: u32 =
    super::generated::ecs_relation_flags_t_EcsRelationOneToMany;

#[allow(clippy::derivable_impls)]
impl Default for TypeOps {
    fn default() -> Self {
        Self {
            ctor: None,
            dtor: None,
            copy_ctor: None,
            copy: None,
            move_ctor: None,
            move_: None,
        }
    }
}
