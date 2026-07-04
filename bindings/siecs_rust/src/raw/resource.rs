use core::ffi::{c_char, c_void};

use super::{component::TypeOps, world::WorldRaw, ResourceId};

pub type ResourceHook = Option<unsafe extern "C" fn(*mut WorldRaw, *const c_void)>;

#[repr(C)]
pub struct ResourceDesc {
    pub name: *const c_char,
    pub size: u64,
    pub ops: TypeOps,
    pub on_set: ResourceHook,
    pub on_remove: ResourceHook,
}

extern "C" {
    pub fn ecs_resource_init(world: *mut WorldRaw, desc: *const ResourceDesc) -> ResourceId;
    pub fn ecs_resource_register(
        world: *mut WorldRaw,
        id: *mut ResourceId,
        desc: *const ResourceDesc,
    ) -> ResourceId;
    pub fn ecs_resource_find(world: *mut WorldRaw, name: *const c_char) -> ResourceId;
    pub fn ecs_resource_is_registered_rid(world: *const WorldRaw, id: ResourceId) -> bool;
    pub fn ecs_set_resource_rid(world: *mut WorldRaw, id: ResourceId, data: *const c_void);
    pub fn ecs_move_resource_rid(world: *mut WorldRaw, id: ResourceId, data: *mut c_void);
    pub fn ecs_resource_rid(world: *mut WorldRaw, id: ResourceId) -> *mut c_void;
    pub fn ecs_try_resource_rid(world: *mut WorldRaw, id: ResourceId) -> *mut c_void;
    pub fn ecs_has_resource_rid(world: *const WorldRaw, id: ResourceId) -> bool;
    pub fn ecs_remove_resource_rid(world: *mut WorldRaw, id: ResourceId);
}
