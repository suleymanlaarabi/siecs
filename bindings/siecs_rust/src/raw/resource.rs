pub type ResourceHook = super::generated::ecs_resource_hook_t;
pub type ResourceDesc = super::generated::ecs_resource_desc_t;

pub use super::generated::{
    ecs_has_resource_rid, ecs_move_resource_rid, ecs_remove_resource_rid, ecs_resource_find,
    ecs_resource_init, ecs_resource_is_registered_rid, ecs_resource_register, ecs_resource_rid,
    ecs_set_resource_rid, ecs_try_resource_rid,
};
