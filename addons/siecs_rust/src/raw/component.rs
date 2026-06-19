use core::ffi::{c_char, c_void};

use super::{world::WorldRaw, ComponentId, EntityId, SireflectStructDesc};

pub type ComponentOnAdd =
    Option<unsafe extern "C" fn(*mut WorldRaw, EntityId, ComponentId, *mut c_void)>;
pub type ComponentOnSet =
    Option<unsafe extern "C" fn(*mut WorldRaw, EntityId, ComponentId, *const c_void, *mut c_void)>;
pub type ComponentOnRemove =
    Option<unsafe extern "C" fn(*mut WorldRaw, EntityId, ComponentId, *mut c_void)>;

#[repr(C)]
pub struct ComponentDesc {
    pub name: *const c_char,
    pub size: u64,
    pub on_set: ComponentOnSet,
    pub on_remove: ComponentOnRemove,
    pub on_add: ComponentOnAdd,
    pub relation_flags: u32,
    pub struct_desc: *const SireflectStructDesc,
}

extern "C" {
    pub fn ecs_component_init(world: *mut WorldRaw, desc: *const ComponentDesc) -> ComponentId;

    pub fn ecs_add_cid(world: *mut WorldRaw, entity: EntityId, id: ComponentId);
    pub fn ecs_remove_cid(world: *mut WorldRaw, entity: EntityId, id: ComponentId);
    pub fn ecs_has_cid(world: *const WorldRaw, entity: EntityId, id: ComponentId) -> bool;
    pub fn ecs_get_cid(world: *mut WorldRaw, entity: EntityId, id: ComponentId) -> *mut c_void;
    pub fn ecs_try_get_cid(world: *mut WorldRaw, entity: EntityId, id: ComponentId) -> *mut c_void;
    pub fn ecs_set_cid(
        world: *mut WorldRaw,
        entity: EntityId,
        id: ComponentId,
        data: *const c_void,
    );
}
