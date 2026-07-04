use core::ffi::{c_char, c_void};

use super::{world::WorldRaw, ModuleId};

pub type ModuleImport = unsafe extern "C" fn(*mut WorldRaw, *const c_void);

#[repr(C)]
pub struct ModuleDesc {
    pub name: *const c_char,
    pub id: *mut ModuleId,
    pub import: Option<ModuleImport>,
    pub desc: *const c_void,
    pub desc_size: u32,
    pub disabled: bool,
}

extern "C" {
    pub fn ecs_module_init(world: *mut WorldRaw, desc: *const ModuleDesc) -> ModuleId;
    pub fn ecs_module_find(world: *mut WorldRaw, id: *const ModuleId) -> ModuleId;
    pub fn ecs_module_enable(world: *mut WorldRaw, module: ModuleId);
    pub fn ecs_module_disable(world: *mut WorldRaw, module: ModuleId);
    pub fn ecs_module_is_enabled(world: *const WorldRaw, module: ModuleId) -> bool;
}
